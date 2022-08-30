//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012, 2013 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================


#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/protocol/Quality.h>
#include <ripple/core/DatabaseCon.h>
#include <ripple/app/main/Application.h>
#include <peersafe/schema/Schema.h>
#include <peersafe/schema/SchemaManager.h>
#include <ripple/app/ledger/AcceptedLedger.h>
#include <ripple/app/ledger/InboundLedger.h>
#include <ripple/app/ledger/InboundLedgers.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/consensus/RCLConsensus.h>
#include <ripple/app/consensus/RCLValidations.h>
#include <ripple/app/ledger/LedgerToJson.h>
#include <ripple/app/ledger/LocalTxs.h>
#include <ripple/app/ledger/OpenLedger.h>
#include <ripple/app/ledger/OrderBookDB.h>
#include <ripple/app/ledger/TransactionMaster.h>
#include <ripple/app/ledger/PendingSaves.h>
#include <ripple/app/main/LoadManager.h>
#include <ripple/app/misc/AmendmentTable.h>
#include <ripple/app/misc/HashRouter.h>
#include <ripple/app/misc/LoadFeeTrack.h>
#include <ripple/app/misc/Transaction.h>
#include <ripple/app/misc/TxQ.h>
#include <ripple/app/misc/ValidatorKeys.h>
#include <ripple/app/misc/ValidatorList.h>
#include <ripple/app/misc/impl/AccountTxPaging.h>
#include <ripple/app/tx/apply.h>
#include <ripple/basics/contract.h>
#include <ripple/basics/PerfLog.h>
#include <ripple/basics/UptimeClock.h>
#include <ripple/basics/base64.h>
#include <ripple/basics/mulDiv.h>
#include <ripple/basics/safe_cast.h>
#include <ripple/beast/core/LexicalCast.h>
#include <ripple/beast/rfc2616.h>
#include <ripple/beast/utility/rngfill.h>
#include <ripple/core/ConfigSections.h>
#include <ripple/crypto/RFC1751.h>
#include <ripple/crypto/csprng.h>
#include <ripple/json/to_string.h>
#include <ripple/overlay/Cluster.h>
#include <ripple/overlay/predicates.h>
#include <ripple/overlay/Overlay.h>
#include <ripple/protocol/BuildInfo.h>
#include <ripple/protocol/Feature.h>
#include <ripple/resource/ResourceManager.h>
#include <ripple/rpc/DeliveredAmount.h>
#include <boost/asio/ip/host_name.hpp>
#include <boost/asio/steady_timer.hpp>
#include <peersafe/rpc/impl/TableAssistant.h>
#include <peersafe/rpc/TableUtils.h>
#include <peersafe/app/misc/TxPool.h>
#include <ripple/basics/base64.h>
#include <ripple/app/tx/impl/Transactor.h>
#include <peersafe/app/misc/StateManager.h>
#include <peersafe/schema/PeerManager.h>
// #include <peersafe/app/consensus/ViewChange.h>
#include <peersafe/app/tx/impl/Tuning.h>
#include <peersafe/app/sql/TxnDBConn.h>
#include <peersafe/app/prometh/PrometheusClient.h>
#include <peersafe/app/util/NetworkUtil.h>
#include <boost/asio/ip/host_name.hpp>
#include <string>
#include <tuple>
#include <utility>
#include <mutex>
#include <string>
#include <tuple>
#include <utility>

namespace ripple {

bool getRawMetaHex(Ledger const& ledger,
    uint256 const& transID, std::string& rawHex, std::string& metaHex);

class NetworkOPsImp final : public NetworkOPs
{
    /**
     * Transaction with input flags and results to be applied in batches.
     */

    class TransactionStatus
    {
    public:
        std::shared_ptr<Transaction> transaction;
        bool admin;
        bool local;
        FailHard failType;
        bool applied;
        STer result;

        TransactionStatus(
            std::shared_ptr<Transaction> t,
            bool a,
            bool l,
            FailHard f)
            : transaction(t), admin(a), local(l), failType(f)
        {
            assert(local || failType == FailHard::no);
        }
    };

    /**
     * Synchronization states for transaction batches.
     */
    enum class DispatchState : unsigned char {
        none,
        scheduled,
        running,
    };

    static std::array<char const*, 5> const states_;

    /**
     * State accounting records two attributes for each possible server state:
     * 1) Amount of time spent in each state (in microseconds). This value is
     *    updated upon each state transition.
     * 2) Number of transitions to each state.
     *
     * This data can be polled through server_info and represented by
     * monitoring systems similarly to how bandwidth, CPU, and other
     * counter-based metrics are managed.
     *
     * State accounting is more accurate than periodic sampling of server
     * state. With periodic sampling, it is very likely that state transitions
     * are missed, and accuracy of time spent in each state is very rough.
     */
    class StateAccounting
    {
        struct Counters
        {
            explicit Counters() = default;

            std::uint32_t transitions = 0;
            std::chrono::microseconds dur = std::chrono::microseconds(0);
        };

        OperatingMode mode_ = OperatingMode::DISCONNECTED;
        std::array<Counters, 5> counters_;
        mutable std::mutex mutex_;
        std::chrono::system_clock::time_point start_ =
            std::chrono::system_clock::now();
        static std::array<Json::StaticString const, 5> const states_;
        static Json::StaticString const transitions_;
        static Json::StaticString const dur_;

    public:
        explicit StateAccounting()
        {
            counters_[static_cast<std::size_t>(OperatingMode::DISCONNECTED)]
                .transitions = 1;
        }

        /**
         * Record state transition. Update duration spent in previous
         * state.
         *
         * @param om New state.
         */
        void
        mode(OperatingMode om);

        /**
         * Json-formatted state accounting data.
         * 1st member: state accounting object.
         * 2nd member: duration in current state.
         */
        using StateCountersJson = std::pair<Json::Value, std::string>;

        /**
         * Output state counters in JSON format.
         *
         * @return JSON object.
         */
        StateCountersJson
        json() const;

        struct CounterData
        {
            decltype(counters_) counters;
            decltype(mode_) mode;
            decltype(start_) start;
        };

        CounterData
        getCounterData() const
        {
            std::lock_guard lock(mutex_);
            return {counters_, mode_, start_};
        }
    };

    //! Server fees published on `server` subscription
    struct ServerFeeSummary
    {
        ServerFeeSummary() = default;

        ServerFeeSummary(
            ZXCAmount fee,
            TxQ::Metrics&& escalationMetrics,
            LoadFeeTrack const& loadFeeTrack);
        bool
        operator!=(ServerFeeSummary const& b) const;

        bool
        operator==(ServerFeeSummary const& b) const
        {
            return !(*this != b);
        }

        std::uint32_t loadFactorServer = 256;
        std::uint32_t loadBaseServer = 256;
        ZXCAmount baseFee{10};
        boost::optional<TxQ::Metrics> em = boost::none;
    };

    struct AccountDelayEntry {
        int failureCount = 0;
        clock_type::time_point timeExpires;
        clock_type::time_point lastTouch;
    };
public:
    NetworkOPsImp(
        Schema& app,
        NetworkOPs::clock_type& clock,
        bool standalone,
        std::size_t minPeerCount,
        bool start_valid,
        JobQueue& job_queue,
        LedgerMaster& ledgerMaster,
        Stoppable& parent,
        ValidatorKeys const& validatorKeys,
        boost::asio::io_service& io_svc,
        beast::Journal journal,
        beast::insight::Collector::ptr const& collector)
        : NetworkOPs(parent)
        , app_(app)
        , m_clock(clock)
        , m_journal(journal)
        , m_localTX(make_LocalTxs())
        , mMode(OperatingMode::FULL)
        // , mMode(start_valid ? OperatingMode::FULL :
        // OperatingMode::DISCONNECTED)
        , heartbeatTimer_(io_svc)
        , clusterTimer_(io_svc)
        , mConsensus(
              app,
              make_FeeVote(
                  setup_FeeVote(app_.config()),
                  app_.journal("FeeVote")),
              ledgerMaster,
              *m_localTX,
              app.getInboundTransactions(),
              beast::get_abstract_clock<std::chrono::steady_clock>(),
              validatorKeys,
              app_.journal("LedgerConsensus"))
        , m_ledgerMaster(ledgerMaster)
        , m_job_queue(job_queue)
        , m_standalone(standalone)
        , minPeerCount_(0/*start_valid ? 0 : minPeerCount*/)
        , m_stats(std::bind(&NetworkOPsImp::collect_metrics, this), collector)
    {
        auto& cfg = app_.config();
        auto sync_section = cfg.section(ConfigSection::autoSync());
        if (sync_section.values().size() > 0)
        {
            auto value = sync_section.values().at(0);
            m_bAutoSync = atoi(value.c_str());
        }
    }

    ~NetworkOPsImp() override
    {
        // This clear() is necessary to ensure the shared_ptrs in this map get
        // destroyed NOW because the objects in this map invoke methods on this
        // class when they are destroyed
        mRpcSubMap.clear();
    }

public:
    OperatingMode
    getOperatingMode() const override
    {
        return mMode;
    }

    std::string
    strOperatingMode(OperatingMode const mode, bool const admin) const override;

    std::string
    strOperatingMode(bool const admin = false) const override
    {
        return strOperatingMode(mMode, admin);
    }

    //
    // Transaction operations.
    //

    // Must complete immediately.
    void
    submitTransaction(std::shared_ptr<STTx const> const&) override;

    void
    processTransaction(
        std::shared_ptr<Transaction>& transaction,
        bool bUnlimited,
        bool bLocal,
        FailHard failType) override;

    /**
     * For transactions submitted directly by a client, apply batch of
     * transactions and wait for this transaction to complete.
     *
     * @param transaction Transaction object.
     * @param bUnliimited Whether a privileged client connection submitted it.
     * @param failType fail_hard setting from transaction submission.
     */
    void
    doTransactionSync(
        std::shared_ptr<Transaction> transaction,
        bool bUnlimited,
        FailHard failType);

    /**
     * For transactions not submitted by a locally connected client, fire and
     * forget. Add to batch and trigger it to be processed if there's no batch
     * currently being applied.
     *
     * @param transaction Transaction object
     * @param bUnlimited Whether a privileged client connection submitted it.
     * @param failType fail_hard setting from transaction submission.
     */
    void
    doTransactionAsync(
        std::shared_ptr<Transaction> transaction,
        bool bUnlimited,
        FailHard failtype);

    std::pair<STer, bool>
    doTransactionCheck(
        std::shared_ptr<Transaction> transaction,
        ApplyFlags flags,
        OpenView const& view);

    STer
    check(PreflightContext const& pfctx, OpenView const& view);

    STer
    checkForAccountDelay(PreflightContext const& pfctx);
    /**
     * Apply transactions in batches. Continue until none are queued.
     */
    void
    transactionBatch();

    /**
     * Attempt to apply transactions and post-process based on the results.
     *
     * @param Lock that protects the transaction batching
     */
    void
    apply(std::unique_lock<std::mutex>& batchLock);

    //
    // Owner functions.
    //

    Json::Value
    getOwnerInfo(
        std::shared_ptr<ReadView const> lpLedger,
        AccountID const& account) override;

    //
    // Book functions.
    //

    void
    getBookPage(
        std::shared_ptr<ReadView const>& lpLedger,
        Book const&,
        AccountID const& uTakerID,
        const bool bProof,
        unsigned int iLimit,
        Json::Value const& jvMarker,
        Json::Value& jvResult) override;

    void
    mapComplete(std::shared_ptr<SHAMap> const& map, bool fromAcquire) override;

    // Network state machine.

    // Used for the "jump" case.
private:
    void
    switchLastClosedLedger(std::shared_ptr<Ledger const> const& newLCL);
    bool
    checkLastClosedLedger(const Overlay::PeerSequence&, uint256& networkClosed);

public:
    void
    setGenesisLedgerIndex(LedgerIndex seq) override;
    void
    onDeleteUntrusted(hash_set<NodeID> const& removed) override;
    bool
    beginConsensus(uint256 const& networkClosed) override;
    void
    endConsensus() override;
    void
    peerConsensusMessage(
        std::shared_ptr<PeerImp> peer,
        bool isTrusted,
        std::shared_ptr<protocol::TMConsensus> const& m) override;

    void
    peerSyncSchema(
        std::shared_ptr<PeerImp> peer,
        std::shared_ptr<protocol::TMSyncSchema> const& m) override;

    void
    setStandAlone() override
    {
        setMode(OperatingMode::FULL);
    }

    /** Called to initially start our timers.
        Not called for stand-alone mode.
    */
    void
    setStateTimer() override;

    void
    setNeedNetworkLedger() override
    {
        needNetworkLedger_ = true;
    }
    void
    clearNeedNetworkLedger() override
    {
        needNetworkLedger_ = false;
    }
    bool
    isNeedNetworkLedger() override
    {
        return needNetworkLedger_;
    }
    bool
    isFull() override
    {
        return !needNetworkLedger_ && (mMode == OperatingMode::FULL);
    }

    void
    setMode(OperatingMode om) override;

    bool
    isAmendmentBlocked() override
    {
        return amendmentBlocked_;
    }

    void
    setAmendmentBlocked() override;
    bool
    isAmendmentWarned() override
    {
        return !amendmentBlocked_ && amendmentWarned_;
    }
    void
    setAmendmentWarned() override
    {
        amendmentWarned_ = true;
    }
    void
    clearAmendmentWarned() override
    {
        amendmentWarned_ = false;
    }
    void
    consensusViewChange() override;

    ConsensusParms const&
    getConsensusParms() override;
    std::recursive_mutex&
    peekConsensusMutex() override;
    Json::Value
    getConsensusInfo(bool full = true) override;
    Json::Value
    getServerInfo(bool human, bool admin, bool counters) override;
    std::string
    getServerStatus() override;
    void
    clearLedgerFetch() override;
    Json::Value
    getLedgerFetchInfo() override;
    bool
    checkLedgerAccept(std::shared_ptr<Ledger const> const& ledger) override;
    std::uint32_t
    acceptLedger(
        boost::optional<std::chrono::milliseconds> consensusDelay) override;
    void
    reportFeeChange() override;
    void
    reportConsensusStateChange(ConsensusPhase phase);

    void
    updateLocalTx(ReadView const& view) override
    {
        m_localTX->sweep(view);
    }
    std::size_t
    getLocalTxCount() override
    {
        return m_localTX->size();
    }

    // Helper function to generate SQL query to get transactions.
    std::string
    transactionsSQL(
        std::string selection,
        AccountID const& account,
        std::int32_t minLedger,
        std::int32_t maxLedger,
        bool descending,
        std::uint32_t offset,
        int limit,
        bool binary,
        bool count,
        bool bUnlimited);

    // Client information retrieval functions.
    using NetworkOPs::AccountTxMarker;
    using NetworkOPs::AccountTxs;
    AccountTxs
    getAccountTxs(
        AccountID const& account,
        std::int32_t minLedger,
        std::int32_t maxLedger,
        bool descending,
        std::uint32_t offset,
        int limit,
        bool bUnlimited) override;

    AccountTxs
    getTxsAccount(
        AccountID const& account,
        std::int32_t minLedger,
        std::int32_t maxLedger,
        bool forward,
        std::optional<AccountTxMarker>& marker,
        int limit,
        bool bUnlimited) override;

    using NetworkOPs::MetaTxsList;
    using NetworkOPs::txnMetaLedgerType;

    MetaTxsList
    getAccountTxsB(
        AccountID const& account,
        std::int32_t minLedger,
        std::int32_t maxLedger,
        bool descending,
        std::uint32_t offset,
        int limit,
        bool bUnlimited) override;

    MetaTxsList
    getTxsAccountB(
        AccountID const& account,
        std::int32_t minLedger,
        std::int32_t maxLedger,
        bool forward,
        std::optional<AccountTxMarker>& marker,
        int limit,
        bool bUnlimited) override;

    //
    // Monitoring: publisher side.
    //
    void
    pubLedger(std::shared_ptr<ReadView const> const& lpAccepted) override;
    void
    pubProposedTransaction(
        std::shared_ptr<ReadView const> const& lpCurrent,
        std::shared_ptr<STTx const> const& stTxn,
        STer terResult) override;
    void
    pubValidation(std::shared_ptr<STValidation> const& val) override;

    void
    tryCheckSubTx() override;

    void
    checkSweepAccountDelay();

    void
    pubTableTxs(
        const AccountID& ownerId,
        const std::string& sTableName,
        const STTx& stTxn,
        const std::tuple<std::string, std::string, std::string>& disposRes,
        bool bVaidated) override;
    // publish results for chain-sql txs
    void
    pubTxResult(
        const STTx& stTxn,
        const std::tuple<std::string, std::string, std::string>& disposRes,
        bool validated,
        bool bForTableTx);
    void
    pubChainSqlTableTxs(
        const AccountID& ownerId,
        const std::string& sTableName,
        const STTx& stTxn,
        const std::tuple<std::string, std::string, std::string>& disposRes);

    void
    PubContractEvents(
        const AccountID& contractID,
        uint256 const* aTopic,
        int iTopicNum,
        const Blob& byValue) override;

    void pubViewChange(uint32_t ledgerSeq, uint64_t view) override;

    bool waitingForInit() override;

    //--------------------------------------------------------------------------
    //
    // InfoSub::Source.
    //
    void
    subAccount(
        InfoSub::ref ispListener,
        hash_set<AccountID> const& vnaAccountIDs,
        InfoSub::ACOUNT_TYPE eType) override;
    void
    unsubAccount(
        InfoSub::ref ispListener,
        hash_set<AccountID> const& vnaAccountIDs,
        InfoSub::ACOUNT_TYPE eType) override;

    // Just remove the subscription from the tracking
    // not from the InfoSub. Needed for InfoSub destruction
    void
    unsubAccountInternal(
        std::uint64_t seq,
        hash_set<AccountID> const& vnaAccountIDs,
        InfoSub::ACOUNT_TYPE eType) override;

    bool
    subLedger(InfoSub::ref ispListener, Json::Value& jvResult) override;
    bool
    unsubLedger(std::uint64_t uListener) override;

    virtual bool subViewChange(InfoSub::ref ispListener) override;
    virtual bool unsubViewChange(std::uint64_t uListener) override;
    // for all txs which changes the table
    virtual void
    subTable(
        InfoSub::ref ispListener,
        AccountID const& accountID,
        std::string const& sTableName) override;
    virtual void
    unsubTable(
        InfoSub::ref ispListener,
        AccountID const& accountID,
        std::string const& sTableName) override;

    // for a single tx
    virtual void
    subTransaction(InfoSub::ref ispListener, uint256 const& txId) override;
    virtual void
    unsubTransaction(InfoSub::ref ispListener, uint256 const& txId) override;

    bool
    subServer(InfoSub::ref ispListener, Json::Value& jvResult, bool admin)
        override;
    bool
    unsubServer(std::uint64_t uListener) override;

    bool
    subBook(InfoSub::ref ispListener, Book const&) override;
    bool
    unsubBook(std::uint64_t uListener, Book const&) override;

    bool
    subManifests(InfoSub::ref ispListener) override;
    bool
    unsubManifests(std::uint64_t uListener) override;
    void
    pubManifest(Manifest const&) override;

    bool
    subTransactions(InfoSub::ref ispListener) override;
    bool
    unsubTransactions(std::uint64_t uListener) override;

    bool
    subRTTransactions(InfoSub::ref ispListener) override;
    bool
    unsubRTTransactions(std::uint64_t uListener) override;

    bool
    subValidations(InfoSub::ref ispListener) override;
    bool
    unsubValidations(std::uint64_t uListener) override;

    bool
    subPeerStatus(InfoSub::ref ispListener) override;
    bool
    unsubPeerStatus(std::uint64_t uListener) override;
    void
    pubPeerStatus(std::function<Json::Value(void)> const&) override;

    bool
    subLogs(InfoSub::ref ispListener) override;
    bool
    unsubLogs(std::uint64_t uListener) override;
    void
    pubLogs(std::string const& log) override;

    bool
    subConsensus(InfoSub::ref ispListener) override;
    bool
    unsubConsensus(std::uint64_t uListener) override;

    InfoSub::pointer
    findRpcSub(std::string const& strUrl) override;
    InfoSub::pointer
    addRpcSub(std::string const& strUrl, InfoSub::ref) override;
    bool
    tryRemoveRpcSub(std::string const& strUrl) override;

    bool
    hasChainSQLTxListener() override
    {
        return !mSubTable.empty() || !mSubTx.empty() ||
            !mValidatedSubTx.empty();
    }

    std::pair<bool, std::string>
    createSchema(const std::shared_ptr<SLE const>& schema, bool bForce, bool bFromLoad = false)
        override;

    //--------------------------------------------------------------------------
    //
    // Stoppable.

    void
    onStop() override
    {
        mAcquiringLedger.reset();
        {
            boost::system::error_code ec;
            heartbeatTimer_.cancel(ec);
            if (ec)
            {
                JLOG(m_journal.error())
                    << "NetworkOPs: heartbeatTimer cancel error: "
                    << ec.message();
            }

            ec.clear();
            clusterTimer_.cancel(ec);
            if (ec)
            {
                JLOG(m_journal.error())
                    << "NetworkOPs: clusterTimer cancel error: "
                    << ec.message();
            }
        }
        // Make sure that any waitHandlers pending in our timers are done
        // before we declare ourselves stopped.
        using namespace std::chrono_literals;
        waitHandlerCounter_.join("NetworkOPs", 1s, m_journal);
        stopped();
    }

private:
    void
    setHeartbeatTimer();
    void
    setClusterTimer();
    void
    processHeartbeatTimer();
    void
    processClusterTimer();

    void
    processSubTxTimer();

    using time_point = std::chrono::system_clock::time_point;
    using SubTxMapType =
        hash_map<uint256, std::pair<InfoSub::wptr, time_point>>;
    using SubMapType = hash_map<std::uint64_t, InfoSub::wptr>;
    using SubInfoMapType = hash_map<AccountID, SubMapType>;
    using subRpcMapType = hash_map<std::string, InfoSub::pointer>;
    using SubTableMapType =
        hash_map<AccountID, hash_map<std::string, SubMapType>>;

    void
    processSubTx(SubTxMapType& subTx, const std::string& status);

    Json::Value
    transJson(
        const STTx& stTxn,
        TER terResult,
        bool bValidated,
        std::shared_ptr<ReadView const> const& lpCurrent);

    void
    pubValidatedTransaction(
        std::shared_ptr<ReadView const> const& alAccepted,
        const AcceptedLedgerTx& alTransaction);
    void
    pubAccountTransaction(
        std::shared_ptr<ReadView const> const& lpCurrent,
        const AcceptedLedgerTx& alTransaction,
        bool isAccepted);

    std::tuple<std::string, std::string, std::string>
    get_res(TER ter, std::string const& contractDetailMsg=std::string(""));

    void
    PubValidatedTxForTable(const AcceptedLedgerTx& alTx);

    void
    pubServer();
    void
    pubConsensus(ConsensusPhase phase);

    std::string
    getHostId(bool forAdmin);

    void
    checkSchemaTx(
        std::shared_ptr<ReadView const> const& alAccepted,
        const AcceptedLedgerTx& alTransaction);
    void
    resetSchemaCfg(uint256 schemaId, std::shared_ptr<SLE const> sleSchema);

    SubInfoMapType&
    getCompatibleSubInfoMap(InfoSub::ACOUNT_TYPE eType);

    void
    addToBroadCast(std::vector<uint256> const& vec);

    void
    broadCastTxs();

    void
    processAccountDelay(std::map<AccountID,int> const& map);
private:

    // XXX Split into more locks.
    using ScopedLockType = std::lock_guard <std::recursive_mutex>;

    Schema& app_;
    clock_type& m_clock;
    beast::Journal m_journal;

    std::unique_ptr<LocalTxs> m_localTX;

    std::recursive_mutex mSubLock;

    std::atomic<OperatingMode> mMode;

    std::atomic<bool> needNetworkLedger_{false};
    std::atomic<bool> amendmentBlocked_{false};
    std::atomic<bool> amendmentWarned_{false};

    ClosureCounter<void, boost::system::error_code const&> waitHandlerCounter_;
    boost::asio::steady_timer heartbeatTimer_;
    boost::asio::steady_timer clusterTimer_;

    RCLConsensus mConsensus;

    ConsensusPhase mLastConsensusPhase;

    LedgerMaster& m_ledgerMaster;
    std::shared_ptr<InboundLedger> mAcquiringLedger;

    SubInfoMapType mSubAccount;
    SubInfoMapType mSubRTAccount;
    SubInfoMapType mSubContract;

    subRpcMapType mRpcSubMap;

    SubTableMapType mSubTable;  //
    SubTxMapType mSubTx;        // All chain-sql related transactions.
    SubTxMapType mValidatedSubTx;

    // SubMapType mSubLedger;            // Accepted ledgers.
    // SubMapType mSubManifests;         // Received validator manifests.
    // SubMapType mSubServer;            // When server changes connectivity
    // state. SubMapType mSubTransactions;      // All accepted transactions.
    // SubMapType mSubRTTransactions;    // All proposed and accepted
    // transactions. SubMapType mSubValidations;       // Received validations.
    // SubMapType mSubPeerStatus;        // peer status changes
    enum SubTypes
    {
        sLedger,                    // Accepted ledgers.
        sManifests,                 // Received validator manifests.
        sServer,                    // When server changes connectivity state.
        sTransactions,              // All accepted transactions.
        sRTTransactions,            // All proposed and accepted transactions.
        sValidations,               // Received validations.
        sPeerStatus,                // Peer status changes.
        sConsensusPhase,  // Consensus phase
        sLogs,
        sViewChange,				 // ViewChange

        sLastEntry = sViewChange			// as this name implies, any new entry must
                                    // be ADDED ABOVE this one
    };
    std::array<SubMapType, SubTypes::sLastEntry + 1> mStreamMaps;

    ServerFeeSummary mLastFeeSummary;

    JobQueue& m_job_queue;

    // Whether we are in standalone mode.
    bool const m_standalone;

    bool m_bCheckTxThread = false;

    bool m_bAutoSync = false;

    // The number of nodes that we need to consider ourselves connected.
    std::size_t const minPeerCount_;

    // Transaction batching.
    std::condition_variable mCond;
    std::mutex mMutex;
    DispatchState mDispatchState = DispatchState::none;
    std::vector<TransactionStatus> mTransactions;

    StateAccounting accounting_{};

    std::vector<uint256> mTxToBroadCast;
    std::mutex mutexBroad_;
    bool m_bBroadThread = false;

    mutable std::shared_mutex mutexAccountDelay_;
    std::map<AccountID, AccountDelayEntry> mMapAccountDelay;

private:
    struct Stats
    {
        template <class Handler>
        Stats(
            Handler const& handler,
            beast::insight::Collector::ptr const& collector)
            : hook(collector->make_hook(handler))
            , disconnected_duration(collector->make_gauge(
                  "State_Accounting",
                  "Disconnected_duration"))
            , connected_duration(collector->make_gauge(
                  "State_Accounting",
                  "Connected_duration"))
            , syncing_duration(
                  collector->make_gauge("State_Accounting", "Syncing_duration"))
            , tracking_duration(collector->make_gauge(
                  "State_Accounting",
                  "Tracking_duration"))
            , full_duration(
                  collector->make_gauge("State_Accounting", "Full_duration"))
            , disconnected_transitions(collector->make_gauge(
                  "State_Accounting",
                  "Disconnected_transitions"))
            , connected_transitions(collector->make_gauge(
                  "State_Accounting",
                  "Connected_transitions"))
            , syncing_transitions(collector->make_gauge(
                  "State_Accounting",
                  "Syncing_transitions"))
            , tracking_transitions(collector->make_gauge(
                  "State_Accounting",
                  "Tracking_transitions"))
            , full_transitions(
                  collector->make_gauge("State_Accounting", "Full_transitions"))
        {
        }

        beast::insight::Hook hook;
        beast::insight::Gauge disconnected_duration;
        beast::insight::Gauge connected_duration;
        beast::insight::Gauge syncing_duration;
        beast::insight::Gauge tracking_duration;
        beast::insight::Gauge full_duration;

        beast::insight::Gauge disconnected_transitions;
        beast::insight::Gauge connected_transitions;
        beast::insight::Gauge syncing_transitions;
        beast::insight::Gauge tracking_transitions;
        beast::insight::Gauge full_transitions;
    };

    std::mutex m_statsMutex;  // Mutex to lock m_stats
    Stats m_stats;

private:
    void
    collect_metrics()
    {
        auto [counters, mode, start] = accounting_.getCounterData();
        auto const current =
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now() - start);
        counters[static_cast<std::size_t>(mode)].dur += current;

        std::lock_guard lock(m_statsMutex);
        m_stats.disconnected_duration.set(
            counters[static_cast<std::size_t>(OperatingMode::DISCONNECTED)]
                .dur.count());
        m_stats.connected_duration.set(
            counters[static_cast<std::size_t>(OperatingMode::CONNECTED)]
                .dur.count());
        m_stats.syncing_duration.set(
            counters[static_cast<std::size_t>(OperatingMode::SYNCING)]
                .dur.count());
        m_stats.tracking_duration.set(
            counters[static_cast<std::size_t>(OperatingMode::TRACKING)]
                .dur.count());
        m_stats.full_duration.set(
            counters[static_cast<std::size_t>(OperatingMode::FULL)]
                .dur.count());

        m_stats.disconnected_transitions.set(
            counters[static_cast<std::size_t>(OperatingMode::DISCONNECTED)]
                .transitions);
        m_stats.connected_transitions.set(
            counters[static_cast<std::size_t>(OperatingMode::CONNECTED)]
                .transitions);
        m_stats.syncing_transitions.set(
            counters[static_cast<std::size_t>(OperatingMode::SYNCING)]
                .transitions);
        m_stats.tracking_transitions.set(
            counters[static_cast<std::size_t>(OperatingMode::TRACKING)]
                .transitions);
        m_stats.full_transitions.set(
            counters[static_cast<std::size_t>(OperatingMode::FULL)]
                .transitions);
    }
};

//------------------------------------------------------------------------------

static std::array<char const*, 5> const stateNames{
    {"disconnected", "connected", "syncing", "tracking", "full"}};

std::array<char const*, 5> const NetworkOPsImp::states_ = stateNames;

std::array<Json::StaticString const, 5> const
    NetworkOPsImp::StateAccounting::states_ = {
        {Json::StaticString(stateNames[0]),
         Json::StaticString(stateNames[1]),
         Json::StaticString(stateNames[2]),
         Json::StaticString(stateNames[3]),
         Json::StaticString(stateNames[4])}};

//------------------------------------------------------------------------------
std::string
NetworkOPsImp::getHostId(bool forAdmin)
{
    static std::string const hostname = boost::asio::ip::host_name();

    if (forAdmin)
        return hostname;

    // For non-admin uses hash the node public key into a
    // single RFC1751 word:
    static std::string const shroudedHostId = [this]() {
        auto const& id = app_.nodeIdentity();

        return RFC1751::getWordFromBlob(id.first.data(), id.first.size());
    }();

    return shroudedHostId;
}

void
NetworkOPsImp::setStateTimer()
{
    setHeartbeatTimer();
    setClusterTimer();
}

void
NetworkOPsImp::setHeartbeatTimer()
{
    // Only start the timer if waitHandlerCounter_ is not yet joined.
    if (auto optionalCountedHandler = waitHandlerCounter_.wrap(
            [this](boost::system::error_code const& e) {
                if ((e.value() == boost::system::errc::success) &&
                    (!app_.isShutdown()))
                {
                    m_job_queue.addJob(
                        jtNETOP_TIMER, "NetOPs.heartbeat", [this](Job&) {
                            processHeartbeatTimer();
                        }, app_.doJobCounter());
                }
                // Recover as best we can if an unexpected error occurs.
                if (e.value() != boost::system::errc::success &&
                    e.value() != boost::asio::error::operation_aborted)
                {
                    // Try again later and hope for the best.
                    JLOG(m_journal.error())
                        << "Heartbeat timer got error '" << e.message()
                        << "'.  Restarting timer.";
                    setHeartbeatTimer();
                }
            }))
    {
        heartbeatTimer_.expires_from_now(mConsensus.parms().ledgerGRANULARITY);
        heartbeatTimer_.async_wait(std::move(*optionalCountedHandler));
    }
}

void
NetworkOPsImp::setClusterTimer()
{
    // Only start the timer if waitHandlerCounter_ is not yet joined.
    if (auto optionalCountedHandler = waitHandlerCounter_.wrap(
            [this](boost::system::error_code const& e) {
                if ((e.value() == boost::system::errc::success) &&
                    (!app_.isShutdown()))
                {
                    m_job_queue.addJob(
                        jtNETOP_CLUSTER, "NetOPs.cluster", [this](Job&) {
                            processClusterTimer();
                        }, app_.doJobCounter());
                }
                // Recover as best we can if an unexpected error occurs.
                if (e.value() != boost::system::errc::success &&
                    e.value() != boost::asio::error::operation_aborted)
                {
                    // Try again later and hope for the best.
                    JLOG(m_journal.error())
                        << "Cluster timer got error '" << e.message()
                        << "'.  Restarting timer.";
                    setClusterTimer();
                }
            }))
    {
        using namespace std::chrono_literals;
        clusterTimer_.expires_from_now(10s);
        clusterTimer_.async_wait(std::move(*optionalCountedHandler));
    }
}

void
NetworkOPsImp::processHeartbeatTimer()
{
    {
        std::unique_lock lock{app_.getMasterMutex()};

        // VFALCO NOTE This is for diagnosing a crash on exit
        LoadManager& mgr(app_.getLoadManager());
        mgr.resetDeadlockDetector();

        std::size_t const numPeers = app_.peerManager().size();

        // do we have sufficient peers? If not, we are disconnected.
        if (numPeers < minPeerCount_)
        {
            if (mMode != OperatingMode::DISCONNECTED)
            {
                setMode(OperatingMode::DISCONNECTED);
                JLOG(m_journal.warn())
                    << "Node count (" << numPeers << ") has fallen "
                    << "below required minimum (" << minPeerCount_ << ").";
            }

            // MasterMutex lock need not be held to call setHeartbeatTimer()
            lock.unlock();
            // We do not call mConsensus.timerEntry until there are enough
            // peers providing meaningful inputs to consensus
            setHeartbeatTimer();
            return;
        }

        if (mMode == OperatingMode::DISCONNECTED)
        {
            setMode(OperatingMode::CONNECTED);
            JLOG(m_journal.info())
                << "Node count (" << numPeers << ") is sufficient.";
        }

        // Check if the last validated ledger forces a change between these
        // states.
        if (mMode == OperatingMode::SYNCING)
            setMode(OperatingMode::SYNCING);
        else if (mMode == OperatingMode::CONNECTED)
            setMode(OperatingMode::CONNECTED);
    }

    auto now = app_.timeKeeper().closeTime();

    mConsensus.timerEntry(now);

    app_.getTxPool().timerEntry(now);

    tryCheckSubTx();

    if(app_.config().BATCH_BROADCAST)
        broadCastTxs();

    const ConsensusPhase currPhase = mConsensus.phase();
    if (mLastConsensusPhase != currPhase)
    {
        reportConsensusStateChange(currPhase);
        mLastConsensusPhase = currPhase;
    }
    app_.getPrometheusClient().timerEntry(now);
    setHeartbeatTimer();
}

void
NetworkOPsImp::tryCheckSubTx()
{
    if (!m_bCheckTxThread && (mSubTx.size() > 0 || mValidatedSubTx.size() > 0))
    {
        m_bCheckTxThread = true;
        m_job_queue.addJob(jtCheckSubTx, "NetOPs.processSubTx", [this](Job&) {
            processSubTxTimer();
        }, app_.doJobCounter());
    }
}

void
NetworkOPsImp::broadCastTxs()
{
    if(!m_bBroadThread && mTxToBroadCast.size() > 0)
    {
        m_bBroadThread = true;
        m_job_queue.addJob(jtBROADCASTBATCH, "NetOPs.boradcastTxs", [this](Job&) {
            std::vector<uint256> transactions;
            if (mTxToBroadCast.size() < MAX_BROAD_CAST_BATCH)
            {
                std::unique_lock lock(mutexBroad_);
                mTxToBroadCast.swap(transactions);
            }
            else
            {
                std::unique_lock lock(mutexBroad_);
                transactions = std::move(std::vector<uint256>(
                    mTxToBroadCast.begin(),
                    mTxToBroadCast.begin() + MAX_BROAD_CAST_BATCH));
                mTxToBroadCast.erase(
                    mTxToBroadCast.begin(),
                    mTxToBroadCast.begin() + MAX_BROAD_CAST_BATCH);
            }
            protocol::TMTransactions txs;
            for (auto key : transactions)
            {
                Serializer s;
                auto tx = app_.getMasterTransaction().fetch(key);
                tx->getSTransaction()->add(s);
                auto pTx = txs.add_transactions();
                pTx->set_rawtransaction(s.data(), s.size());
            }
            txs.set_schemaid(app_.schemaId().begin(), uint256::size());

            std::set<std::uint32_t> toSkip;
            app_.peerManager().foreach(send_if_not(
                std::make_shared<Message>(txs, protocol::mtTRANSACTIONS),
                peer_in_set(toSkip)));       
            m_bBroadThread = false;
        }, app_.doJobCounter());
    }
}

void
NetworkOPsImp::processSubTxTimer()
{
    std::lock_guard sl(mSubLock);

    processSubTx(mSubTx, (std::string)jss::validate_timeout);
    processSubTx(mValidatedSubTx, (std::string)jss::db_timeout);

    m_bCheckTxThread = false;
}

void 
NetworkOPsImp::processAccountDelay(std::map<AccountID, int> const& map)
{
    std::unique_lock<std::shared_mutex> lock(mutexAccountDelay_);

    auto timeStart = utcTime();
    std::chrono::seconds constexpr timeDelay{30};
    auto now = m_clock.now();
    //try sweep map
    auto it = mMapAccountDelay.begin();
    while (it != mMapAccountDelay.end())
    {
        if (it->second.failureCount >= DELAY_START_COUNT &&
            it->second.timeExpires <= now)
        {
            it->second.failureCount = DELAY_START_COUNT - 
                (1 + (now - it->second.timeExpires) / timeDelay);
            it->second.lastTouch = now;
        }
        else if(it->second.failureCount < DELAY_START_COUNT && it->second.lastTouch + timeDelay < now)
        {
            it->second.failureCount -= (1 + (now - it->second.lastTouch) / timeDelay);
            it->second.lastTouch = now;
        }

        if (it->second.failureCount <= 0)
            it = mMapAccountDelay.erase(it);
        else
            it++;
    }

    auto iter = map.begin();
    while (iter != map.end())
    {
        if (mMapAccountDelay.find(iter->first) != mMapAccountDelay.end())
        {
            bool bDelayedBefore =
                mMapAccountDelay[iter->first].failureCount >= DELAY_START_COUNT;
            mMapAccountDelay[iter->first].failureCount += iter->second;
            if (bDelayedBefore)
            {
                mMapAccountDelay[iter->first].timeExpires += (timeDelay * iter->second);
            }
            else
            {
                if (mMapAccountDelay[iter->first].failureCount < DELAY_START_COUNT)
                    mMapAccountDelay[iter->first].lastTouch = now;
                else
                    mMapAccountDelay[iter->first].timeExpires = now +
                        timeDelay * (mMapAccountDelay[iter->first].failureCount - DELAY_START_COUNT + 1);
            }
        }
        else
        {
            mMapAccountDelay[iter->first].failureCount = iter->second;
            if (mMapAccountDelay[iter->first].failureCount >= DELAY_START_COUNT)
                mMapAccountDelay[iter->first].timeExpires = now + 
                    timeDelay * (mMapAccountDelay[iter->first].failureCount - DELAY_START_COUNT + 1);
            else
                mMapAccountDelay[iter->first].lastTouch = now;
        }

        
        if (mMapAccountDelay[iter->first].timeExpires > now + timeDelay * 10)
            mMapAccountDelay[iter->first].timeExpires = now + timeDelay * 10;

        iter++;
    }
    JLOG(m_journal.info()) << "processAccountDelay, time used: "
                           << utcTime() - timeStart << "ms";
}

void
NetworkOPsImp::processSubTx(SubTxMapType& subTx, const std::string& status)
{
    auto iter = subTx.begin();
    while (iter != subTx.end())
    {
        auto now = std::chrono::system_clock::now();
        using duration_type = std::chrono::duration<double>;
        duration_type time_span = std::chrono::duration_cast<duration_type>(now - iter->second.second);
        if (time_span.count() >= TX_TIMEOUT)
        //if(app_.getLedgerMaster().getValidLedgerIndex() > iter->second.second)
        {
            //notify time out
            InfoSub::pointer p = iter->second.first.lock();
            if (p)
            {
                Json::Value jvObj(Json::objectValue);
                jvObj[jss::type] = "singleTransaction";
                jvObj[jss::transaction][jss::hash] = to_string(iter->first);
                jvObj[jss::status] = status;
                p->send(jvObj, true);
            }

            //// remove from tx-pool and reset account sequence
            //if (status == "validate_timeout")
            //{
            //    auto tx =
            //        app_.getMasterTransaction().fetch(iter->first);
            //    if (tx)
            //    {
            //        auto act = tx->getSTransaction()->getAccountID(
            //            sfAccount);
            //        app_.getStateManager().resetAccountSeq(act);
            //    }
            //    app_.getTxPool().removeTx(iter->first);
            //}

            iter = subTx.erase(iter);
        }
        else
        {
            iter++;
        }
    }
}

void
NetworkOPsImp::processClusterTimer()
{
    using namespace std::chrono_literals;
    bool const update = app_.cluster().update(
        app_.nodeIdentity().first,
        "",
        (m_ledgerMaster.getValidatedLedgerAge() <= 4min)
            ? app_.getFeeTrack().getLocalFee()
            : 0,
        app_.timeKeeper().now());

    if (!update)
    {
        JLOG(m_journal.debug()) << "Too soon to send cluster update";
        setClusterTimer();
        return;
    }

    protocol::TMCluster cluster;
    app_.cluster().for_each([&cluster](ClusterNode const& node) {
        protocol::TMClusterNode& n = *cluster.add_clusternodes();
        n.set_publickey(toBase58(TokenType::NodePublic, node.identity()));
        n.set_reporttime(node.getReportTime().time_since_epoch().count());
        n.set_nodeload(node.getLoadFee());
        if (!node.name().empty())
            n.set_nodename(node.name());
    });

    Resource::Gossip gossip = app_.getResourceManager().exportConsumers();
    for (auto& item : gossip.items)
    {
        protocol::TMLoadSource& node = *cluster.add_loadsources();
        node.set_name(to_string(item.address));
        node.set_cost(item.balance);
    }
    app_.peerManager().foreach(send_if(
        std::make_shared<Message>(cluster, protocol::mtCLUSTER),
        peer_in_cluster()));
    setClusterTimer();
}

//------------------------------------------------------------------------------

std::string
NetworkOPsImp::strOperatingMode(OperatingMode const mode, bool const admin)
    const
{
    if (mode == OperatingMode::FULL && admin)
    {
        auto const consensusMode = mConsensus.mode();
        if (consensusMode != ConsensusMode::wrongLedger)
        {
            if (consensusMode == ConsensusMode::proposing)
                return "proposing";

            if (mConsensus.validating())
                return "validating";
        }
    }

    return states_[static_cast<std::size_t>(mode)];
}

void
NetworkOPsImp::submitTransaction(std::shared_ptr<STTx const> const& iTrans)
{
    if (isNeedNetworkLedger())
    {
        // Nothing we can do if we've never been in sync
        return;
    }

    // this is an asynchronous interface
    auto const trans = sterilize(*iTrans);

    auto const txid = trans->getTransactionID();
    auto const flags = app_.getHashRouter().getFlags(txid);

    if ((flags & SF_BAD) != 0)
    {
        JLOG(m_journal.warn()) << "Submitted transaction cached bad";
        return;
    }

    try
    {
        auto const [validity, reason] = checkValidity(
            app_,
            app_.getHashRouter(),
            *trans,
            m_ledgerMaster.getValidatedRules(),
            app_.config());

        if (validity != Validity::Valid)
        {
            JLOG(m_journal.warn())
                << "Submitted transaction invalid: " << reason;
            return;
        }
    }
    catch (std::exception const&)
    {
        JLOG(m_journal.warn()) << "Exception checking transaction" << txid;

        return;
    }

    std::string reason;

    auto tx = std::make_shared<Transaction>(trans, reason, app_);

    m_job_queue.addJob(jtTRANSACTION, "submitTxn", [this, tx](Job&) {
        auto t = tx;
        processTransaction(t, false, false, FailHard::no);
    }, app_.doJobCounter());
}

void
NetworkOPsImp::processTransaction(
    std::shared_ptr<Transaction>& transaction,
    bool bUnlimited,
    bool bLocal,
    FailHard failType)
{
    auto ev = m_job_queue.makeLoadEvent(jtTXN_PROC, "ProcessTXN");
    auto const newFlags = app_.getHashRouter().getFlags(transaction->getID());

    if ((newFlags & SF_BAD) != 0)
    {
        // cached bad
        transaction->setStatus(INVALID);
        transaction->setResult(temBAD_SIGNATURE);
        return;
    }

    // NOTE eahennis - I think this check is redundant,
    // but I'm not 100% sure yet.
    // If so, only cost is looking up HashRouter flags.
    auto const view = m_ledgerMaster.getCurrentLedger();
    auto const [validity, reason] = checkValidity(
        app_,
        app_.getHashRouter(),
        *transaction->getSTransaction(),
        view->rules(),
        app_.config());
    assert(validity == Validity::Valid);

    // Not concerned with local checks at this point.
    if (validity == Validity::SigBad)
    {
        JLOG(m_journal.info()) << "Transaction has bad signature: " << reason;
        transaction->setStatus(INVALID);
        transaction->setResult(temBAD_SIGNATURE);
        app_.getHashRouter().setFlags(transaction->getID(), SF_BAD);
        return;
    }

    // canonicalize can change our pointer
    app_.getMasterTransaction().canonicalize(&transaction);

    if (bLocal)
        doTransactionSync(transaction, bUnlimited, failType);
    else
        doTransactionAsync(transaction, bUnlimited, failType);
}

std::pair<STer, bool>
NetworkOPsImp::doTransactionCheck(
    std::shared_ptr<Transaction> transaction,
    ApplyFlags flags,
    OpenView const& view)
{
    auto txCur = transaction->getSTransaction();

    if (app_.getTxPool().txExists(txCur->getTransactionID()))
    {
        std::string from = (flags & tapFromClient) ? "local" : "relay";
        JLOG(m_journal.info()) << "Tx: " << txCur->getTransactionID()
                               << " already in Tx pool from " << from;

        return {tefALREADY, false};
    }

    PreflightContext const pfctx(
        app_, *txCur, app_.openLedger().current()->rules(), flags, m_journal);

    auto ter = check(pfctx, view);

    if (ter.ter == tesSUCCESS)
    {
        // after check and transaction's check result is tesSUCCESS add it to
        // TxPool:
        ter = app_.getTxPool().insertTx(transaction, view.seq());
        if (ter.ter != tesSUCCESS)
        {
            return {ter, false};
        }

        app_.getStateManager().onTxCheckSuccess(txCur->getAccountID(sfAccount));
        return {tesSUCCESS, true};
    }
    else if (ter.ter != terPRE_SEQ && ter.ter != tefPAST_SEQ)
    {
        app_.getStateManager().addFailedSeq(
            txCur->getAccountID(sfAccount), txCur->getSequence());
    }

    return {ter, false};
}

STer
NetworkOPsImp::checkForAccountDelay(PreflightContext const& pfctx)
{
    STer ter = tesSUCCESS;
    if (!app_.config().OPEN_ACCOUNT_DELAY)
        return ter;

    std::shared_lock<std::shared_mutex> lock(mutexAccountDelay_);
    auto const id = pfctx.tx.getAccountID(sfAccount);
    if (mMapAccountDelay.find(id) != mMapAccountDelay.end())
    {
        if (mMapAccountDelay[id].failureCount >= DELAY_START_COUNT)
        {
            auto timeForbidden =
                std::chrono::duration_cast<std::chrono::seconds>(
                    mMapAccountDelay[id].timeExpires - m_clock.now())
                    .count();
            if (timeForbidden == 0)
                timeForbidden = 1;
            if (timeForbidden > 0)
            {
                std::string msg = "The account is forbidden to submit tx,";
                msg += std::to_string(timeForbidden);
                msg += " seconds remaining.";
                ter = STer(tefACCOUNT_FORBIDDEN, msg);
                return ter;
            }
        }        
    }
    return ter;
}

STer
NetworkOPsImp::check(PreflightContext const& pfctx, OpenView const& view)
{
    STer ter = preflight1(pfctx);
    if (ter.ter != tesSUCCESS)
    {
        return ter;
    }

    ter = checkForAccountDelay(pfctx);
    if (ter.ter != tesSUCCESS)
    {
        return ter;
    }

    // mock the process of doTransactionAsync or doTransactionSync

    /**
     * In its inner function like apply, you should do the following:
     * 1. check sequence & LastLedgerSeq like in Transactor::checkSeq
     * 2. check fee like Transactor::checkFee
     * 3. check accountid match publickey like Transactor::checkSign
     */

    boost::optional<PreclaimContext const> pcctx;
    pcctx.emplace(app_, view, ter, pfctx.tx, pfctx.flags, m_journal);

    ter = Transactor::checkFrozen(*pcctx);
    if (ter.ter != tesSUCCESS)
        return ter;

    ter = Transactor::checkSign(*pcctx);
    if (ter.ter != tesSUCCESS)
        return ter;

    ter = Transactor::checkUserCert(*pcctx);
    if (ter.ter != tesSUCCESS)
        return ter;

    auto const baseFee = Transactor::calculateBaseFee(pcctx->view, pcctx->tx);
    ter = Transactor::checkFee(*pcctx, baseFee);
    if (ter.ter != tesSUCCESS)
        return ter;

    //Move to end,since terPreSeq code will heldTransaction
    ter = Transactor::checkSeq2(*pcctx);
    if (ter.ter != tesSUCCESS)
    {
        return ter;
    }

    return ter;
}

void
NetworkOPsImp::doTransactionAsync(
    std::shared_ptr<Transaction> transaction,
    bool bUnlimited,
    FailHard failType)
{
    std::lock_guard lock(mMutex);

    if (transaction->getApplying())
        return;

    mTransactions.push_back(
        TransactionStatus(transaction, bUnlimited, false, failType));
    transaction->setApplying();

    if (mDispatchState == DispatchState::none)
    {
        if (m_job_queue.addJob(jtBATCH, "transactionBatch", [this](Job&) {
                transactionBatch();
            }, app_.doJobCounter()))
        {
            mDispatchState = DispatchState::scheduled;
        }
    }
}

void
NetworkOPsImp::doTransactionSync(
    std::shared_ptr<Transaction> transaction,
    bool bUnlimited,
    FailHard failType)
{
    auto stTx = *transaction->getSTransaction();
    if (stTx.isChainSqlTableType())
    {
        bool ret = app_.getTableAssistant().Put(stTx);
        if (!ret)
        {
            transaction->setStatus(INVALID);
            transaction->setResult(temBAD_PUT);
            return;
        }
    }
    std::unique_lock<std::mutex> lock(mMutex);

    if (!transaction->getApplying())
    {
        mTransactions.push_back(
            TransactionStatus(transaction, bUnlimited, true, failType));
        transaction->setApplying();
    }

    do
    {
        if (mDispatchState == DispatchState::running)
        {
            // A batch processing job is already running, so wait.
            mCond.wait(lock);
        }
        else
        {
            apply(lock);

            if (mTransactions.size())
            {
                // More transactions need to be applied, but by another job.
                if (m_job_queue.addJob(
                        jtBATCH, "transactionBatch", [this](Job&) {
                            transactionBatch();
                        }, app_.doJobCounter()))
                {
                    mDispatchState = DispatchState::scheduled;
                }
            }
        }
    } while (transaction->getApplying());
}

void
NetworkOPsImp::transactionBatch()
{
    std::unique_lock<std::mutex> lock(mMutex);

    if (mDispatchState == DispatchState::running)
        return;

    while (mTransactions.size())
    {
        apply(lock);
    }
}

void
NetworkOPsImp::apply(std::unique_lock<std::mutex>& batchLock)
{
    std::vector<TransactionStatus> submit_held;
    std::vector<TransactionStatus> transactions;
    if(mTransactions.size() > 10000)
    {
        JLOG(m_journal.error()) << "mTransactions.size(): " << mTransactions.size();
    }
    
    mTransactions.swap (transactions);
    assert (! transactions.empty());

    assert(mDispatchState != DispatchState::running);
    mDispatchState = DispatchState::running;

    batchLock.unlock();

    {
        std::unique_lock masterLock{app_.getMasterMutex(), std::defer_lock};
        bool changed = false;
        {
            // std::lock_guard <std::recursive_mutex> lock (
            //    m_ledgerMaster.peekMutex());

            // app_.openLedger().modify(
            //    [&](OpenView& view, beast::Journal j)
            //{
            for (TransactionStatus& e : transactions)
            {
                // we check before adding to the batch
                ApplyFlags flags = tapNO_CHECK_SIGN;
                if (e.local)
                    flags = flags | tapFromClient;
                else
                    flags = flags | tapByRelay;

                if (e.admin)
                    flags |= tapUNLIMITED;

                if (e.failType == FailHard::yes)
                    flags |= tapFAIL_HARD;

                // if (mConsensus.adaptor_.getUseNewConsensus())
                //{
                auto const result = doTransactionCheck(
                    e.transaction, flags, *app_.checkedOpenLedger().current());
                //}
                // else
                //{
                //    //auto const result = app_.getTxQ().apply(
                //    //    app_, view, e.transaction->getSTransaction(),
                //    //    flags, j);
                //}
                e.result = result.first;
                e.applied = result.second;

                if (e.result.ter == tefTABLE_STORAGEERROR)
                    e.failType = FailHard::yes;
                changed = changed || result.second;
            }
            // return changed;
            //});
        }
        // if (changed)
        //    reportFeeChange();

        boost::optional<LedgerIndex> validatedLedgerIndex;
        if (auto const l = m_ledgerMaster.getValidatedLedger())
            validatedLedgerIndex = l->info().seq;

        std::vector<uint256> vecTxToBrod;
        auto newOL = app_.openLedger().current();
        for (TransactionStatus& e : transactions)
        {
            e.transaction->clearSubmitResult();

            if (e.applied)
            {
                pubProposedTransaction(
                    newOL, e.transaction->getSTransaction(), e.result);
                e.transaction->setApplied();
            }

            e.transaction->setResult(e.result);

            if (isTemMalformed(e.result))
                app_.getHashRouter().setFlags(e.transaction->getID(), SF_BAD);

#ifdef BEAST_DEBUG
            if (e.result.ter != tesSUCCESS)
            {
                std::string token, human;

                if (transResultInfo(e.result, token, human))
                {
                    JLOG(m_journal.info())
                        << "TransactionResult: " << token << ": " << human;
                }
            }
#endif

            //bool addLocal = e.local;

            if (e.result.ter == tesSUCCESS)
            {
                JLOG(m_journal.debug())
                    << "Transaction is now included in open ledger";
                e.transaction->setStatus(INCLUDED);

                auto txCur = e.transaction->getSTransaction();
                for (auto const& tx : m_ledgerMaster.pruneHeldTransactions(
                         txCur->getAccountID(sfAccount),
                         txCur->getSequence() + 1))
                {
                    //std::string reason;
                    //auto const trans = sterilize(*tx);
                    //auto t = std::make_shared<Transaction>(trans, reason, app_);
                    submit_held.emplace_back(tx, false, false, FailHard::no);
                    //app_.getMasterTransaction().canonicalize(&t, true);
                    tx->setApplying();
                }
            }
            else if (e.result.ter == tefPAST_SEQ)
            {
                // duplicate or conflict
                JLOG(m_journal.info())
                    << "Transaction is obsolete " << e.transaction->getID()
                    << " from " << (e.local ? "local" : "remote");
                e.transaction->setStatus(OBSOLETE);
            }
            else if (isTerRetry(e.result.ter))
            {
                if (e.failType != FailHard::yes)
                {
                    auto txCur = e.transaction->getSTransaction();
                    auto seq = app_.getStateManager().getAccountCheckSeq(
                        txCur->getAccountID(sfAccount),
                        *app_.checkedOpenLedger().current());

                    if (txCur->getSequence() > seq + 2*MAX_ACCOUNT_HELD_COUNT)
                    {
                        JLOG(m_journal.warn())
                            << "Account sequence too large,accountId="
                            << txCur->getAccountID(sfAccount)
                            << ", curSeq = " << seq
                            << ",tx.Sequence=" << txCur->getSequence() << ",tx.hash="<<txCur->getTransactionID();
                        e.transaction->setResult(telSEQ_TOOLARGE);
                        e.transaction->setStatus(REMOVED);
                    }
                    else
                    {
                        bool bForceAdd = (txCur->getSequence() <
                            seq + MAX_ACCOUNT_HELD_COUNT);
                        // transaction should be held
                        bool ret = m_ledgerMaster.addHeldTransaction(e.transaction,bForceAdd);
                        if (ret)
                        {
                            JLOG(m_journal.info())
                                << "Transaction should be held: " << e.result;
                            e.transaction->setStatus(HELD);
                            e.transaction->setKept();
                        }
                        else
                        {
                            e.transaction->setResult(telTX_HELD_FAIL);
                            e.transaction->setStatus(REMOVED);
                        }
                    }
                }
            }
            else
            {
                JLOG(m_journal.info())
                    << "Status other than success " << e.transaction->getID()
                    << " from " << (e.local ? "local " : "remote ")
                    << e.transaction->getSTransaction()->getAccountID(sfAccount)
                    << " " << e.transaction->getSTransaction()->getSequence()
                    << " " << e.result;
                e.transaction->setStatus(INVALID);
            }

            auto const enforceFailHard =
                e.failType == FailHard::yes && !isTesSuccess(e.result);
            //// chainsql type tx will not retry.
            //if (addLocal && !enforceFailHard &&
            //    !e.transaction->getSTransaction()->isChainSqlTableType())
            //{
            //    m_localTX->push_back(
            //        m_ledgerMaster.getCurrentLedgerIndex(),
            //        e.transaction->getSTransaction());
            //    e.transaction->setKept();
            //}

            if ((e.applied ||
                 ((mMode != OperatingMode::FULL) &&
                  (e.failType != FailHard::yes) && e.local)) &&
                !enforceFailHard)
            {
                if(e.local && app_.config().BATCH_BROADCAST)
                {
                    vecTxToBrod.push_back(e.transaction->getID());
                }
                else
                {
                    auto const toSkip =
                       app_.getHashRouter().shouldRelay(e.transaction->getID());

                    if (toSkip)
                    {
                       protocol::TMTransaction tx;
                       Serializer s;

                       e.transaction->getSTransaction()->add(s);
                       tx.set_rawtransaction(s.data(), s.size());
                       tx.set_status(protocol::tsCURRENT);
                       tx.set_receivetimestamp(
                           app_.timeKeeper().now().time_since_epoch().count());
                       tx.set_deferred(e.result.ter == terQUEUED);
                       tx.set_schemaid(app_.schemaId().begin(), uint256::size());
                       // FIXME: This should be when we received it
                       app_.peerManager().foreach(send_if_not(
                           std::make_shared<Message>(tx, protocol::mtTRANSACTION),
                           peer_in_set(*toSkip)));
                       e.transaction->setBroadcast();
                    }
                }
            }

            if (validatedLedgerIndex)
            {
                auto [fee, accountSeq, availableSeq] =
                    app_.getTxQ().getTxRequiredFeeAndSeq(
                        *newOL, e.transaction->getSTransaction());
                e.transaction->setCurrentLedgerState(
                    *validatedLedgerIndex, fee, accountSeq, availableSeq);
            }
        }
        addToBroadCast(vecTxToBrod);
    }


    batchLock.lock();

    for (TransactionStatus& e : transactions)
        e.transaction->clearApplying();

    if (!submit_held.empty())
    {
        if (mTransactions.empty())
            mTransactions.swap(submit_held);
        else
            mTransactions.insert(
                mTransactions.begin(), submit_held.begin(), submit_held.end());
    }

    mCond.notify_all();

    mDispatchState = DispatchState::none;
}


void
NetworkOPsImp::addToBroadCast(std::vector<uint256> const& vecTxs)
{
    std::unique_lock lock(mutexBroad_);
    mTxToBroadCast.insert(mTxToBroadCast.end(),vecTxs.begin(), vecTxs.end());
}
//
// Owner functions
//

Json::Value
NetworkOPsImp::getOwnerInfo(
    std::shared_ptr<ReadView const> lpLedger,
    AccountID const& account)
{
    Json::Value jvObjects(Json::objectValue);
    auto root = keylet::ownerDir(account);
    auto sleNode = lpLedger->read(keylet::page(root));
    if (sleNode)
    {
        std::uint64_t uNodeDir;

        do
        {
            for (auto const& uDirEntry : sleNode->getFieldV256(sfIndexes))
            {
                auto sleCur = lpLedger->read(keylet::child(uDirEntry));
                assert(sleCur);

                switch (sleCur->getType())
                {
                    case ltOFFER:
                        if (!jvObjects.isMember(jss::offers))
                            jvObjects[jss::offers] =
                                Json::Value(Json::arrayValue);

                        jvObjects[jss::offers].append(
                            sleCur->getJson(JsonOptions::none));
                        break;

                    case ltRIPPLE_STATE:
                        if (!jvObjects.isMember(jss::ripple_lines))
                        {
                            jvObjects[jss::ripple_lines] =
                                Json::Value(Json::arrayValue);
                        }

                        jvObjects[jss::ripple_lines].append(
                            sleCur->getJson(JsonOptions::none));
                        break;

                    case ltACCOUNT_ROOT:
                    case ltDIR_NODE:
                    default:
                        assert(false);
                        break;
                }
            }

            uNodeDir = sleNode->getFieldU64(sfIndexNext);

            if (uNodeDir)
            {
                sleNode = lpLedger->read(keylet::page(root, uNodeDir));
                assert(sleNode);
            }
        } while (uNodeDir);
    }

    return jvObjects;
}

//
// Other
//

void
NetworkOPsImp::setAmendmentBlocked()
{
    amendmentBlocked_ = true;
    setMode(OperatingMode::TRACKING);
}

bool
NetworkOPsImp::checkLastClosedLedger(
    const PeerManager::PeerSequence& peerList,
    uint256& networkClosed)
{
    // Returns true if there's an *abnormal* ledger issue, normal changing in
    // TRACKING mode should return false.  Do we have sufficient validations for
    // our last closed ledger? Or do sufficient nodes agree? And do we have no
    // better ledger available?  If so, we are either tracking or full.

    JLOG(m_journal.trace()) << "NetworkOPsImp::checkLastClosedLedger";

    auto const ourClosed = m_ledgerMaster.getClosedLedger();

    if (!ourClosed)
        return false;

    uint256 closedLedger = ourClosed->info().hash;
    uint256 prevClosedLedger = ourClosed->info().parentHash;
    JLOG(m_journal.trace()) << "OurClosed:  " << closedLedger;
    JLOG(m_journal.trace()) << "PrevClosed: " << prevClosedLedger;

    //-------------------------------------------------------------------------
    // Determine preferred last closed ledger

    auto& validations = app_.getValidations();
    JLOG(m_journal.debug())
        << "ValidationTrie " << Json::Compact(validations.getJsonTrie());

    // Will rely on peer LCL if no trusted validations exist
    hash_map<uint256, std::uint32_t> peerCounts;
    peerCounts[closedLedger] = 0;
    if (mMode >= OperatingMode::TRACKING)
        peerCounts[closedLedger]++;

    for (auto& peer : peerList)
    {
        uint256 peerLedger = peer->getClosedLedgerHash(app_.schemaId());

        if (peerLedger.isNonZero())
            ++peerCounts[peerLedger];
    }

    for (auto const& it : peerCounts)
        JLOG(m_journal.debug()) << "L: " << it.first << " n=" << it.second;

    uint256 preferredLCL = validations.getPreferredLCL(
        RCLValidatedLedger{ourClosed, validations.adaptor().journal()},
        m_ledgerMaster.getValidLedgerIndex(),
        peerCounts);

    bool switchLedgers = preferredLCL != closedLedger;
    if (switchLedgers)
        closedLedger = preferredLCL;
    //-------------------------------------------------------------------------
    if (switchLedgers && (closedLedger == prevClosedLedger))
    {
        // don't switch to our own previous ledger
        JLOG(m_journal.info()) << "We won't switch to our own previous ledger";
        networkClosed = ourClosed->info().hash;
        switchLedgers = false;
    }
    else
        networkClosed = closedLedger;

    if (!switchLedgers)
        return false;

    auto consensus = m_ledgerMaster.getLedgerByHash(closedLedger);

    if (!consensus)
        consensus = app_.getInboundLedgers().acquire(
            closedLedger, 0, InboundLedger::Reason::CONSENSUS);

    if (consensus &&
        (!m_ledgerMaster.canBeCurrent(consensus, mConsensus.consensusType()) ||
         !m_ledgerMaster.isCompatible(
             *consensus, m_journal.debug(), "Not switching")))
    {
        // Don't switch to a ledger not on the validated chain
        // or with an invalid close time or sequence
        networkClosed = ourClosed->info().hash;
        return false;
    }

    JLOG(m_journal.warn()) << "We are not running on the consensus ledger";
    JLOG(m_journal.info()) << "Our LCL: " << getJson(*ourClosed);
    JLOG(m_journal.info()) << "Net LCL " << closedLedger;

    if ((mMode == OperatingMode::TRACKING) || (mMode == OperatingMode::FULL))
    {
        setMode(OperatingMode::CONNECTED);
    }

    if (consensus)
    {
        // FIXME: If this rewinds the ledger sequence, or has the same
        // sequence, we should update the status on any stored transactions
        // in the invalidated ledgers.
        switchLastClosedLedger(consensus);
    }

    return true;
}

void
NetworkOPsImp::switchLastClosedLedger(
    std::shared_ptr<Ledger const> const& newLCL)
{
    // set the newLCL as our last closed ledger -- this is abnormal code
    JLOG(m_journal.error())
        << "JUMP last closed ledger to " << newLCL->info().hash;

    clearNeedNetworkLedger();

    // Update fee computations.
    app_.getTxQ().processClosedLedger(app_, *newLCL, true);

    // Clear avoid when restart consensus for a ledger.
    app_.getTxPool().clearAvoid(newLCL->seq() + 1);
    // Caller must own master lock
    {
        // Apply tx in old open ledger to new
        // open ledger. Then apply local tx.

        auto retries = m_localTX->getTxSet();
        auto const lastVal = app_.getLedgerMaster().getValidatedLedger();
        boost::optional<Rules> rules;
        if (lastVal)
            rules.emplace(*lastVal, app_.config().features);
        else
            rules.emplace(app_.config().features);
        app_.openLedger().accept(
            app_,
            *rules,
            newLCL,
            OrderedTxs({}),
            false,
            retries,
            tapNONE,
            "jump",
            [&](OpenView& view, beast::Journal j) {
                // Stuff the ledger with transactions from the queue.
                return app_.getTxQ().accept(app_, view);
            });
    }

    m_ledgerMaster.switchLCL(newLCL);

    notify(app_, protocol::neSWITCHED_LEDGER, newLCL, true, m_journal);
}

void
NetworkOPsImp::setGenesisLedgerIndex(LedgerIndex seq)
{
    mConsensus.setGenesisLedgerIndex(seq);
}

void
NetworkOPsImp::onDeleteUntrusted(hash_set<NodeID> const& removed)
{
    mConsensus.onDeleteUntrusted(removed);
}

bool
NetworkOPsImp::beginConsensus(uint256 const& networkClosed)
{
    assert(networkClosed.isNonZero());

    auto closingInfo = m_ledgerMaster.getCurrentLedger()->info();

    JLOG(m_journal.info()) << "Consensus time for #" << closingInfo.seq
                           << " with LCL " << closingInfo.parentHash;

    auto prevLedger = m_ledgerMaster.getLedgerByHash(closingInfo.parentHash);

    if (!prevLedger)
    {
        JLOG(m_journal.warn()) << "Don't have LCL";
        // this shouldn't happen unless we jump ledgers
        if (mMode == OperatingMode::FULL)
        {
            JLOG(m_journal.warn()) << "Don't have LCL, going to tracking";
            setMode(OperatingMode::TRACKING);
        }

        return false;
    }

    assert(prevLedger->info().hash == closingInfo.parentHash);
    assert(
        closingInfo.parentHash ==
        m_ledgerMaster.getClosedLedger()->info().hash);

    if (prevLedger->rules().enabled(featureNegativeUNL))
        app_.validators().setNegativeUNL(prevLedger->negativeUNL());

    TrustChanges const changes = app_.validators().updateTrustedAndBroadcast(
        app_.getValidations().getCurrentNodeIDs(),
        app_.schemaId(),
        closingInfo.seq,
        mConsensus.consensusType() == ConsensusType::HOTSTUFF
            ? mConsensus.getCurrentTurn()
            : 0,
        app_.peerManager(),
        app_.getHashRouter());

    if (!changes.added.empty() || !changes.removed.empty())
        app_.getValidations().trustChanged(changes.added, changes.removed);

    mConsensus.startRound(
        app_.timeKeeper().closeTime(),
        networkClosed,
        prevLedger,
        changes.removed,
        changes.added);

    const ConsensusPhase currPhase = mConsensus.phase();
    if (mLastConsensusPhase != currPhase)
    {
        reportConsensusStateChange(currPhase);
        mLastConsensusPhase = currPhase;
    }

    JLOG(m_journal.debug()) << "Initiating consensus engine";
    return true;
}

void
NetworkOPsImp::endConsensus()
{
    uint256 deadLedger = m_ledgerMaster.getClosedLedger()->info().parentHash;

    for (auto const& it : app_.peerManager().getActivePeers())
    {
        if (it && (it->getClosedLedgerHash(app_.schemaId()) == deadLedger))
        {
            JLOG(m_journal.trace()) << "Killing obsolete peer status";
            it->cycleStatus(app_.schemaId());
        }
    }

    uint256 networkClosed;
    bool ledgerChange = checkLastClosedLedger(
        app_.peerManager().getActivePeers(), networkClosed);

    if (networkClosed.isZero())
        return;

    // WRITEME: Unless we are in FULL and in the process of doing a consensus,
    // we must count how many nodes share our LCL, how many nodes disagree with
    // our LCL, and how many validations our LCL has. We also want to check
    // timing to make sure there shouldn't be a newer LCL. We need this
    // information to do the next three tests.

    if (((mMode == OperatingMode::CONNECTED) ||
         (mMode == OperatingMode::SYNCING)) &&
        !ledgerChange)
    {
        // Count number of peers that agree with us and UNL nodes whose
        // validations we have for LCL.  If the ledger is good enough, go to
        // TRACKING - TODO
        if (!needNetworkLedger_)
            setMode(OperatingMode::TRACKING);
    }

    if (((mMode == OperatingMode::CONNECTED) ||
         (mMode == OperatingMode::TRACKING)) &&
        !ledgerChange)
    {
        // check if the ledger is good enough to go to FULL
        // Note: Do not go to FULL if we don't have the previous ledger
        // check if the ledger is bad enough to go to CONNECTE  D -- TODO
        auto current = m_ledgerMaster.getCurrentLedger();
        if (app_.timeKeeper().now() < (current->info().parentCloseTime +
                                       2 * current->info().closeTimeResolution))
        {
            setMode(OperatingMode::FULL);
        }
    }

    beginConsensus(networkClosed);
}

inline void
NetworkOPsImp::peerConsensusMessage(
    std::shared_ptr<PeerImp> peer,
    bool isTrusted,
    std::shared_ptr<protocol::TMConsensus> const& m)
{
    if (mConsensus.peerConsensusMessage(peer, isTrusted, m))
    {
        app_.peerManager().relay(*m, consensusMessageUniqueId(*m));
    }
}

void
NetworkOPsImp::peerSyncSchema(
    std::shared_ptr<PeerImp> peer,
    std::shared_ptr<protocol::TMSyncSchema> const& m)
{
    if (m->type() == protocol::TMSyncSchema::ssApplyValidators)
    {
        auto ledger =
            app_.app().getLedgerMaster().getLedgerBySeq(m->ledgerseq());
        if (!ledger)
        {
            JLOG(m_journal.warn())
                << "syncSchema: ledger " << m->ledgerseq() << " not found";
            return;
        }

        AcceptedLedger::pointer aLedger;
        try
        {
            aLedger = app_.app().getAcceptedLedgerCache().fetch(ledger->info().hash);
            if (!aLedger)
            {
                aLedger = std::make_shared<AcceptedLedger>(
                    ledger, app_.app().accountIDCache(), app_.app().logs());
                app_.app().getAcceptedLedgerCache().canonicalize_replace_client(
                    ledger->info().hash, aLedger);
            }
        }
        catch (std::exception const&)
        {
            JLOG(m_journal.warn())
                << "syncSchema: An accepted ledger was missing nodes";
            app_.app().getLedgerMaster().failedSave(
                ledger->info().seq, ledger->info().hash);
            // Clients can now trust the database for information about this
            // ledger sequence.
            app_.app().pendingSaves().finishWork(ledger->info().seq);
            return;
        }

        auto alTx = aLedger->getTxn(m->txindex());
        if (alTx && alTx->getTransactionID() == uint256{m->txhash()} &&
            alTx->getResult() == tesSUCCESS &&
            alTx->getTxnType() == ttSCHEMA_MODIFY &&
            alTx->getTxn()->getFieldH256(sfSchemaID) == app_.schemaId())
        {
            std::shared_ptr<STTx const> stTxn = alTx->getTxn();
            std::vector<PublicKey> validators;
            bool bOperatingSelf = false;

            for (auto& validator : stTxn->getFieldArray(sfValidators))
            {
                auto publicKey =
                    PublicKey(makeSlice(validator.getFieldVL(sfPublicKey)));
                if (app_.getValidationPublicKey() == publicKey)
                    bOperatingSelf = true;
                validators.push_back(publicKey);
            }

            if (bOperatingSelf)
            {
                JLOG(m_journal.info()) << "syncSchema: operating self";
                return;
            }

            auto const disp =
                app_.validators().applySchemaModifyAndBroadcast(
                    m->ledgerseq(),
                    (SchemaModifyOp)stTxn->getFieldU16(sfOpType),
                    m->txindex(),
                    uint256{m->txhash()},
                    validators,
                    *this,
                    app_.schemaId(),
                    app_.peerManager(),
                    app_.getHashRouter());

            switch (disp)
            {
                case ListDisposition::accepted:
                    JLOG(m_journal.info())
                        << "Applied new sync schema from peer "
                        << peer->getRemoteAddress();
                    break;
                case ListDisposition::same_sequence:
                    JLOG(m_journal.warn())
                        << "Sync schema with current sequence from peer "
                        << peer->getRemoteAddress();
                    peer->charge(Resource::feeUnwantedData);
                    break;
                case ListDisposition::stale:
                    JLOG(m_journal.warn()) << "Stale sync schema from peer "
                                           << peer->getRemoteAddress();
                    peer->charge(Resource::feeMediumBurdenPeer);
                    break;
                default:
                    assert(false);
            }
        }
        else
        {
            JLOG(m_journal.warn()) << "syncSchema: transaction check failed";
            peer->charge(Resource::feeHighBurdenPeer);
            return;
        }
    }
    else if (m->type() == protocol::TMSyncSchema::ssUpdateValidators)
    {
        LedgerIndex curSeq = m_ledgerMaster.getCurrentLedgerIndex();
        uint64_t curTurn = mConsensus.getCurrentTurn();

        if (curSeq > m->updateseq() ||
            (curSeq == m->updateseq() && curTurn >= m->updateturn()))
        {
            JLOG(m_journal.info()) << "Update validators on consensus ledger "
                                   << curSeq << " and current turn " << curTurn
                                   << " from peer " << peer->getRemoteAddress();
            app_.validators().updateTrustedAndBroadcast(
                app_.getValidations().getCurrentNodeIDs(),
                app_.schemaId(),
                curSeq,
                curTurn,
                app_.peerManager(),
                app_.getHashRouter());
        }
    }
}

void
NetworkOPsImp::mapComplete(std::shared_ptr<SHAMap> const& map, bool fromAcquire)
{
    // We now have an additional transaction set
    // either created locally during the consensus process
    // or acquired from a peer

    // Inform peers we have this set
    protocol::TMHaveTransactionSet msg;
    msg.set_hash(map->getHash().as_uint256().begin(), 256 / 8);
    msg.set_status(protocol::tsHAVE);
    msg.set_schemaid(app_.schemaId().begin(), uint256::size());
    app_.peerManager().foreach(
        send_always(std::make_shared<Message>(msg, protocol::mtHAVE_SET)));

    // We acquired it because consensus asked us to
    if (fromAcquire)
        mConsensus.gotTxSet(app_.timeKeeper().closeTime(), RCLTxSet{map});
}

void
NetworkOPsImp::consensusViewChange()
{
    if ((mMode == OperatingMode::FULL) || (mMode == OperatingMode::TRACKING))
    {
        setMode(OperatingMode::CONNECTED);
    }
}

void
NetworkOPsImp::pubManifest(Manifest const& mo)
{
    // VFALCO consider std::shared_mutex
    std::lock_guard sl(mSubLock);

    if (!mStreamMaps[sManifests].empty())
    {
        Json::Value jvObj(Json::objectValue);

        jvObj[jss::type] = "manifestReceived";
        jvObj[jss::master_key] = toBase58(TokenType::NodePublic, mo.masterKey);
        if (!mo.signingKey.empty())
            jvObj[jss::signing_key] =
                toBase58(TokenType::NodePublic, mo.signingKey);
        jvObj[jss::seq] = Json::UInt(mo.sequence);
        if (auto sig = mo.getSignature())
            jvObj[jss::signature] = strHex(*sig);
        jvObj[jss::master_signature] = strHex(mo.getMasterSignature());

        for (auto i = mStreamMaps[sManifests].begin();
             i != mStreamMaps[sManifests].end();)
        {
            if (auto p = i->second.lock())
            {
                p->send(jvObj, true);
                ++i;
            }
            else
            {
                i = mStreamMaps[sManifests].erase(i);
            }
        }
    }
}

NetworkOPsImp::ServerFeeSummary::ServerFeeSummary(
    ZXCAmount fee,
    TxQ::Metrics&& escalationMetrics,
    LoadFeeTrack const& loadFeeTrack)
    : loadFactorServer{loadFeeTrack.getLoadFactor()}
    , loadBaseServer{loadFeeTrack.getLoadBase()}
    , baseFee{fee}
    , em{std::move(escalationMetrics)}
{
}

bool
NetworkOPsImp::ServerFeeSummary::operator!=(
    NetworkOPsImp::ServerFeeSummary const& b) const
{
    if (loadFactorServer != b.loadFactorServer ||
        loadBaseServer != b.loadBaseServer || baseFee != b.baseFee ||
        em.is_initialized() != b.em.is_initialized())
        return true;

    if (em && b.em)
    {
        return (
            em->minProcessingFeeLevel != b.em->minProcessingFeeLevel ||
            em->openLedgerFeeLevel != b.em->openLedgerFeeLevel ||
            em->referenceFeeLevel != b.em->referenceFeeLevel);
    }

    return false;
}

// Need to cap to uint64 to uint32 due to JSON limitations
static std::uint32_t
trunc32(std::uint64_t v)
{
    constexpr std::uint64_t max32 = std::numeric_limits<std::uint32_t>::max();

    return std::min(max32, v);
};

void
NetworkOPsImp::pubServer()
{
    // VFALCO TODO Don't hold the lock across calls to send...make a copy of the
    //             list into a local array while holding the lock then release
    //             the lock and call send on everyone.
    //
    std::lock_guard sl(mSubLock);

    if (!mStreamMaps[sServer].empty())
    {
        Json::Value jvObj(Json::objectValue);

        ServerFeeSummary f{
            app_.openLedger().current()->fees().base,
            app_.getTxQ().getMetrics(*app_.openLedger().current()),
            app_.getFeeTrack()};

        jvObj[jss::type] = "serverStatus";
        jvObj[jss::server_status] = strOperatingMode();
        jvObj[jss::load_base] = f.loadBaseServer;
        jvObj[jss::load_factor_server] = f.loadFactorServer;
        jvObj[jss::base_fee] = f.baseFee.jsonClipped();
        jvObj[jss::schema_id] = to_string(app_.schemaId());

        if (f.em)
        {
            auto const loadFactor = std::max(
                safe_cast<std::uint64_t>(f.loadFactorServer),
                mulDiv(
                    f.em->openLedgerFeeLevel,
                    f.loadBaseServer,
                    f.em->referenceFeeLevel)
                    .second);

            jvObj[jss::load_factor] = trunc32(loadFactor);
            jvObj[jss::load_factor_fee_escalation] =
                f.em->openLedgerFeeLevel.jsonClipped();
            jvObj[jss::load_factor_fee_queue] =
                f.em->minProcessingFeeLevel.jsonClipped();
            jvObj[jss::load_factor_fee_reference] =
                f.em->referenceFeeLevel.jsonClipped();
        }
        else
            jvObj[jss::load_factor] = f.loadFactorServer;

        mLastFeeSummary = f;

        for (auto i = mStreamMaps[sServer].begin();
             i != mStreamMaps[sServer].end();)
        {
            InfoSub::pointer p = i->second.lock();

            // VFALCO TODO research the possibility of using thread queues and
            //             linearizing the deletion of subscribers with the
            //             sending of JSON data.
            if (p)
            {
                p->send(jvObj, true);
                ++i;
            }
            else
            {
                i = mStreamMaps[sServer].erase(i);
            }
        }
    }
}

void
NetworkOPsImp::pubConsensus(ConsensusPhase phase)
{
    std::lock_guard sl(mSubLock);

    auto& streamMap = mStreamMaps[sConsensusPhase];
    if (!streamMap.empty())
    {
        Json::Value jvObj(Json::objectValue);
        jvObj[jss::type] = "consensusPhase";
        jvObj[jss::consensus] = to_string(phase);

        for (auto i = streamMap.begin(); i != streamMap.end();)
        {
            if (auto p = i->second.lock())
            {
                p->send(jvObj, true);
                ++i;
            }
            else
            {
                i = streamMap.erase(i);
            }
        }
    }
}

void
NetworkOPsImp::pubValidation(std::shared_ptr<STValidation> const& val)
{
    // VFALCO consider std::shared_mutex
    std::lock_guard sl(mSubLock);

    if (!mStreamMaps[sValidations].empty())
    {
        Json::Value jvObj(Json::objectValue);

        auto const signerPublic = val->getSignerPublic();

        jvObj[jss::type] = "validationReceived";
        jvObj[jss::validation_public_key] =
            toBase58(TokenType::NodePublic, signerPublic);
        jvObj[jss::ledger_hash] = to_string(val->getLedgerHash());
        //jvObj[jss::signature] = strHex(val->getSignature());
        jvObj[jss::full] = val->isFull();
        jvObj[jss::flags] = val->getFlags();
        jvObj[jss::signing_time] = *(*val)[~sfSigningTime];

        auto const masterKey =
            app_.validatorManifests().getMasterKey(signerPublic);

        if (masterKey != signerPublic)
            jvObj[jss::master_key] = toBase58(TokenType::NodePublic, masterKey);

        if (auto const seq = (*val)[~sfLedgerSequence])
            jvObj[jss::ledger_index] = to_string(*seq);

        if (val->isFieldPresent(sfAmendments))
        {
            jvObj[jss::amendments] = Json::Value(Json::arrayValue);
            for (auto const& amendment : val->getFieldV256(sfAmendments))
                jvObj[jss::amendments].append(to_string(amendment));
        }

        if (auto const closeTime = (*val)[~sfCloseTime])
            jvObj[jss::close_time] = *closeTime;

        if (auto const loadFee = (*val)[~sfLoadFee])
            jvObj[jss::load_fee] = *loadFee;

        if (auto const baseFee = (*val)[~sfBaseFee])
            jvObj[jss::base_fee] = static_cast<double>(*baseFee);

        if (auto const dropsPerByte = (*val)[~sfDropsPerByte])
            jvObj[jss::drops_per_byte] = static_cast<double>(*dropsPerByte);

        if (auto const reserveBase = (*val)[~sfReserveBase])
            jvObj[jss::reserve_base] = *reserveBase;

        if (auto const reserveInc = (*val)[~sfReserveIncrement])
            jvObj[jss::reserve_inc] = *reserveInc;

        if (auto const gas_price = (*val)[~sfGasPrice])
            jvObj[jss::gas_price] = static_cast<double>(*gas_price);

        for (auto i = mStreamMaps[sValidations].begin();
             i != mStreamMaps[sValidations].end();)
        {
            if (auto p = i->second.lock())
            {
                p->send(jvObj, true);
                ++i;
            }
            else
            {
                i = mStreamMaps[sValidations].erase(i);
            }
        }
    }
}

void NetworkOPsImp::pubViewChange(uint32_t ledgerSeq, uint64_t view)
{
    ScopedLockType sl(mSubLock);

    if (!mStreamMaps[sViewChange].empty())
    {
        Json::Value jvObj(Json::objectValue);

        jvObj[jss::type] = "viewChange";
        jvObj[jss::ledger_index] = ledgerSeq;
        jvObj[jss::view] = (uint32_t)view;

        for (auto i = mStreamMaps[sViewChange].begin();
            i != mStreamMaps[sViewChange].end(); )
        {
            if (auto p = i->second.lock())
            {
                p->send(jvObj, true);
                ++i;
            }
            else
            {
                i = mStreamMaps[sViewChange].erase(i);
            }
        }
    }
}

void NetworkOPsImp::pubPeerStatus (
    std::function<Json::Value(void)> const& func)
{
    std::lock_guard sl(mSubLock);

    if (!mStreamMaps[sPeerStatus].empty())
    {
        Json::Value jvObj(func());

        jvObj[jss::type] = "peerStatusChange";

        for (auto i = mStreamMaps[sPeerStatus].begin();
             i != mStreamMaps[sPeerStatus].end();)
        {
            InfoSub::pointer p = i->second.lock();

            if (p)
            {
                p->send(jvObj, true);
                ++i;
            }
            else
            {
                i = mStreamMaps[sPeerStatus].erase(i);
            }
        }
    }
}

void
NetworkOPsImp::pubLogs(std::string const& log)
{
    std::lock_guard sl(mSubLock);

    if (!mStreamMaps[sLogs].empty())
    {
        Json::Value jvObj(Json::objectValue);

        jvObj[jss::type] = "log";
        jvObj[jss::log] = log;

        for (auto i = mStreamMaps[sLogs].begin();
             i != mStreamMaps[sLogs].end();)
        {
            InfoSub::pointer p = i->second.lock();

            if (p)
            {
                p->send(jvObj, true);
                ++i;
            }
            else
            {
                i = mStreamMaps[sLogs].erase(i);
            }
        }
    }
}

void
NetworkOPsImp::setMode(OperatingMode om)
{
    using namespace std::chrono_literals;
    if (om == OperatingMode::CONNECTED)
    {
        if (app_.getLedgerMaster().getValidatedLedgerAge() < 1min)
            om = OperatingMode::SYNCING;
    }
    else if (om == OperatingMode::SYNCING)
    {
        if (app_.getLedgerMaster().getValidatedLedgerAge() >= 1min)
            om = OperatingMode::CONNECTED;
    }

    if ((om > OperatingMode::TRACKING) && amendmentBlocked_)
        om = OperatingMode::TRACKING;

    if (mMode == om)
        return;

    mMode = om;

    accounting_.mode(om);

    JLOG(m_journal.info()) << "STATE->" << strOperatingMode();
    pubServer();
}

std::string
NetworkOPsImp::transactionsSQL(
    std::string selection,
    AccountID const& account,
    std::int32_t minLedger,
    std::int32_t maxLedger,
    bool descending,
    std::uint32_t offset,
    int limit,
    bool binary,
    bool count,
    bool bUnlimited)
{
    std::uint32_t NONBINARY_PAGE_LENGTH = 200;
    std::uint32_t BINARY_PAGE_LENGTH = 500;

    std::uint32_t numberOfResults;

    if (count)
    {
        numberOfResults = 1000000000;
    }
    else if (limit < 0)
    {
        numberOfResults = binary ? BINARY_PAGE_LENGTH : NONBINARY_PAGE_LENGTH;
    }
    else if (!bUnlimited)
    {
        numberOfResults = std::min(
            binary ? BINARY_PAGE_LENGTH : NONBINARY_PAGE_LENGTH,
            static_cast<std::uint32_t>(limit));
    }
    else
    {
        numberOfResults = limit;
    }

    std::string maxClause = "";
    std::string minClause = "";

    if (maxLedger != -1)
    {
        maxClause = boost::str(
            boost::format("AND AccountTransactions.LedgerSeq <= '%u'") %
            maxLedger);
    }

    if (minLedger != -1)
    {
        minClause = boost::str(
            boost::format("AND AccountTransactions.LedgerSeq >= '%u'") %
            minLedger);
    }

    std::string sql;

    if (count)
        sql = boost::str(
            boost::format("SELECT %s FROM AccountTransactions "
                          "WHERE Account = '%s' %s %s LIMIT %u, %u;") %
            selection % app_.accountIDCache().toBase58(account) % maxClause %
            minClause % beast::lexicalCastThrow<std::string>(offset) %
            beast::lexicalCastThrow<std::string>(numberOfResults));
    else
        sql = boost::str(
            boost::format(
                "SELECT %s FROM "
                "AccountTransactions INNER JOIN Transactions "
                "ON Transactions.TransID = AccountTransactions.TransID "
                "WHERE Account = '%s' %s %s "
                "ORDER BY AccountTransactions.LedgerSeq %s, "
                "AccountTransactions.TxnSeq %s, AccountTransactions.TransID %s "
                "LIMIT %u, %u;") %
            selection % app_.accountIDCache().toBase58(account) % maxClause %
            minClause % (descending ? "DESC" : "ASC") %
            (descending ? "DESC" : "ASC") % (descending ? "DESC" : "ASC") %
            beast::lexicalCastThrow<std::string>(offset) %
            beast::lexicalCastThrow<std::string>(numberOfResults));
    JLOG(m_journal.trace()) << "txSQL query: " << sql;
    return sql;
}

NetworkOPs::AccountTxs
NetworkOPsImp::getAccountTxs(
    AccountID const& account,
    std::int32_t minLedger,
    std::int32_t maxLedger,
    bool descending,
    std::uint32_t offset,
    int limit,
    bool bUnlimited)
{
    // can be called with no locks
    AccountTxs ret;

    std::string sql = transactionsSQL(
        "AccountTransactions.LedgerSeq,Status,RawTxn,TxnMeta",
        account,
        minLedger,
        maxLedger,
        descending,
        offset,
        limit,
        false,
        false,
        bUnlimited);

    {
        auto db = app_.getTxnDBCHECK().checkoutDbRead();

        boost::optional<std::uint64_t> ledgerSeq;
        boost::optional<std::string> status;
        soci::blob sociTxnBlob(*db), sociTxnMetaBlob(*db);
        soci::indicator rti, tmi;
        Blob rawTxn, txnMeta;

        soci::statement st =
            (db->prepare << sql,
             soci::into(ledgerSeq),
             soci::into(status),
             soci::into(sociTxnBlob, rti),
             soci::into(sociTxnMetaBlob, tmi));

        st.execute();
        while (st.fetch())
        {
            if (soci::i_ok == rti)
                convert(sociTxnBlob, rawTxn);
            else
                rawTxn.clear();

            if (soci::i_ok == tmi)
                convert(sociTxnMetaBlob, txnMeta);
            else
                txnMeta.clear();

            auto txn = Transaction::transactionFromSQL(
                ledgerSeq, status, rawTxn, txnMeta, app_);

            if (txnMeta.empty())
            {  // Work around a bug that could leave the metadata missing
                auto const seq =
                    rangeCheckedCast<std::uint32_t>(ledgerSeq.value_or(0));

                JLOG(m_journal.warn())
                    << "Recovering ledger " << seq << ", txn " << txn->getID();

                if (auto l = m_ledgerMaster.getLedgerBySeq(seq))
                    pendSaveValidated(app_, l, false, false);
            }

            if (txn)
                ret.emplace_back(
                    txn,
                    std::make_shared<TxMeta>(
                        txn->getID(), txn->getLedger(), txnMeta));
        }
    }

    return ret;
}

std::vector<NetworkOPsImp::txnMetaLedgerType>
NetworkOPsImp::getAccountTxsB(
    AccountID const& account,
    std::int32_t minLedger,
    std::int32_t maxLedger,
    bool descending,
    std::uint32_t offset,
    int limit,
    bool bUnlimited)
{
    // can be called with no locks
    std::vector<txnMetaLedgerType> ret;

    std::string sql = transactionsSQL (
        "AccountTransactions.LedgerSeq,AccountTransactions.TransID,Status", account,
        minLedger, maxLedger, descending, offset, limit, true/*binary*/, false,
        bUnlimited);

    {
        auto db = app_.getTxnDBCHECK().checkoutDbRead();

        boost::optional<std::uint64_t> ledgerSeq;
        boost::optional<std::string> transID;
        boost::optional<std::string> status;

        soci::statement st =
                (db->prepare << sql,
                 soci::into(ledgerSeq),
                 soci::into(transID),
                 soci::into(status));
        st.execute ();

        std::map<uint32_t, std::shared_ptr<const ripple::Ledger>> ledgerCache;
        while (st.fetch ())
        {
            auto const seq =
                rangeCheckedCast<std::uint32_t>(ledgerSeq.value_or(0));

            std::shared_ptr<const ripple::Ledger> lgr = nullptr;

            if (ledgerCache.count(seq))
            {
                lgr = ledgerCache[seq];
            }
            else if (lgr = app_.getLedgerMaster().getLedgerBySeq(ledgerSeq.value_or(0)))
            {
                ledgerCache.emplace(seq, lgr);
            }

            if (lgr)
            {
                std::string rawHex;
                std::string metaHex;

                if (getRawMetaHex(*lgr, from_hex_text<uint256>(transID.value()), rawHex, metaHex))
                {
                    ret.emplace_back(rawHex, metaHex, seq);
                }
            }
        }
    }

    return ret;
}

NetworkOPsImp::AccountTxs
NetworkOPsImp::getTxsAccount(
    AccountID const& account,
    std::int32_t minLedger,
    std::int32_t maxLedger,
    bool forward,
    std::optional<AccountTxMarker>& marker,
    int limit,
    bool bUnlimited)
{
    static std::uint32_t const page_length(200);

    Schema& app = app_;
    NetworkOPsImp::AccountTxs ret;

    auto bound = [&ret, &app](
                     std::uint32_t ledger_index,
                     Blob&& rawTxn,
                     Blob&& rawMeta) {
        convertBlobsToTxResult(ret, ledger_index, rawTxn, rawMeta, app);
    };

    if (app_.config().SAVE_TX_RAW)
    {
        accountTxPageSQL(
            app_,
            app_.getTxnDBCHECK().connRead(),
            app_.accountIDCache(),
            std::bind(saveLedgerAsync, std::ref(app_), std::placeholders::_1),
            bound,
            account,
            minLedger,
            maxLedger,
            forward,
            marker,
            limit,
            bUnlimited,
            page_length);
    }
    else
    {
        accountTxPage(
            app_,
            app_.getTxnDBCHECK().connRead(),
            app_.accountIDCache(),
            std::bind(saveLedgerAsync, std::ref(app_), std::placeholders::_1),
            bound,
            account,
            minLedger,
            maxLedger,
            forward,
            marker,
            limit,
            bUnlimited,
            page_length);
    }
    

    return ret;
}

NetworkOPsImp::MetaTxsList
NetworkOPsImp::getTxsAccountB(
    AccountID const& account,
    std::int32_t minLedger,
    std::int32_t maxLedger,
    bool forward,
    std::optional<AccountTxMarker>& marker,
    int limit,
    bool bUnlimited)
{
    static const std::uint32_t page_length(500);

    MetaTxsList ret;

    auto bound = [&ret](
                     std::uint32_t ledgerIndex,
                     Blob&& rawTxn,
                     Blob&& rawMeta) {
        ret.emplace_back(strHex(rawTxn), strHex(rawMeta), ledgerIndex);
    };

    if (app_.config().SAVE_TX_RAW)
    {
        accountTxPageSQL(
            app_,
            app_.getTxnDBCHECK().connRead(),
            app_.accountIDCache(),
            std::bind(saveLedgerAsync, std::ref(app_), std::placeholders::_1),
            bound,
            account,
            minLedger,
            maxLedger,
            forward,
            marker,
            limit,
            bUnlimited,
            page_length);
    }
    else
    {
        accountTxPage(
            app_,
            app_.getTxnDBCHECK().connRead(),
            app_.accountIDCache(),
            std::bind(saveLedgerAsync, std::ref(app_), std::placeholders::_1),
            bound,
            account,
            minLedger,
            maxLedger,
            forward,
            marker,
            limit,
            bUnlimited,
            page_length);
    }

    return ret;
}

inline std::recursive_mutex&
NetworkOPsImp::peekConsensusMutex()
{
    return mConsensus.peekMutex();
}

inline ConsensusParms const&
NetworkOPsImp::getConsensusParms()
{
    return mConsensus.parms();
}

inline Json::Value
NetworkOPsImp::getConsensusInfo(bool full)
{
    return mConsensus.getJson(full);
}

Json::Value
NetworkOPsImp::getServerInfo(bool human, bool admin, bool counters)
{
    Json::Value info = Json::objectValue;

    // System-level warnings
    {
        Json::Value warnings{Json::arrayValue};
        if (isAmendmentBlocked())
        {
            Json::Value& w = warnings.append(Json::objectValue);
            w[jss::id] = warnRPC_AMENDMENT_BLOCKED;
            w[jss::message] =
                "This server is amendment blocked, and must be updated to be "
                "able to stay in sync with the network.";
        }
        if (admin && isAmendmentWarned())
        {
            Json::Value& w = warnings.append(Json::objectValue);
            w[jss::id] = warnRPC_UNSUPPORTED_MAJORITY;
            w[jss::message] =
                "One or more unsupported amendments have reached majority. "
                "Upgrade to the latest version before they are activated "
                "to avoid being amendment blocked.";
            if (auto const expected =
                    app_.getAmendmentTable().firstUnsupportedExpected())
            {
                auto& d = w[jss::details] = Json::objectValue;
                d[jss::expected_date] = expected->time_since_epoch().count();
                d[jss::expected_date_UTC] = to_string(*expected);
            }
        }

        if (warnings.size())
            info[jss::warnings] = std::move(warnings);
    }

    // hostid: unique string describing the machine
    if (human)
        info[jss::hostid] = getHostId(admin);

    if (auto const netid = app_.app().overlay().networkID())
        info[jss::network_id] = static_cast<Json::UInt>(*netid);

    info[jss::build_version] = BuildInfo::getVersionString();

    info[jss::server_state] = strOperatingMode(admin);

    info[jss::time] = to_string(date::floor<std::chrono::microseconds>(
        std::chrono::system_clock::now()));

    info [jss::node_alg_type] = CommonKey::getAlgTypeStr();
    info [jss::hash_type]     = CommonKey::getHashTypeStr();

    if (needNetworkLedger_)
        info[jss::network_ledger] = "waiting";

    info[jss::validation_quorum] =
        static_cast<Json::UInt>(app_.validators().quorum());

    if (admin)
    {
        auto when = app_.validators().expires();

        if (!human)
        {
            if (when)
                info[jss::validator_list_expires] =
                    safe_cast<Json::UInt>(when->time_since_epoch().count());
            else
                info[jss::validator_list_expires] = 0;
        }
        else
        {
            auto& x = (info[jss::validator_list] = Json::objectValue);

            x[jss::count] = static_cast<Json::UInt>(app_.validators().count());

            if (when)
            {
                if (*when == TimeKeeper::time_point::max())
                {
                    //x[jss::expiration] = "never";
                    x[jss::status] = "active";
                }
                else
                {
                    //x[jss::expiration] = to_string(*when);

                    if (*when > app_.timeKeeper().now())
                        x[jss::status] = "active";
                    else
                        x[jss::status] = "expired";
                }
            }
            else
            {
                x[jss::status] = "unknown";
                x[jss::expiration] = "unknown";
            }
        }
    }
    info[jss::io_latency_ms] =
        static_cast<Json::UInt>(app_.app().getIOLatency().count());

    if (admin)
    {
        if (!app_.getValidationPublicKey().empty() &&
           (app_.schemaId() != beast::zero || !app_.config().ONLY_VALIDATE_FOR_SCHEMA))
        {
            info[jss::pubkey_validator] = toBase58(
                TokenType::NodePublic, app_.validators().localPublicKey());
        }
        else
        {
            info[jss::pubkey_validator] = "none";
        }
    }

    if (counters)
    {
        info[jss::counters] = app_.getPerfLog().countersJson();
        info[jss::current_activities] = app_.getPerfLog().currentJson();
    }

    info[jss::pubkey_node] =
        toBase58(TokenType::NodePublic, app_.nodeIdentity().first);

    info[jss::complete_ledgers] = app_.getLedgerMaster().getCompleteLedgers();

    if (amendmentBlocked_)
        info[jss::amendment_blocked] = true;

    auto const fp = m_ledgerMaster.getFetchPackCacheSize();

    if (fp != 0)
        info[jss::fetch_pack] = Json::UInt(fp);

    info[jss::peers] = Json::UInt(app_.peerManager().size());

    Json::Value lastClose = Json::objectValue;
    lastClose[jss::proposers] = Json::UInt(mConsensus.prevProposers());

    if (human)
    {
        lastClose[jss::converge_time_s] =
            std::chrono::duration<double>{mConsensus.prevRoundTime()}.count();
    }
    else
    {
        lastClose[jss::converge_time] =
            Json::Int(mConsensus.prevRoundTime().count());
    }

    info[jss::last_close] = lastClose;

    //  info[jss::consensus] = mConsensus.getJson();

    if (admin)
        info[jss::load] = m_job_queue.getJson();

    auto const escalationMetrics =
        app_.getTxQ().getMetrics(*app_.openLedger().current());

    auto const loadFactorServer = app_.getFeeTrack().getLoadFactor();
    auto const loadBaseServer = app_.getFeeTrack().getLoadBase();
    /* Scale the escalated fee level to unitless "load factor".
       In practice, this just strips the units, but it will continue
       to work correctly if either base value ever changes. */
    auto const loadFactorFeeEscalation =
        mulDiv(
            escalationMetrics.openLedgerFeeLevel,
            loadBaseServer,
            escalationMetrics.referenceFeeLevel)
            .second;

    auto const loadFactor = std::max(
        safe_cast<std::uint64_t>(loadFactorServer), loadFactorFeeEscalation);

    if (!human)
    {
        info[jss::load_base] = loadBaseServer;
        info[jss::load_factor] = trunc32(loadFactor);
        info[jss::load_factor_server] = loadFactorServer;

        /* Json::Value doesn't support uint64, so clamp to max
            uint32 value. This is mostly theoretical, since there
            probably isn't enough extant ZXC to drive the factor
            that high.
        */
        info[jss::load_factor_fee_escalation] =
            escalationMetrics.openLedgerFeeLevel.jsonClipped();
        info[jss::load_factor_fee_queue] =
            escalationMetrics.minProcessingFeeLevel.jsonClipped();
        info[jss::load_factor_fee_reference] =
            escalationMetrics.referenceFeeLevel.jsonClipped();
    }
    else
    {
        info[jss::load_factor] =
            static_cast<double>(loadFactor) / loadBaseServer;

        if (loadFactorServer != loadFactor)
            info[jss::load_factor_server] =
                static_cast<double>(loadFactorServer) / loadBaseServer;

        if (admin)
        {
            std::uint32_t fee = app_.getFeeTrack().getLocalFee();
            if (fee != loadBaseServer)
                info[jss::load_factor_local] =
                    static_cast<double>(fee) / loadBaseServer;
            fee = app_.getFeeTrack().getRemoteFee();
            if (fee != loadBaseServer)
                info[jss::load_factor_net] =
                    static_cast<double>(fee) / loadBaseServer;
            fee = app_.getFeeTrack().getClusterFee();
            if (fee != loadBaseServer)
                info[jss::load_factor_cluster] =
                    static_cast<double>(fee) / loadBaseServer;
        }
        if (escalationMetrics.openLedgerFeeLevel !=
                escalationMetrics.referenceFeeLevel &&
            (admin || loadFactorFeeEscalation != loadFactor))
            info[jss::load_factor_fee_escalation] =
                escalationMetrics.openLedgerFeeLevel.decimalFromReference(
                    escalationMetrics.referenceFeeLevel);
        if (escalationMetrics.minProcessingFeeLevel !=
            escalationMetrics.referenceFeeLevel)
            info[jss::load_factor_fee_queue] =
                escalationMetrics.minProcessingFeeLevel.decimalFromReference(
                    escalationMetrics.referenceFeeLevel);
    }

    bool valid = false;
    auto lpClosed = m_ledgerMaster.getValidatedLedger();

    if (lpClosed)
        valid = true;
    else
        lpClosed = m_ledgerMaster.getClosedLedger();

    if (lpClosed)
    {
        ZXCAmount const baseFee = lpClosed->fees().base;
        Json::Value l(Json::objectValue);
        l[jss::seq] = Json::UInt(lpClosed->info().seq);
        l[jss::hash] = to_string(lpClosed->info().hash);

        std::uint64_t drops_per_byte = lpClosed->fees().drops_per_byte;
        std::uint64_t gas_price = lpClosed->fees().gas_price;

        if (!human)
        {
            l[jss::base_fee] = baseFee.jsonClipped();
            l[jss::reserve_base] =
                lpClosed->fees().accountReserve(0).jsonClipped();
            l[jss::reserve_inc] = lpClosed->fees().increment.jsonClipped();
            l[jss::close_time] = Json::Value::UInt(
                lpClosed->info().closeTime.time_since_epoch().count());
        }
        else
        {
            l[jss::drops_per_byte] = Json::Value::UInt(drops_per_byte);
            l[jss::gas_price] = Json::Value::UInt(gas_price);
            l[jss::base_fee_zxc] = baseFee.decimalZXC();
            l[jss::reserve_base_zxc] =
                lpClosed->fees().accountReserve(0).decimalZXC();
            l[jss::reserve_inc_zxc] = lpClosed->fees().increment.decimalZXC();

            auto const nowOffset = app_.timeKeeper().nowOffset();
            if (std::abs(nowOffset.count()) >= 60)
                l[jss::system_time_offset] = nowOffset.count();

            auto const closeOffset = app_.timeKeeper().closeOffset();
            if (std::abs(closeOffset.count()) >= 60)
                l[jss::close_time_offset] = closeOffset.count();

            constexpr std::chrono::seconds highAgeThreshold{1000000};
            if (m_ledgerMaster.haveValidated())
            {
                auto const age = m_ledgerMaster.getValidatedLedgerAge();
                l[jss::age] =
                    Json::UInt(age < highAgeThreshold ? age.count() : 0);
            }
            else
            {
                auto lCloseTime = lpClosed->info().closeTime;
                auto closeTime = app_.timeKeeper().closeTime();
                if (lCloseTime <= closeTime)
                {
                    using namespace std::chrono_literals;
                    auto age = closeTime - lCloseTime;
                    l[jss::age] =
                        Json::UInt(age < highAgeThreshold ? age.count() : 0);
                }
            }
        }

        if (valid)
            info[jss::validated_ledger] = l;
        else
            info[jss::closed_ledger] = l;

        auto lpPublished = m_ledgerMaster.getPublishedLedger();
        if (!lpPublished)
            info[jss::published_ledger] = "none";
        else if (lpPublished->info().seq != lpClosed->info().seq)
            info[jss::published_ledger] = lpPublished->info().seq;
    }

    std::tie(info[jss::state_accounting], info[jss::server_state_duration_us]) =
        accounting_.json();
    info[jss::uptime] = UptimeClock::now().time_since_epoch().count();
    info[jss::jq_trans_overflow] =
        std::to_string(app_.app().overlay().getJqTransOverflow());
    info[jss::peer_disconnects] =
        std::to_string(app_.app().overlay().getPeerDisconnect());
    info[jss::peer_disconnects_resources] =
        std::to_string(app_.app().overlay().getPeerDisconnectCharges());

    // comprehensive judgement for server_status
    info[jss::server_status] = getServerStatus();

    return info;
}

std::string
NetworkOPsImp::getServerStatus()
{
    if (mConsensus.waitingForInit())
    {
        return "abnormal";
    }

    // Time out in milliseconds
    auto timeout = std::chrono::milliseconds(std::numeric_limits<
                  Json::Value::Int>::max());
    if (mConsensus.getConsensusTimeOut().count() > 0)
        timeout = 2 * mConsensus.getConsensusTimeOut();
    

    bool consensusValid = m_ledgerMaster.getValidatedLedgerAge() < timeout;
    auto mode = mConsensus.mode();
    auto phase = mConsensus.phase();

    if ((consensusValid ||
        phase == ConsensusPhase::accepted) &&
        (mode == ConsensusMode::proposing ||
         mode == ConsensusMode::switchedLedger ||
         (mode == ConsensusMode::observing && !mConsensus.validating())))
    {
        return "normal";
    }
    else
    {
        return "abnormal";
    }
}

bool NetworkOPsImp::waitingForInit()
{
    return mConsensus.waitingForInit();
}

void
NetworkOPsImp::clearLedgerFetch()
{
    app_.getInboundLedgers().clearFailures();
}

Json::Value
NetworkOPsImp::getLedgerFetchInfo()
{
    return app_.getInboundLedgers().getInfo();
}

bool
NetworkOPsImp::checkLedgerAccept(std::shared_ptr<Ledger const> const& ledger)
{
    return mConsensus.checkLedgerAccept(ledger);
}

void
NetworkOPsImp::pubProposedTransaction(
    std::shared_ptr<ReadView const> const& lpCurrent,
    std::shared_ptr<STTx const> const& stTxn,
    STer terResult)
{
    Json::Value jvObj = transJson(*stTxn, terResult.ter, false, lpCurrent);

    {
        std::lock_guard sl(mSubLock);

        auto it = mStreamMaps[sRTTransactions].begin();
        while (it != mStreamMaps[sRTTransactions].end())
        {
            InfoSub::pointer p = it->second.lock();

            if (p)
            {
                p->send(jvObj, true);
                ++it;
            }
            else
            {
                it = mStreamMaps[sRTTransactions].erase(it);
            }
        }
    }
    AcceptedLedgerTx alt(
        lpCurrent, stTxn, terResult.ter, terResult.msg, app_.accountIDCache(), app_.logs());
    JLOG(m_journal.trace()) << "pubProposed: " << alt.getJson();
    pubAccountTransaction(lpCurrent, alt, false);
}

void
NetworkOPsImp::pubLedger(std::shared_ptr<ReadView const> const& lpAccepted)
{
    // Ledgers are published only when they acquire sufficient validations
    // Holes are filled across connection loss or other catastrophe

    std::shared_ptr<AcceptedLedger> alpAccepted =
        app_.getAcceptedLedgerCache().fetch(lpAccepted->info().hash);
    if (!alpAccepted)
    {
        alpAccepted = std::make_shared<AcceptedLedger>(
            lpAccepted, app_.accountIDCache(), app_.logs());
        app_.getAcceptedLedgerCache().canonicalize_replace_client(
            lpAccepted->info().hash, alpAccepted);
    }

    {
        std::lock_guard sl(mSubLock);

        if (!mStreamMaps[sLedger].empty())
        {
            Json::Value jvObj(Json::objectValue);

            jvObj[jss::type] = "ledgerClosed";
            jvObj[jss::ledger_index] = lpAccepted->info().seq;
            jvObj[jss::ledger_hash] = to_string(lpAccepted->info().hash);
            jvObj[jss::ledger_time] = Json::Value::UInt(
                lpAccepted->info().closeTime.time_since_epoch().count());

            jvObj[jss::fee_ref] = lpAccepted->fees().units.jsonClipped();
            jvObj[jss::fee_base] = lpAccepted->fees().base.jsonClipped();
            jvObj[jss::drops_per_byte] =
                Json::UInt(lpAccepted->fees().drops_per_byte);
            jvObj[jss::gas_price] = Json::UInt(lpAccepted->fees().gas_price);
            jvObj[jss::reserve_base] =
                lpAccepted->fees().accountReserve(0).jsonClipped();
            jvObj[jss::reserve_inc] =
                lpAccepted->fees().increment.jsonClipped();

            jvObj[jss::txn_count] = Json::UInt(alpAccepted->getTxnCount());
            jvObj[jss::schema_id] = to_string(app_.schemaId());

            int iSuccess = 0;
            int iFailure = 0;
            std::map<AccountID, int> mapFailureCount;

            //
            for (auto const& vt : alpAccepted->getMap())
            {
                if (vt.second->getResult() == tesSUCCESS)
                    iSuccess++;
                else
                {
                    iFailure++;
                    if (app_.config().OPEN_ACCOUNT_DELAY)
                        mapFailureCount[vt.second->getTxn()->getAccountID(
                            sfAccount)]++;
                }
            }

            jvObj[jss::txn_success] = Json::UInt(iSuccess);
            jvObj[jss::txn_failure] = Json::UInt(iFailure);

            if (app_.config().OPEN_ACCOUNT_DELAY)
                processAccountDelay(mapFailureCount);

            if (mMode >= OperatingMode::SYNCING)
            {
                jvObj[jss::validated_ledgers] =
                    app_.getLedgerMaster().getCompleteLedgers();
            }

            auto it = mStreamMaps[sLedger].begin();
            while (it != mStreamMaps[sLedger].end())
            {
                InfoSub::pointer p = it->second.lock();
                if (p)
                {
                    p->send(jvObj, true);
                    ++it;
                }
                else
                {
                    auto id = std::string("#") + std::to_string(it->first) + " ";
                    JLOG(m_journal.info()) << "InfoSub" << id << "destructed, unsubLegder";
                    it = mStreamMaps[sLedger].erase(it);
                }
            }
        }
        else if (app_.config().OPEN_ACCOUNT_DELAY)
        {
            std::map<AccountID, int> mapFailureCount;
            for (auto const& vt : alpAccepted->getMap())
            {
                if (vt.second->getResult() != tesSUCCESS)
                    mapFailureCount[vt.second->getTxn()->getAccountID(
                        sfAccount)]++;
            }
            processAccountDelay(mapFailureCount);
        }
    }

    if (!mStreamMaps[sTransactions].empty() ||
        !mStreamMaps[sRTTransactions].empty() || !mSubAccount.empty() ||
        !mSubRTAccount.empty() || !mSubTable.empty() || !mSubTx.empty() ||
        !mValidatedSubTx.empty() || app_.getOrderBookDB().hasListener())
    {
        auto timeStart = utcTime();
        // Don't lock since pubAcceptedTransaction is locking.
        for (auto const& [_, accTx] : alpAccepted->getMap())
        {
            boost::ignore_unused(_);
            JLOG(m_journal.trace()) << "pubAccepted: " << accTx->getJson();
            pubValidatedTransaction(lpAccepted, *accTx);
        }
        JLOG(m_journal.info())
            << "pub all Txs, time used: " << utcTime() - timeStart << "ms";
    }

    // Check schema txs in main schema
    if (app_.schemaId() == beast::zero)
    {
        for (auto const& vt : alpAccepted->getMap())
        {
            checkSchemaTx(lpAccepted, *vt.second);
        }
    }
}

void
NetworkOPsImp::reportFeeChange()
{
    ServerFeeSummary f{app_.openLedger().current()->fees().base,
                       app_.getTxQ().getMetrics(*app_.openLedger().current()),
                       app_.getFeeTrack()};

    // only schedule the job if something has changed
    if (f != mLastFeeSummary)
    {
        m_job_queue.addJob(
            jtCLIENT, "reportFeeChange->pubServer", [this](Job&) {
                pubServer();
            }, app_.doJobCounter());
    }
}

void
NetworkOPsImp::reportConsensusStateChange(ConsensusPhase phase)
{
    m_job_queue.addJob(
        jtCLIENT,
        "reportConsensusStateChange->pubConsensus",
        [this, phase](Job&) { pubConsensus(phase); }, app_.doJobCounter());
}

// This routine should only be used to publish accepted or validated
// transactions.
Json::Value
NetworkOPsImp::transJson(
    const STTx& stTxn,
    TER terResult,
    bool bValidated,
    std::shared_ptr<ReadView const> const& lpCurrent)
{
    Json::Value jvObj(Json::objectValue);
    std::string sToken;
    std::string sHuman;

    transResultInfo(terResult, sToken, sHuman);

    jvObj[jss::type] = "transaction";
    jvObj[jss::transaction] = stTxn.getJson(JsonOptions::none);

    if (bValidated)
    {
        jvObj[jss::ledger_index] = lpCurrent->info().seq;
        jvObj[jss::ledger_hash] = to_string(lpCurrent->info().hash);
        jvObj[jss::transaction][jss::date] =
            lpCurrent->info().closeTime.time_since_epoch().count();
        jvObj[jss::validated] = true;

        // WRITEME: Put the account next seq here
    }
    else
    {
        jvObj[jss::validated] = false;
        jvObj[jss::ledger_current_index] = lpCurrent->info().seq;
    }

    jvObj[jss::status] = bValidated ? "closed" : "proposed";
    jvObj[jss::engine_result] = sToken;
    jvObj[jss::engine_result_code] = terResult;
    jvObj[jss::engine_result_message] = sHuman;

    if (stTxn.getTxnType() == ttOFFER_CREATE)
    {
        auto const account = stTxn.getAccountID(sfAccount);
        auto const amount = stTxn.getFieldAmount(sfTakerGets);

        // If the offer create is not self funded then add the owner balance
        if (account != amount.issue().account)
        {
            auto const ownerFunds = accountFunds(
                *lpCurrent,
                account,
                amount,
                fhIGNORE_FREEZE,
                app_.journal("View"));
            jvObj[jss::transaction][jss::owner_funds] = ownerFunds.getText();
        }
    }

    return jvObj;
}

std::pair<bool, std::string>
NetworkOPsImp::createSchema(
    const std::shared_ptr<SLE const>& sleSchema,
    bool bForce, bool bFromLoad/* = false*/)
{
    SchemaParams params{};
    params.readFromSle(sleSchema);
    params.fromLoad = bFromLoad;

    bool bShouldCreate = bForce;
    if (!bShouldCreate)
    {
        for (auto& validator : params.validator_list)
        {
            if ((validator.second || app_.config().AUTO_ACCEPT_NEW_SCHEMA) &&
                app_.app().getValidationPublicKey().size() != 0 &&
                validator.first == app_.app().getValidationPublicKey())
            {
                bShouldCreate = true;
            }
        }
    }

    if (!bShouldCreate)
        return std::make_pair(true, "");

    if (!app_.getSchemaManager().contains(params.schema_id))
    {
        try
        {
            auto newSchema = app_.getSchemaManager().createSchema(
                app_.app().config(), params);
            if (newSchema)
            {
                if (!newSchema->initBeforeSetup())
                {
                    app_.getSchemaManager().removeSchema(newSchema->schemaId());
                    return std::make_pair(false, "Error while initBeforeSetup");
                }
                if (!newSchema->setup())
                {
                    app_.getSchemaManager().removeSchema(newSchema->schemaId());
                    return std::make_pair(false, "Error while setup");
                }
                app_.app().overlay().onSchemaCreated(params.schema_id);
            }
        }
        catch (std::exception const& e)
        {
            return std::make_pair(false, e.what());
        }
    }

    return std::make_pair(true, "");
}

void
NetworkOPsImp::resetSchemaCfg(
    uint256 schemaID,
    std::shared_ptr<SLE const> sleSchema)
{
    auto config_dir = boost::filesystem::path(app_.config().SCHEMA_PATH) /
        to_string(schemaID);
    app_.app().config(schemaID).initSchemaInfo(
        config_dir, app_.app().getSchema(schemaID).getSchemaParams());

    std::vector<std::string> validator_list;
    auto validators = sleSchema->getFieldArray(sfValidators);
    for (auto& validator : validators)
    {
        auto publicKey =
            PublicKey(makeSlice(validator.getFieldVL(sfPublicKey)));
        validator_list.push_back(toBase58(TokenType::NodePublic, publicKey));
    }

    std::vector<std::string> peer_list;
    auto peers = sleSchema->getFieldArray(sfPeerList);
    for (auto& peer : peers)
    {
        peer_list.push_back(strCopy(peer.getFieldVL(sfEndpoint)));
    }
    Config config;
    config.onSchemaModify(
        app_.app().config(schemaID), validator_list, peer_list);
}

void
NetworkOPsImp::checkSchemaTx(
    std::shared_ptr<ReadView const> const& alAccepted,
    const AcceptedLedgerTx& alTx)
{
    if (alTx.getResult() != tesSUCCESS)
        return;
    std::shared_ptr<STTx const> stTxn = alTx.getTxn();
    if (stTxn->getTxnType() == ttSCHEMA_CREATE)
    {
        auto const account = stTxn->getAccountID(sfAccount);
        auto const sle = alAccepted->read(keylet::account(account));
        auto sleKey = keylet::schema(
            account, (*sle)[sfSequence] - 1, alAccepted->info().parentHash);
        auto const sleSchema = alAccepted->read(sleKey);

        auto ret = this->createSchema(sleSchema, false);
        if (!ret.first)
        {
            JLOG(m_journal.fatal()) << ret.second;
        }
    }
    else if (stTxn->getTxnType() == ttSCHEMA_MODIFY)
    {
        auto schemaID = stTxn->getFieldH256(sfSchemaID);

        auto validators = stTxn->getFieldArray(sfValidators);
        std::vector<PublicKey> vecValidators;
        bool bOperatingSelf = false;
        for (auto& validator : validators)
        {
            auto publicKey =
                PublicKey(makeSlice(validator.getFieldVL(sfPublicKey)));
            vecValidators.push_back(publicKey);
            if (app_.app().getValidationPublicKey() == publicKey)
                bOperatingSelf = true;
        }

        if (!app_.app().hasSchema(schemaID))
        {
            if (stTxn->getFieldU16(sfOpType) == (uint8_t)SchemaModifyOp::del ||
                (stTxn->getFieldU16(sfOpType) == (uint8_t)SchemaModifyOp::add &&
                 !bOperatingSelf))
            {
                return;
            }
        }

        //update validators list
        if (!bOperatingSelf)
        {
            //Since the code is called by doAdvance,in case different node on subchain
            //execute the 'modify' in different ledger,and result in different validator
            //for subchain nodes and consensus cannot reach. Here we add a mechanism:
            //when the first node executing the code,it tells other peer nodes to execute,
            //since the ledger have reached consensus,most of them have this tx,so can execute
            //immediately!
            app_.app()
                .validators(schemaID)
                .applySchemaModifyAndBroadcast(
                    alAccepted->seq(),
                    (SchemaModifyOp)stTxn->getFieldU16(sfOpType),
                    alTx.getIndex(),
                    stTxn->getTransactionID(),
                    vecValidators,
                    app_.app().getOPs(schemaID),
                    schemaID,
                    app_.app().peerManager(schemaID),
                    app_.app().getHashRouter(schemaID));
        }

        std::vector<std::string> vecPeers;
        auto& peers = stTxn->getFieldArray(sfPeerList);
        for (auto& peer : peers)
        {
            vecPeers.push_back(strCopy(peer.getFieldVL(sfEndpoint)));
        }

        auto sleSchema = alAccepted->read(Keylet(ltSCHEMA, schemaID));
        if (stTxn->getFieldU16(sfOpType) == (uint8_t)SchemaModifyOp::add)
        {
            if (bOperatingSelf)
            {
                auto ret = this->createSchema(sleSchema, false);
                if (!ret.first)
                {
                    JLOG(m_journal.fatal()) << ret.second;
                }
            }
            else
            {
                app_.app().getSchema(schemaID).getSchemaParams().modify(
                    SchemaModifyOp::add, vecValidators, vecPeers);
                resetSchemaCfg(schemaID, sleSchema);

                app_.app().overlay().onSchemaAddPeer(
                    schemaID, vecPeers, vecValidators);
            }
        }
        else if (stTxn->getFieldU16(sfOpType) == (uint8_t)SchemaModifyOp::del)
        {
            app_.app().getSchema(schemaID).getSchemaParams().modify(
                SchemaModifyOp::del, vecValidators, vecPeers);
            resetSchemaCfg(schemaID, sleSchema);

            if (bOperatingSelf)
            {
                app_.app().getJobQueue().addJob(
                            jtSTOP_SCHEMA,
                            "StopSchema",
                            [this, schemaID](Job&) {
                                app_.app().doStopSchema(schemaID);
                            });
                //app_.app().getSchema(schemaID).doStop();
                //if (app_.app().getSchema(schemaID).isShutdown())
                  //  app_.getSchemaManager().removeSchema(schemaID);
            }
            else
                app_.app().peerManager(schemaID).remove(vecValidators);
        }
    }
    else if (stTxn->getTxnType() == ttSCHEMA_DELETE)
    {
        auto schemaID = stTxn->getFieldH256(sfSchemaID);

        if (app_.app().hasSchema(schemaID))
        {
            app_.app().getJobQueue().addJob(
                            jtSTOP_SCHEMA,
                            "StopSchema",
                            [this, schemaID](Job&) {
                                app_.app().doStopSchema(schemaID);
                            });
        }
    }
}

void
NetworkOPsImp::pubValidatedTransaction(
    std::shared_ptr<ReadView const> const& alAccepted,
    const AcceptedLedgerTx& alTx)
{
    std::shared_ptr<STTx const> stTxn = alTx.getTxn();
    Json::Value jvObj = transJson(*stTxn, alTx.getResult(), true, alAccepted);

    if (auto const txMeta = alTx.getMeta())
    {
        jvObj[jss::meta] = txMeta->getJson(JsonOptions::none);
        RPC::insertDeliveredAmount(
            jvObj[jss::meta], *alAccepted, stTxn, *txMeta);
    }

    {
        std::lock_guard sl(mSubLock);

        auto it = mStreamMaps[sTransactions].begin();
        while (it != mStreamMaps[sTransactions].end())
        {
            InfoSub::pointer p = it->second.lock();

            if (p)
            {
                p->send(jvObj, true);
                ++it;
            }
            else
                it = mStreamMaps[sTransactions].erase(it);
        }

        it = mStreamMaps[sRTTransactions].begin();

        while (it != mStreamMaps[sRTTransactions].end())
        {
            InfoSub::pointer p = it->second.lock();

            if (p)
            {
                p->send(jvObj, true);
                ++it;
            }
            else
                it = mStreamMaps[sRTTransactions].erase(it);
        }
    }

    if (app_.getOrderBookDB().hasListener())
    {
        app_.getOrderBookDB().processTxn(alAccepted, alTx, jvObj);
    }

    if (!mSubAccount.empty() || !mSubRTAccount.empty())
    {
        // if (alTx.getResult() == tesSUCCESS)
        pubAccountTransaction(alAccepted, alTx, true);
    }

    if (!mSubTable.empty() || !mSubTx.empty() || !mValidatedSubTx.empty())
    {
        PubValidatedTxForTable(alTx);
    }
}

std::tuple<std::string, std::string, std::string>
NetworkOPsImp::get_res(TER ter, std::string const& contracDetailMsg)
{
    if (ter == tesSUCCESS)
        return std::make_tuple((std::string)jss::validate_success, "", "");
    if (!contracDetailMsg.empty())
        return std::make_tuple(std::string(jss::validate_error), 
            contracDetailMsg, contracDetailMsg);
    return std::make_tuple(
        std::string(jss::validate_error), transToken(ter), transHuman(ter));
}

void
NetworkOPsImp::PubValidatedTxForTable(const AcceptedLedgerTx& alTx)
{
    auto tx = *alTx.getTxn();
    auto res = get_res(alTx.getResult(), alTx.getContractDetailMsg());

    auto ledger = app_.getLedgerMaster().getPublishedLedger();
    auto vecTxs = app_.getMasterTransaction().getTxs(tx, "", ledger, 0);
    if (vecTxs.size() > 1)
    {
        std::list<std::pair<AccountID, std::string>> listPair;
        for (auto& tx : vecTxs)
        {
            if (tx.isFieldPresent(sfTables))
            {
                auto const& sTxTables = tx.getFieldArray(sfTables);
                Blob vTableName = sTxTables[0].getFieldVL(sfTableName);
                auto sTableName = strCopy(vTableName);

                AccountID owner = beast::zero;
                if (tx.isFieldPresent(sfOwner))
                    owner = tx.getAccountID(sfOwner);
                else if (tx.isFieldPresent(sfAccount))
                    owner = tx.getAccountID(sfAccount);

                auto it = std::find_if(
                    listPair.begin(),
                    listPair.end(),
                    [&sTableName,
                     &owner](std::pair<AccountID, std::string>& item) {
                        return owner == item.first && sTableName == item.second;
                    });

                if (it == listPair.end())
                    listPair.push_back(std::make_pair(owner, sTableName));
            }
        }
        for (auto item : listPair)
        {
            pubChainSqlTableTxs(item.first, item.second, tx, res);
        }
        if (listPair.size() > 0)
        {
            pubTxResult(tx, res, true, true);
        }
    }
    else if (vecTxs.size() == 1)
    {
        auto txFinal = vecTxs[0];
        if (txFinal.isFieldPresent(sfTables))
        {
            auto const& sTxTables = txFinal.getFieldArray(sfTables);
            auto sTableName = strCopy(sTxTables[0].getFieldVL(sfTableName));

            AccountID owner = beast::zero;
            if (txFinal.isFieldPresent(sfOwner))
                owner = txFinal.getAccountID(sfOwner);
            else if (txFinal.isFieldPresent(sfAccount))
                owner = txFinal.getAccountID(sfAccount);

            pubTableTxs(owner, sTableName, tx, res, true);
        }
    }
    else
    {
        // ripple original tx
        pubTxResult(tx, res, true, false);
    }
}

void
NetworkOPsImp::PubContractEvents(
    const AccountID& contractID,
    uint256 const* aTopic,
    int iTopicNum,
    const Blob& byValue)
{
    hash_set<InfoSub::pointer> notify;

    auto simiIt = mSubContract.find(contractID);
    if (simiIt != mSubContract.end())
    {
        auto it = simiIt->second.begin();

        while (it != simiIt->second.end())
        {
            InfoSub::pointer p = it->second.lock();

            if (p)
            {
                notify.insert(p);
                ++it;
            }
            else
                it = simiIt->second.erase(it);
        }
    }

    JLOG(m_journal.trace()) << "event:" << to_string(contractID);

    if (!notify.empty())
    {
        Json::Value jvObj;
        jvObj[jss::type] = "contract_event";
        jvObj[jss::ContractAddress] = to_string(contractID);
        jvObj[jss::ContractEventTopics] = Json::arrayValue;
        for (int i = 0; i < iTopicNum; i++)
        {
            jvObj[jss::ContractEventTopics][i] = to_string(aTopic[i]);
        }

        jvObj[jss::ContractEventInfo] = strHex(byValue);

        for (InfoSub::ref isrListener : notify)
            isrListener->send(jvObj, true);
    }
}

void
NetworkOPsImp::pubAccountTransaction(
    std::shared_ptr<ReadView const> const& lpCurrent,
    const AcceptedLedgerTx& alTx,
    bool bAccepted)
{
    hash_set<InfoSub::pointer> notify;
    int iProposed = 0;
    int iAccepted = 0;

    {
        std::lock_guard sl(mSubLock);

        if (!bAccepted && mSubRTAccount.empty())
            return;

        if (!mSubAccount.empty() || (!mSubRTAccount.empty()))
        {
            for (auto const& affectedAccount : alTx.getAffected())
            {
                auto simiIt = mSubRTAccount.find(affectedAccount);
                if (simiIt != mSubRTAccount.end())
                {
                    auto it = simiIt->second.begin();

                    while (it != simiIt->second.end())
                    {
                        InfoSub::pointer p = it->second.lock();

                        if (p)
                        {
                            notify.insert(p);
                            ++it;
                            ++iProposed;
                        }
                        else
                            it = simiIt->second.erase(it);
                    }
                }

                if (bAccepted)
                {
                    simiIt = mSubAccount.find(affectedAccount);

                    if (simiIt != mSubAccount.end())
                    {
                        auto it = simiIt->second.begin();
                        while (it != simiIt->second.end())
                        {
                            InfoSub::pointer p = it->second.lock();

                            if (p)
                            {
                                notify.insert(p);
                                ++it;
                                ++iAccepted;
                            }
                            else
                                it = simiIt->second.erase(it);
                        }
                    }
                }
            }
        }
    }
    JLOG(m_journal.trace())
        << "pubAccountTransaction:"
        << " iProposed=" << iProposed << " iAccepted=" << iAccepted;

    if (!notify.empty())
    {
        std::shared_ptr<STTx const> stTxn = alTx.getTxn();
        Json::Value jvObj =
            transJson(*stTxn, alTx.getResult(), bAccepted, lpCurrent);

        if (alTx.isApplied())
        {
            if (auto const txMeta = alTx.getMeta())
            {
                jvObj[jss::meta] = txMeta->getJson(JsonOptions::none);
                RPC::insertDeliveredAmount(
                    jvObj[jss::meta], *lpCurrent, stTxn, *txMeta);
            }
        }

        for (InfoSub::ref isrListener : notify)
            isrListener->send(jvObj, true);
    }
}

void
NetworkOPsImp::pubTableTxs(
    const AccountID& owner,
    const std::string& sTableName,
    const STTx& stTxn,
    const std::tuple<std::string, std::string, std::string>& res,
    bool bValidated)
{
    // db_success come,but validate_success not processed
    if (!bValidated && mSubTx.find(stTxn.getTransactionID()) != mSubTx.end())
    {
        auto result = std::make_tuple(std::string(jss::validate_success), "", "");
        pubTxResult(stTxn, result, true, true);
    }

    pubTxResult(stTxn, res, bValidated, true);
    pubChainSqlTableTxs(owner, sTableName, stTxn, res);
}

// publish results for chain-sql txs
void
NetworkOPsImp::pubTxResult(
    const STTx& stTxn,
    const std::tuple<std::string, std::string, std::string>& disposRes,
    bool bValidated,
    bool bForTableTx)
{
    ScopedLockType sl(mSubLock);
    auto& subTx = bValidated ? mSubTx : mValidatedSubTx;
    if (!subTx.empty())
    {
        auto txId   = stTxn.getTransactionID();
        auto simiIt	= subTx.find(txId);
        if (simiIt != subTx.end())
        {
            //bool bPendErase = false;
            InfoSub::pointer p = simiIt->second.first.lock();
            if (p)
            {
                Json::Value jvObj(Json::objectValue);
                jvObj[jss::type] = "singleTransaction";
                jvObj[jss::transaction] = stTxn.getJson(JsonOptions::none);
                if (jvObj[jss::transaction].isMember(jss::Raw) &&
                    jvObj[jss::transaction][jss::Raw].asString().size() > RAW_SHOW_SIZE)
                {
                    jvObj[jss::transaction].removeMember(jss::Raw);
                }
                jvObj[jss::status] = std::get<0>(disposRes);
                if (std::get<1>(disposRes).size() != 0)
                {
                    jvObj[jss::error] = std::get<1>(disposRes);
                }
                if (std::get<2>(disposRes).size() != 0)
                {
                    jvObj[jss::error_message] = std::get<2>(disposRes);
                }

                p->send(jvObj, true);

                // for table-related tx and validation event
                if (bValidated && bForTableTx && 
                    std::get<0>(disposRes) == jss::validate_success)
                {
                    // for chainsql type, subscribe db event
                    mValidatedSubTx[simiIt->first] =
                        make_pair(p, std::chrono::system_clock::now());
                }
                subTx.erase(simiIt);
                // if (bPendErase)
                //{
                //	std::thread([this, txId, jvToPub]() {
                //		std::this_thread::sleep_for(std::chrono::milliseconds(50));
                //		std::lock_guard sl(mSubLock);
                //		auto simiIt = mSubTx.find(txId);
                //		if (simiIt != mSubTx.end())
                //		{
                //			InfoSub::pointer p =
                //simiIt->second.first.lock(); 			if (p)
                //			{
                //				p->send(jvToPub, true);
                //			}
                //		}
                //		mSubTx.erase(simiIt);
                //	}).detach();
                //}
            }
        }
    }
}

void
NetworkOPsImp::pubChainSqlTableTxs(
    const AccountID& ownerId,
    const std::string& sTableName,
    const STTx& stTxn,
    const std::tuple<std::string, std::string, std::string>& disposRes)
{
    std::lock_guard sl(mSubLock);
    if (mSubTable.find(ownerId) != mSubTable.end() &&
        mSubTable[ownerId].find(sTableName) != mSubTable[ownerId].end())
    {
        Json::Value jvObj(Json::objectValue);
        jvObj[jss::type] = "table";
        jvObj[jss::tablename] = sTableName;
        jvObj[jss::owner] = to_string(ownerId);
        jvObj[jss::transaction] = stTxn.getJson(JsonOptions::none);
        jvObj[jss::status] = std::get<0>(disposRes);
        if (std::get<1>(disposRes).size() != 0)
        {
            jvObj[jss::error] = std::get<1>(disposRes);
        }
        if (std::get<2>(disposRes).size() != 0)
        {
            jvObj[jss::error_message] = std::get<2>(disposRes);
        }

        auto iter = mSubTable[ownerId][sTableName].begin();
        while (iter != mSubTable[ownerId][sTableName].end())
        {
            InfoSub::pointer p = iter->second.lock();

            if (p)
            {
                p->send(jvObj, true);
                ++iter;
            }
            else
            {
                iter = mSubTable[ownerId][sTableName].erase(iter);
                if (mSubTable[ownerId][sTableName].size() == 0)
                {
                    mSubTable[ownerId].erase(sTableName);
                    if (mSubTable[ownerId].size() == 0)
                        mSubTable.erase(ownerId);
                    break;
                }
            }
        }
    }
}
//
// Monitoring
//

void
NetworkOPsImp::subAccount(
    InfoSub::ref isrListener,
    hash_set<AccountID> const& vnaAccountIDs,
    InfoSub::ACOUNT_TYPE eType)
{
    SubInfoMapType& subMap = getCompatibleSubInfoMap(eType);

    for (auto const& naAccountID : vnaAccountIDs)
    {
        JLOG(m_journal.trace())
            << "subAccount: account: " << toBase58(naAccountID);

        isrListener->insertSubAccountInfo(naAccountID, eType);
    }

    std::lock_guard sl(mSubLock);

    for (auto const& naAccountID : vnaAccountIDs)
    {
        auto simIterator = subMap.find(naAccountID);
        if (simIterator == subMap.end())
        {
            // Not found, note that account has a new single listner.
            SubMapType usisElement;
            usisElement[isrListener->getSeq()] = isrListener;
            // VFALCO NOTE This is making a needless copy of naAccountID
            subMap.insert(simIterator, make_pair(naAccountID, usisElement));
        }
        else
        {
            // Found, note that the account has another listener.
            simIterator->second[isrListener->getSeq()] = isrListener;
        }
    }
}

void
NetworkOPsImp::unsubAccount(
    InfoSub::ref isrListener,
    hash_set<AccountID> const& vnaAccountIDs,
    InfoSub::ACOUNT_TYPE eType)
{
    for (auto const& naAccountID : vnaAccountIDs)
    {
        // Remove from the InfoSub
        isrListener->deleteSubAccountInfo(naAccountID, eType);
    }

    // Remove from the server
    unsubAccountInternal(isrListener->getSeq(), vnaAccountIDs, eType);
}

void
NetworkOPsImp::unsubAccountInternal(
    std::uint64_t uSeq,
    hash_set<AccountID> const& vnaAccountIDs,
    InfoSub::ACOUNT_TYPE eType)
{
    std::lock_guard sl(mSubLock);

    SubInfoMapType& subMap = getCompatibleSubInfoMap(eType);

    for (auto const& naAccountID : vnaAccountIDs)
    {
        auto simIterator = subMap.find(naAccountID);

        if (simIterator != subMap.end())
        {
            // Found
            simIterator->second.erase(uSeq);

            if (simIterator->second.empty())
            {
                // Don't need hash entry.
                subMap.erase(simIterator);
            }
        }
    }
}

NetworkOPsImp::SubInfoMapType&
NetworkOPsImp::getCompatibleSubInfoMap(InfoSub::ACOUNT_TYPE eType)
{
    switch (eType)
    {
        case ripple::InfoSub::ACCOUNT_NORMANT:
            return mSubAccount;
        case ripple::InfoSub::ACCOUNT_REALTIME:
            return mSubRTAccount;
        case ripple::InfoSub::ACCOUNT_CONTRACT:
            return mSubContract;
        default:
            return mSubAccount;
    }

    return mSubAccount;
}

bool
NetworkOPsImp::subBook(InfoSub::ref isrListener, Book const& book)
{
    if (auto listeners = app_.getOrderBookDB().makeBookListeners(book))
        listeners->addSubscriber(isrListener);
    else
        assert(false);
    return true;
}

bool
NetworkOPsImp::unsubBook(std::uint64_t uSeq, Book const& book)
{
    if (auto listeners = app_.getOrderBookDB().getBookListeners(book))
        listeners->removeSubscriber(uSeq);

    return true;
}

std::uint32_t
NetworkOPsImp::acceptLedger(
    boost::optional<std::chrono::milliseconds> consensusDelay)
{
    // This code-path is exclusively used when the server is in standalone
    // mode via `ledger_accept`
    assert(m_standalone);

    if (!m_standalone)
        Throw<std::runtime_error>(
            "Operation only possible in STANDALONE mode.");

    // FIXME Could we improve on this and remove the need for a specialized
    // API in Consensus?
    beginConsensus(m_ledgerMaster.getClosedLedger()->info().hash);
    mConsensus.simulate(app_.timeKeeper().closeTime(), consensusDelay);
    return m_ledgerMaster.getCurrentLedger()->info().seq;
}

// <-- bool: true=added, false=already there
bool
NetworkOPsImp::subLedger(InfoSub::ref isrListener, Json::Value& jvResult)
{
    if (auto lpClosed = m_ledgerMaster.getValidatedLedger())
    {
        jvResult[jss::type] = "ledgerClosed";
        jvResult[jss::ledger_index] = lpClosed->info().seq;
        jvResult[jss::ledger_hash] = to_string(lpClosed->info().hash);
        jvResult[jss::ledger_time] = Json::Value::UInt(
            lpClosed->info().closeTime.time_since_epoch().count());
        jvResult[jss::fee_ref] = lpClosed->fees().units.jsonClipped();
        jvResult[jss::fee_base] = lpClosed->fees().base.jsonClipped();
        jvResult[jss::drops_per_byte] =
            Json::UInt(lpClosed->fees().drops_per_byte);
        jvResult[jss::gas_price] =
            Json::UInt(lpClosed->fees().gas_price);
        jvResult[jss::reserve_base] =
            lpClosed->fees().accountReserve(0).jsonClipped();
        jvResult[jss::reserve_inc] = lpClosed->fees().increment.jsonClipped();
        jvResult[jss::schema_id] = to_string(app_.schemaId());
    }

    if ((mMode >= OperatingMode::SYNCING) && !isNeedNetworkLedger())
    {
        jvResult[jss::validated_ledgers] =
            app_.getLedgerMaster().getCompleteLedgers();
    }

    std::lock_guard sl(mSubLock);
    return mStreamMaps[sLedger]
        .emplace(isrListener->getSeq(), isrListener)
        .second;
}

// <-- bool: true=erased, false=was not there
bool
NetworkOPsImp::unsubLedger(std::uint64_t uSeq)
{
    std::lock_guard sl(mSubLock);

    auto id = std::string("#") + std::to_string(uSeq) + " ";
    JLOG(m_journal.info()) << id << "unsubLedger";

    return mStreamMaps[sLedger].erase(uSeq);
}


bool NetworkOPsImp::subViewChange(InfoSub::ref ispListener)
{
    ScopedLockType sl(mSubLock);
    return mStreamMaps[sViewChange].emplace(
        ispListener->getSeq(), ispListener).second;
}

bool NetworkOPsImp::unsubViewChange(std::uint64_t uSeq)
{
    ScopedLockType sl(mSubLock);
    return mStreamMaps[sViewChange].erase(uSeq);
}

//for all txs which changes the table
void NetworkOPsImp::subTable(InfoSub::ref isrListener, AccountID const& accountID, std::string const& sTableName)
{
    std::lock_guard sl(mSubLock);

    auto simIterator = mSubTable.find(accountID);
    if (simIterator == mSubTable.end())
    {
        hash_map<std::string, SubMapType> map;
        // Not found, note that account has a new single listener.
        SubMapType usisElement;
        usisElement[isrListener->getSeq()] = isrListener;
        map[sTableName] = usisElement;
        mSubTable.insert(simIterator, make_pair(accountID, map));
    }
    else
    {
        // Found owner, not found table
        auto iter = simIterator->second.find(sTableName);
        if (iter == simIterator->second.end())
        {
            SubMapType usisElement;
            usisElement[isrListener->getSeq()] = isrListener;
            simIterator->second.insert(
                iter, make_pair(sTableName, usisElement));
        }
        else
        {
            // owner and table both found
            simIterator->second[sTableName][isrListener->getSeq()] =
                isrListener;
        }
    }
}
void
NetworkOPsImp::unsubTable(
    InfoSub::ref isplistener,
    AccountID const& accountID,
    std::string const& sTableName)
{
    std::lock_guard sl(mSubLock);
    if (mSubTable.find(accountID) != mSubTable.end())
    {
        if (mSubTable[accountID].find(sTableName) != mSubTable[accountID].end())
        {
            mSubTable[accountID][sTableName].erase(isplistener->getSeq());
            if (mSubTable[accountID][sTableName].size() == 0)
            {
                mSubTable[accountID].erase(sTableName);
            }
        }
        if (mSubTable[accountID].size() == 0)
        {
            mSubTable.erase(accountID);
        }
    }
}

// for a single tx
void
NetworkOPsImp::subTransaction(InfoSub::ref isrListener, uint256 const& uTxId)
{
    std::lock_guard sl(mSubLock);

    mSubTx[uTxId] = make_pair(isrListener, std::chrono::system_clock::now());
}

void
NetworkOPsImp::unsubTransaction(InfoSub::ref ispListener, uint256 const& uTxId)
{
    std::lock_guard sl(mSubLock);
    mSubTx.erase(uTxId);
}

// <-- bool: true=added, false=already there
bool
NetworkOPsImp::subManifests(InfoSub::ref isrListener)
{
    std::lock_guard sl(mSubLock);
    return mStreamMaps[sManifests]
        .emplace(isrListener->getSeq(), isrListener)
        .second;
}

// <-- bool: true=erased, false=was not there
bool
NetworkOPsImp::unsubManifests(std::uint64_t uSeq)
{
    std::lock_guard sl(mSubLock);
    return mStreamMaps[sManifests].erase(uSeq);
}

// <-- bool: true=added, false=already there
bool
NetworkOPsImp::subServer(
    InfoSub::ref isrListener,
    Json::Value& jvResult,
    bool admin)
{
    uint256 uRandom;

    if (m_standalone)
        jvResult[jss::stand_alone] = m_standalone;

    // CHECKME: is it necessary to provide a random number here?
    beast::rngfill(uRandom.begin(), uRandom.size(), crypto_prng());

    auto const& feeTrack = app_.getFeeTrack();
    jvResult[jss::type] = "serverStatus";
    jvResult[jss::random] = to_string(uRandom);
    jvResult[jss::server_status] = strOperatingMode(admin);
    jvResult[jss::load_base] = feeTrack.getLoadBase();
    jvResult[jss::load_factor] = feeTrack.getLoadFactor();

    jvResult[jss::hostid] = getHostId(admin);
    jvResult[jss::pubkey_node] =
        toBase58(TokenType::NodePublic, app_.nodeIdentity().first);
    jvResult[jss::schema_id] = to_string(app_.schemaId());

    std::lock_guard sl(mSubLock);
    return mStreamMaps[sServer]
        .emplace(isrListener->getSeq(), isrListener)
        .second;
}

// <-- bool: true=erased, false=was not there
bool
NetworkOPsImp::unsubServer(std::uint64_t uSeq)
{
    std::lock_guard sl(mSubLock);
    return mStreamMaps[sServer].erase(uSeq);
}

// <-- bool: true=added, false=already there
bool
NetworkOPsImp::subTransactions(InfoSub::ref isrListener)
{
    std::lock_guard sl(mSubLock);
    return mStreamMaps[sTransactions]
        .emplace(isrListener->getSeq(), isrListener)
        .second;
}

// <-- bool: true=erased, false=was not there
bool
NetworkOPsImp::unsubTransactions(std::uint64_t uSeq)
{
    std::lock_guard sl(mSubLock);
    return mStreamMaps[sTransactions].erase(uSeq);
}

// <-- bool: true=added, false=already there
bool
NetworkOPsImp::subRTTransactions(InfoSub::ref isrListener)
{
    std::lock_guard sl(mSubLock);
    return mStreamMaps[sRTTransactions]
        .emplace(isrListener->getSeq(), isrListener)
        .second;
}

// <-- bool: true=erased, false=was not there
bool
NetworkOPsImp::unsubRTTransactions(std::uint64_t uSeq)
{
    std::lock_guard sl(mSubLock);
    return mStreamMaps[sRTTransactions].erase(uSeq);
}

// <-- bool: true=added, false=already there
bool
NetworkOPsImp::subValidations(InfoSub::ref isrListener)
{
    std::lock_guard sl(mSubLock);
    return mStreamMaps[sValidations]
        .emplace(isrListener->getSeq(), isrListener)
        .second;
}

// <-- bool: true=erased, false=was not there
bool
NetworkOPsImp::unsubValidations(std::uint64_t uSeq)
{
    std::lock_guard sl(mSubLock);
    return mStreamMaps[sValidations].erase(uSeq);
}

// <-- bool: true=added, false=already there
bool
NetworkOPsImp::subPeerStatus(InfoSub::ref isrListener)
{
    std::lock_guard sl(mSubLock);
    return mStreamMaps[sPeerStatus]
        .emplace(isrListener->getSeq(), isrListener)
        .second;
}

// <-- bool: true=erased, false=was not there
bool
NetworkOPsImp::unsubPeerStatus(std::uint64_t uSeq)
{
    std::lock_guard sl(mSubLock);
    return mStreamMaps[sPeerStatus].erase(uSeq);
}

// <-- bool: true=added, false=already there
bool
NetworkOPsImp::subConsensus(InfoSub::ref isrListener)
{
    std::lock_guard sl(mSubLock);
    return mStreamMaps[sConsensusPhase]
        .emplace(isrListener->getSeq(), isrListener)
        .second;
}

// <-- bool: true=erased, false=was not there
bool
NetworkOPsImp::unsubConsensus(std::uint64_t uSeq)
{
    std::lock_guard sl(mSubLock);
    return mStreamMaps[sConsensusPhase].erase(uSeq);
}

bool
NetworkOPsImp::subLogs(InfoSub::ref isrListener)
{
    std::lock_guard sl(mSubLock);
    return mStreamMaps[sLogs]
        .emplace(isrListener->getSeq(), isrListener)
        .second;
}

bool
NetworkOPsImp::unsubLogs(std::uint64_t uSeq)
{
    std::lock_guard sl(mSubLock);
    return mStreamMaps[sLogs].erase(uSeq);
}

InfoSub::pointer
NetworkOPsImp::findRpcSub(std::string const& strUrl)
{
    std::lock_guard sl(mSubLock);

    subRpcMapType::iterator it = mRpcSubMap.find(strUrl);

    if (it != mRpcSubMap.end())
        return it->second;

    return InfoSub::pointer();
}

InfoSub::pointer
NetworkOPsImp::addRpcSub(std::string const& strUrl, InfoSub::ref rspEntry)
{
    std::lock_guard sl(mSubLock);

    mRpcSubMap.emplace(strUrl, rspEntry);

    return rspEntry;
}

bool
NetworkOPsImp::tryRemoveRpcSub(std::string const& strUrl)
{
    std::lock_guard sl(mSubLock);
    auto pInfo = findRpcSub(strUrl);

    if (!pInfo)
        return false;

    // check to see if any of the stream maps still hold a weak reference to
    // this entry before removing
    for (SubMapType const& map : mStreamMaps)
    {
        if (map.find(pInfo->getSeq()) != map.end())
            return false;
    }
    mRpcSubMap.erase(strUrl);
    return true;
}

#ifndef USE_NEW_BOOK_PAGE

// NIKB FIXME this should be looked at. There's no reason why this shouldn't
//            work, but it demonstrated poor performance.
//
void
NetworkOPsImp::getBookPage(
    std::shared_ptr<ReadView const>& lpLedger,
    Book const& book,
    AccountID const& uTakerID,
    bool const bProof,
    unsigned int iLimit,
    Json::Value const& jvMarker,
    Json::Value& jvResult)
{  // CAUTION: This is the old get book page logic
    Json::Value& jvOffers =
        (jvResult[jss::offers] = Json::Value(Json::arrayValue));

    std::map<AccountID, STAmount> umBalance;
    const uint256 uBookBase = getBookBase(book);
    const uint256 uBookEnd = getQualityNext(uBookBase);
    uint256 uTipIndex = uBookBase;

    if (auto stream = m_journal.trace())
    {
        stream << "getBookPage:" << book;
        stream << "getBookPage: uBookBase=" << uBookBase;
        stream << "getBookPage: uBookEnd=" << uBookEnd;
        stream << "getBookPage: uTipIndex=" << uTipIndex;
    }

    ReadView const& view = *lpLedger;

    bool const bGlobalFreeze = isGlobalFrozen(view, book.out.account) ||
        isGlobalFrozen(view, book.in.account);

    bool bDone = false;
    bool bDirectAdvance = true;

    std::shared_ptr<SLE const> sleOfferDir;
    uint256 offerIndex;
    unsigned int uBookEntry;
    STAmount saDirRate;

    auto const rate = transferRate(view, book.out.account);
    auto viewJ = app_.journal("View");

    while (!bDone && iLimit-- > 0)
    {
        if (bDirectAdvance)
        {
            bDirectAdvance = false;

            JLOG(m_journal.trace()) << "getBookPage: bDirectAdvance";

            auto const ledgerIndex = view.succ(uTipIndex, uBookEnd);
            if (ledgerIndex)
                sleOfferDir = view.read(keylet::page(*ledgerIndex));
            else
                sleOfferDir.reset();

            if (!sleOfferDir)
            {
                JLOG(m_journal.trace()) << "getBookPage: bDone";
                bDone = true;
            }
            else
            {
                uTipIndex = sleOfferDir->key();
                saDirRate = amountFromQuality(getQuality(uTipIndex));

                cdirFirst(
                    view,
                    uTipIndex,
                    sleOfferDir,
                    uBookEntry,
                    offerIndex,
                    viewJ);

                JLOG(m_journal.trace())
                    << "getBookPage:   uTipIndex=" << uTipIndex;
                JLOG(m_journal.trace())
                    << "getBookPage: offerIndex=" << offerIndex;
            }
        }

        if (!bDone)
        {
            auto sleOffer = view.read(keylet::offer(offerIndex));

            if (sleOffer)
            {
                auto const uOfferOwnerID = sleOffer->getAccountID(sfAccount);
                auto const& saTakerGets = sleOffer->getFieldAmount(sfTakerGets);
                auto const& saTakerPays = sleOffer->getFieldAmount(sfTakerPays);
                STAmount saOwnerFunds;
                bool firstOwnerOffer(true);

                if (book.out.account == uOfferOwnerID)
                {
                    // If an offer is selling issuer's own IOUs, it is fully
                    // funded.
                    saOwnerFunds = saTakerGets;
                }
                else if (bGlobalFreeze)
                {
                    // If either asset is globally frozen, consider all offers
                    // that aren't ours to be totally unfunded
                    saOwnerFunds.clear(book.out);
                }
                else
                {
                    auto umBalanceEntry = umBalance.find(uOfferOwnerID);
                    if (umBalanceEntry != umBalance.end())
                    {
                        // Found in running balance table.

                        saOwnerFunds = umBalanceEntry->second;
                        firstOwnerOffer = false;
                    }
                    else
                    {
                        // Did not find balance in table.

                        saOwnerFunds = accountHolds(
                            view,
                            uOfferOwnerID,
                            book.out.currency,
                            book.out.account,
                            fhZERO_IF_FROZEN,
                            viewJ);

                        if (saOwnerFunds < beast::zero)
                        {
                            // Treat negative funds as zero.

                            saOwnerFunds.clear();
                        }
                    }
                }

                Json::Value jvOffer = sleOffer->getJson(JsonOptions::none);

                STAmount saTakerGetsFunded;
                STAmount saOwnerFundsLimit = saOwnerFunds;
                Rate offerRate = parityRate;

                if (rate != parityRate
                    // Have a tranfer fee.
                    && uTakerID != book.out.account
                    // Not taking offers of own IOUs.
                    && book.out.account != uOfferOwnerID)
                // Offer owner not issuing ownfunds
                {
                    // Need to charge a transfer fee to offer owner.
                    offerRate = rate;
                    saOwnerFundsLimit = divide(saOwnerFunds, offerRate);
                }

                if (saOwnerFundsLimit >= saTakerGets)
                {
                    // Sufficient funds no shenanigans.
                    saTakerGetsFunded = saTakerGets;
                }
                else
                {
                    // Only provide, if not fully funded.

                    saTakerGetsFunded = saOwnerFundsLimit;

                    saTakerGetsFunded.setJson(jvOffer[jss::taker_gets_funded]);
                    std::min(
                        saTakerPays,
                        multiply(
                            saTakerGetsFunded, saDirRate, saTakerPays.issue()))
                        .setJson(jvOffer[jss::taker_pays_funded]);
                }

                STAmount saOwnerPays = (parityRate == offerRate)
                    ? saTakerGetsFunded
                    : std::min(
                          saOwnerFunds, multiply(saTakerGetsFunded, offerRate));

                umBalance[uOfferOwnerID] = saOwnerFunds - saOwnerPays;

                // Include all offers funded and unfunded
                Json::Value& jvOf = jvOffers.append(jvOffer);
                jvOf[jss::quality] = saDirRate.getText();

                if (firstOwnerOffer)
                    jvOf[jss::owner_funds] = saOwnerFunds.getText();
            }
            else
            {
                JLOG(m_journal.warn()) << "Missing offer";
            }

            if (!cdirNext(
                    view,
                    uTipIndex,
                    sleOfferDir,
                    uBookEntry,
                    offerIndex,
                    viewJ))
            {
                bDirectAdvance = true;
            }
            else
            {
                JLOG(m_journal.trace())
                    << "getBookPage: offerIndex=" << offerIndex;
            }
        }
    }

    //  jvResult[jss::marker]  = Json::Value(Json::arrayValue);
    //  jvResult[jss::nodes]   = Json::Value(Json::arrayValue);
}

#else

// This is the new code that uses the book iterators
// It has temporarily been disabled

void
NetworkOPsImp::getBookPage(
    std::shared_ptr<ReadView const> lpLedger,
    Book const& book,
    AccountID const& uTakerID,
    bool const bProof,
    unsigned int iLimit,
    Json::Value const& jvMarker,
    Json::Value& jvResult)
{
    auto& jvOffers = (jvResult[jss::offers] = Json::Value(Json::arrayValue));

    std::map<AccountID, STAmount> umBalance;

    MetaView lesActive(lpLedger, tapNONE, true);
    OrderBookIterator obIterator(lesActive, book);

    auto const rate = transferRate(lesActive, book.out.account);

    const bool bGlobalFreeze = lesActive.isGlobalFrozen(book.out.account) ||
        lesActive.isGlobalFrozen(book.in.account);

    while (iLimit-- > 0 && obIterator.nextOffer())
    {
        SLE::pointer sleOffer = obIterator.getCurrentOffer();
        if (sleOffer)
        {
            auto const uOfferOwnerID = sleOffer->getAccountID(sfAccount);
            auto const& saTakerGets = sleOffer->getFieldAmount(sfTakerGets);
            auto const& saTakerPays = sleOffer->getFieldAmount(sfTakerPays);
            STAmount saDirRate = obIterator.getCurrentRate();
            STAmount saOwnerFunds;

            if (book.out.account == uOfferOwnerID)
            {
                // If offer is selling issuer's own IOUs, it is fully funded.
                saOwnerFunds = saTakerGets;
            }
            else if (bGlobalFreeze)
            {
                // If either asset is globally frozen, consider all offers
                // that aren't ours to be totally unfunded
                saOwnerFunds.clear(book.out);
            }
            else
            {
                auto umBalanceEntry = umBalance.find(uOfferOwnerID);

                if (umBalanceEntry != umBalance.end())
                {
                    // Found in running balance table.

                    saOwnerFunds = umBalanceEntry->second;
                }
                else
                {
                    // Did not find balance in table.

                    saOwnerFunds = lesActive.accountHolds(
                        uOfferOwnerID,
                        book.out.currency,
                        book.out.account,
                        fhZERO_IF_FROZEN);

                    if (saOwnerFunds.isNegative())
                    {
                        // Treat negative funds as zero.

                        saOwnerFunds.zero();
                    }
                }
            }

            Json::Value jvOffer = sleOffer->getJson(JsonOptions::none);

            STAmount saTakerGetsFunded;
            STAmount saOwnerFundsLimit = saOwnerFunds;
            Rate offerRate = parityRate;

            if (rate != parityRate
                // Have a tranfer fee.
                && uTakerID != book.out.account
                // Not taking offers of own IOUs.
                && book.out.account != uOfferOwnerID)
            // Offer owner not issuing ownfunds
            {
                // Need to charge a transfer fee to offer owner.
                offerRate = rate;
                saOwnerFundsLimit = divide(saOwnerFunds, offerRate);
            }

            if (saOwnerFundsLimit >= saTakerGets)
            {
                // Sufficient funds no shenanigans.
                saTakerGetsFunded = saTakerGets;
            }
            else
            {
                // Only provide, if not fully funded.
                saTakerGetsFunded = saOwnerFundsLimit;

                saTakerGetsFunded.setJson(jvOffer[jss::taker_gets_funded]);

                // TOOD(tom): The result of this expression is not used - what's
                // going on here?
                std::min(
                    saTakerPays,
                    multiply(saTakerGetsFunded, saDirRate, saTakerPays.issue()))
                    .setJson(jvOffer[jss::taker_pays_funded]);
            }

            STAmount saOwnerPays = (parityRate == offerRate)
                ? saTakerGetsFunded
                : std::min(
                      saOwnerFunds, multiply(saTakerGetsFunded, offerRate));

            umBalance[uOfferOwnerID] = saOwnerFunds - saOwnerPays;

            if (!saOwnerFunds.isZero() || uOfferOwnerID == uTakerID)
            {
                // Only provide funded offers and offers of the taker.
                Json::Value& jvOf = jvOffers.append(jvOffer);
                jvOf[jss::quality] = saDirRate.getText();
            }
        }
    }

    //  jvResult[jss::marker]  = Json::Value(Json::arrayValue);
    //  jvResult[jss::nodes]   = Json::Value(Json::arrayValue);
}

#endif

//------------------------------------------------------------------------------

NetworkOPs::NetworkOPs(Stoppable& parent)
    : InfoSub::Source("NetworkOPs", parent)
{
}

//------------------------------------------------------------------------------

void
NetworkOPsImp::StateAccounting::mode(OperatingMode om)
{
    auto now = std::chrono::system_clock::now();

    std::lock_guard lock(mutex_);
    ++counters_[static_cast<std::size_t>(om)].transitions;
    counters_[static_cast<std::size_t>(mode_)].dur +=
        std::chrono::duration_cast<std::chrono::microseconds>(now - start_);

    mode_ = om;
    start_ = now;
}

NetworkOPsImp::StateAccounting::StateCountersJson
NetworkOPsImp::StateAccounting::json() const
{
    auto [counters, mode, start] = getCounterData();
    auto const current = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now() - start);
    counters[static_cast<std::size_t>(mode)].dur += current;

    Json::Value ret = Json::objectValue;

    for (std::size_t i = static_cast<std::size_t>(OperatingMode::DISCONNECTED);
         i <= static_cast<std::size_t>(OperatingMode::FULL);
         ++i)
    {
        ret[states_[i]] = Json::objectValue;
        auto& state = ret[states_[i]];
        state[jss::transitions] = counters[i].transitions;
        state[jss::duration_us] = std::to_string(counters[i].dur.count());
    }

    return {ret, std::to_string(current.count())};
}

//------------------------------------------------------------------------------

std::unique_ptr<NetworkOPs>
make_NetworkOPs(
    Schema& app,
    NetworkOPs::clock_type& clock,
    bool standalone,
    std::size_t minPeerCount,
    bool startvalid,
    JobQueue& job_queue,
    LedgerMaster& ledgerMaster,
    Stoppable& parent,
    ValidatorKeys const& validatorKeys,
    boost::asio::io_service& io_svc,
    beast::Journal journal,
    beast::insight::Collector::ptr const& collector)
{
    return std::make_unique<NetworkOPsImp>(
        app,
        clock,
        standalone,
        minPeerCount,
        startvalid,
        job_queue,
        ledgerMaster,
        parent,
        validatorKeys,
        io_svc,
        journal,
        collector);
}

}  // namespace ripple
