#include <ripple/app/main/Application.h>
#include <ripple/core/DatabaseCon.h>
#include <ripple/app/consensus/RCLValidations.h>
#include <ripple/app/main/DBInit.h>
#include <ripple/app/main/BasicApp.h>
#include <ripple/app/main/Tuning.h>
#include <ripple/app/ledger/InboundLedgers.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/ledger/LedgerToJson.h>
#include <ripple/app/ledger/OpenLedger.h>
#include <ripple/app/ledger/OrderBookDB.h>
#include <ripple/app/ledger/PendingSaves.h>
#include <ripple/app/ledger/InboundTransactions.h>
#include <ripple/app/ledger/TransactionMaster.h>
#include <ripple/app/main/LoadManager.h>
#include <ripple/app/main/NodeIdentity.h>
#include <ripple/app/main/NodeStoreScheduler.h>
#include <ripple/app/misc/AmendmentTable.h>
#include <ripple/app/misc/HashRouter.h>
#include <ripple/app/misc/LoadFeeTrack.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/app/misc/SHAMapStore.h>
#include <ripple/app/misc/TxQ.h>
#include <ripple/app/misc/ValidatorSite.h>
#include <ripple/app/misc/ValidatorKeys.h>
#include <ripple/app/paths/PathRequests.h>
#include <ripple/app/tx/apply.h>
#include <ripple/basics/ByteUtilities.h>
#include <ripple/basics/ResolverAsio.h>
#include <ripple/basics/safe_cast.h>
#include <ripple/basics/PerfLog.h>
#include <ripple/json/json_reader.h>
#include <ripple/nodestore/DummyScheduler.h>
#include <ripple/overlay/Cluster.h>
#include <ripple/overlay/make_Overlay.h>
#include <ripple/protocol/BuildInfo.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/STParsedJSON.h>
#include <ripple/protocol/Protocol.h>
#include <ripple/resource/Fees.h>
#include <ripple/rpc/impl/RPCHelpers.h>
#include <ripple/rpc/ShardArchiveHandler.h>
#include <ripple/beast/asio/io_latency_probe.h>
#include <ripple/beast/core/LexicalCast.h>
#include <ripple/shamap/NodeFamily.h>
#include <ripple/shamap/ShardFamily.h>
#include <ripple/overlay/PeerReservationTable.h>
#include <peersafe/app/sql/TxStore.h>
#include <peersafe/app/storage/TableStorage.h>
#include <peersafe/rpc/impl/TableAssistant.h>
#include <peersafe/app/misc/ContractHelper.h>
#include <peersafe/app/misc/CACertSite.h>
#include <peersafe/app/misc/CertList.h>
#include <peersafe/app/table/TableTxAccumulator.h>
#include <peersafe/app/table/TableSync.h>
#include <peersafe/app/table/TableStatusDBMySQL.h>
#include <peersafe/app/table/TableStatusDBSQLite.h>
#include <peersafe/app/misc/TxPool.h>
#include <peersafe/app/prometh/PrometheusClient.h>
#include <peersafe/precompiled/PreContractFace.h>
#include <peersafe/app/misc/StateManager.h>
#include <peersafe/app/misc/ConnectionPool.h>
#include <peersafe/schema/Schema.h>
#include <peersafe/schema/PeerManager.h>
#include <peersafe/schema/SchemaManager.h>
#include <peersafe/app/bloom/BloomManager.h>
#include <peersafe/app/sql/TxnDBConn.h>
#include <openssl/evp.h>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <ripple/nodestore/Scheduler.h>
namespace ripple {


class SchemaImp : public Schema, public RootStoppable
{
public:
    bool
    setup() override;
    bool
    initBeforeSetup() override;
    bool
    available() override;
    bool
    nodeToShards();
    bool
    validateShards();
    bool
    startGenesisLedger();
    bool
    setSynTable();

    std::shared_ptr<Ledger>
    getLastFullLedger(unsigned offset);

    std::shared_ptr<Ledger>
    loadLedgerFromFile(std::string const& ledgerID);

    bool
    loadOldLedger(std::string const& ledgerID, bool replay, bool isFilename, unsigned offset=0);

    bool
    startGenesisLedger(std::shared_ptr<Ledger const> loadLedger);

private:
    Application& app_;
    beast::Journal m_journal;
    SchemaParams schema_params_;
    std::shared_ptr<Config> config_;

    bool m_schemaAvailable;
    std::atomic<bool> waitingBeginConsensus_;

    Application::MutexType m_masterMutex;
    TransactionMaster m_txMaster;

    std::unique_ptr<SHAMapStore> m_shaMapStore;
    PendingSaves pendingSaves_;
    AccountIDCache accountIDCache_;
    boost::optional<OpenLedger> openLedger_;

    // These are not Stoppable-derived
    NodeCache m_tempNodeCache;
    CachedSLEs cachedSLEs_;

    // These are Stoppable-related
    std::unique_ptr<NodeStore::Database> m_nodeStore;
    std::unique_ptr<NodeStore::DatabaseShard> shardStore_;
    NodeFamily nodeFamily_;
    std::unique_ptr<ShardFamily> shardFamily_;
    std::unique_ptr<RPC::ShardArchiveHandler> shardArchiveHandler_;
    // VFALCO TODO Make OrderBookDB abstract
    OrderBookDB m_orderBookDB;
    std::unique_ptr<PathRequests> m_pathRequests;
    std::unique_ptr<LedgerMaster> m_ledgerMaster;
    std::unique_ptr<InboundLedgers> m_inboundLedgers;
    std::unique_ptr<InboundTransactions> m_inboundTransactions;
    TaggedCache<uint256, AcceptedLedger> m_acceptedLedgerCache;
    std::unique_ptr<NetworkOPs> m_networkOPs;
    std::unique_ptr<Cluster> cluster_;
    std::unique_ptr<PeerReservationTable> peerReservations_;
    std::unique_ptr<ManifestCache> validatorManifests_;
    std::unique_ptr<ManifestCache> publisherManifests_;
    std::unique_ptr<ValidatorList> validators_;
    std::unique_ptr<ValidatorSite> validatorSites_;
    std::unique_ptr<UserCertList> userCertList_;
    std::unique_ptr<CACertSite> caCertSites_;
    std::unique_ptr<AmendmentTable> m_amendmentTable;
    // std::unique_ptr <PreContractFace> m_preContractFace;
    std::unique_ptr<LoadFeeTrack> mFeeTrack;
    std::unique_ptr<HashRouter> mHashRouter;
    RCLValidations mValidations;
    std::unique_ptr<TxQ> txQ_;
    std::unique_ptr<TxStoreDBConn> m_pTxStoreDBConn;
    std::unique_ptr<TxStore> m_pTxStore;
    std::unique_ptr<TableStatusDB> m_pTableStatusDB;
    std::unique_ptr<TableSync> m_pTableSync;
    std::unique_ptr<TableStorage> m_pTableStorage;
    std::unique_ptr<TableAssistant> m_pTableAssistant;
    std::unique_ptr<ContractHelper> m_pContractHelper;
    std::unique_ptr<TableTxAccumulator> m_pTableTxAccumulator;
    std::unique_ptr<TxPool> m_pTxPool;
    std::unique_ptr<StateManager> m_pStateManager;
    std::unique_ptr<BloomManager> m_pBloomManager;
    std::unique_ptr<ConnectionPool> m_pConnectionPool;
    ClosureCounter<void, boost::system::error_code const&> waitHandlerCounter_;

    std::unique_ptr<TxnDBCon> mTxnDB;
    std::unique_ptr<DatabaseCon> mLedgerDB;
    std::unique_ptr<DatabaseCon> mWalletDB;
    std::unique_ptr<PeerManager> m_peerManager;
    std::unique_ptr<PrometheusClient> m_pPrometheusClient;

public:
    SchemaImp(
        SchemaParams const& params,
        std::shared_ptr<Config> config,
        Application& app,
        beast::Journal j)
        : RootStoppable("Schema")
        , app_(app)
        , m_journal(j)
        , schema_params_(params)
        , config_(config)
        , m_schemaAvailable(
              schema_params_.schemaId() == beast::zero ? true : false)

        , waitingBeginConsensus_{false}

        , m_txMaster(*this)

        , m_shaMapStore(make_SHAMapStore(
              *this,
              *this,
              app_.nodeStoreScheduler(),
              SchemaImp::journal("SHAMapStore")))

        , accountIDCache_(128000)

        , m_tempNodeCache(
              "NodeCache",
              16384,
              std::chrono::seconds{90},
              stopwatch(),
              SchemaImp::journal("TaggedCache"))

        , cachedSLEs_(std::chrono::minutes(1), stopwatch())
   
        //
        // Anything which calls addJob must be a descendant of the JobQueue
        //
        , m_nodeStore(m_shaMapStore->makeNodeStore("NodeStore.main", 4))

        // , shardStore_(
        // 	m_shaMapStore->makeDatabaseShard("ShardStore", 4, *this))

        , shardStore_(make_ShardStore(
              *this,
              *this,
              app_.nodeStoreScheduler(),
              4,
              SchemaImp::journal("ShardStore")))

        , nodeFamily_(*this, app.getCollectorManager())

        , m_orderBookDB(*this, *this)

        , m_pathRequests(std::make_unique<PathRequests>(
              *this,
              SchemaImp::journal("PathRequest"),
              app.getCollectorManager().collector()))

        , m_ledgerMaster(std::make_unique<LedgerMaster>(
              *this,
              stopwatch(),
              *this,
              app.getCollectorManager().collector(),
              SchemaImp::journal("LedgerMaster")))

        // VFALCO NOTE must come before NetworkOPs to prevent a crash due
        //             to dependencies in the destructor.
        //
        , m_inboundLedgers(make_InboundLedgers(
              *this,
              stopwatch(),
              *this,
              app.getCollectorManager().collector()))

        , m_inboundTransactions(make_InboundTransactions(
              *this,
              *this,
              app.getCollectorManager().collector(),
              [this](std::shared_ptr<SHAMap> const& set, bool fromAcquire) {
                  gotTXSet(set, fromAcquire);
              }))
        
        , m_acceptedLedgerCache(
              "AcceptedLedger",
              4,
              std::chrono::minutes{1},
              stopwatch(),
              SchemaImp::journal("TaggedCache"))

        , m_networkOPs(make_NetworkOPs(
              *this,
              stopwatch(),
              config_->standalone(),
              config_->NETWORK_QUORUM,
              config_->START_VALID,
              app_.getJobQueue(),
              *m_ledgerMaster,
              *this,
              app_.getValidatorKeys(),
              dynamic_cast<BasicApp&>(app_).get_io_service(),
              SchemaImp::journal("NetworkOPs"),
              app.getCollectorManager().collector()))

        , cluster_(std::make_unique<Cluster>(SchemaImp::journal("Overlay")))

        , peerReservations_(std::make_unique<PeerReservationTable>(
              SchemaImp::journal("PeerReservationTable")))

        , validatorManifests_(std::make_unique<ManifestCache>(
              SchemaImp::journal("ManifestCache")))

        , publisherManifests_(std::make_unique<ManifestCache>(
              SchemaImp::journal("ManifestCache")))

        , validators_(std::make_unique<ValidatorList>(
              *validatorManifests_,
              *publisherManifests_,
              app_.timeKeeper(),
              config_->legacy("database_path"),
              SchemaImp::journal("ValidatorList"),
              config_->VALIDATION_QUORUM))
        , validatorSites_(std::make_unique<ValidatorSite>(*this))

        , userCertList_(std::make_unique<UserCertList>(
              config_->USER_ROOT_CERTIFICATES,
              SchemaImp::journal("CertList")))
        , caCertSites_(std::make_unique<CACertSite>(*this))

        , mFeeTrack(
              std::make_unique<LoadFeeTrack>(SchemaImp::journal("LoadManager")))

        , mHashRouter(std::make_unique<HashRouter>(
              stopwatch(),
              HashRouter::getDefaultHoldTime(),
              HashRouter::getDefaultRecoverLimit()))

        , mValidations(
              ValidationParms(),
              stopwatch(),
              *this,
              SchemaImp::journal("Validations"))

        , txQ_(std::make_unique<TxQ>(
              setup_TxQ(*config_),
              SchemaImp::journal("TxQ")))

        , m_pTxStoreDBConn(std::make_unique<TxStoreDBConn>(*config_))

        , m_pTxStore(std::make_unique<TxStore>(
              m_pTxStoreDBConn->GetDBConn(),
              *config_,
              SchemaImp::journal("TxStore")))

        , m_pTableSync(std::make_unique<TableSync>(
              *this,
              *config_,
              SchemaImp::journal("TableSync")))

        , m_pTableStorage(std::make_unique<TableStorage>(
              *this,
              *config_,
              SchemaImp::journal("TableStorage")))

        , m_pTableAssistant(std::make_unique<TableAssistant>(
              *this,
              *config_,
              SchemaImp::journal("TableAssistant")))

        , m_pContractHelper(std::make_unique<ContractHelper>(*this))

        , m_pTableTxAccumulator(std::make_unique<TableTxAccumulator>(*this))

        , m_pTxPool(
              std::make_unique<TxPool>(*this, SchemaImp::journal("TxPool")))

        , m_pStateManager(std::make_unique<StateManager>(
              *this,
              SchemaImp::journal("StateManager")))

        , m_pBloomManager(std::make_unique<BloomManager>(
                *this,
                SchemaImp::journal("BloomManager")
            ))

        , m_pConnectionPool(std::make_unique<ConnectionPool>(*this))

        , m_peerManager(make_PeerManager(*this))
        , m_pPrometheusClient(std::make_unique<PrometheusClient>(
              *this,
              *config_,
              app.getPromethExposer(),
              SchemaImp::journal("PrometheusClient")))

    {
    }

    Application&
    app() override
    {
        return app_;
    }

    Logs&
    logs() override
    {
        return app_.logs();
    }

    beast::Journal
    journal(std::string const& name) override
    {
        std::string prefix = strHex(
            schema_params_.schema_id.begin(),
            schema_params_.schema_id.begin() + 2);
        return app_.logs().journal("[" + prefix + "]" + name);
    }

    CollectorManager&
    getCollectorManager() override
    {
        return app_.getCollectorManager();
    }

    TimeKeeper&
    timeKeeper() override
    {
        return app_.timeKeeper();
    }

    boost::asio::io_service&
    getIOService() override
    {
        return app_.getIOService();
    }

    JobQueue&
    getJobQueue() override
    {
        return app_.getJobQueue();
    }

    std::recursive_mutex&
    getMasterMutex() override
    {
        return m_masterMutex;
    }

    LoadManager&
    getLoadManager() override
    {
        return app_.getLoadManager();
    }

    Resource::Manager&
    getResourceManager() override
    {
        return app_.getResourceManager();
    }

    perf::PerfLog&
    getPerfLog() override
    {
        return app_.getPerfLog();
    }

    std::pair<PublicKey, SecretKey> const&
    nodeIdentity() override
    {
        return app_.nodeIdentity();
    }

    virtual PublicKey const&
    getValidationPublicKey() const override
    {
        return app_.getValidationPublicKey();
    }

    ValidatorKeys const&
    getValidatorKeys() const override
    {
        return app_.getValidatorKeys();
    }

    Config&
    config() override
    {
        return *config_;
    }
    Family&
    getNodeFamily() override
    {
        return nodeFamily_;
    }

    Family*
    getShardFamily() override
    {
        return shardFamily_.get();
    }

    TxStoreDBConn&
    getTxStoreDBConn() override
    {
        return *m_pTxStoreDBConn;
    }

    TxStore&
    getTxStore() override
    {
        return *m_pTxStore;
    }

    TableStatusDB&
    getTableStatusDB() override
    {
        return *m_pTableStatusDB;
    }

    TableSync&
    getTableSync() override
    {
        return *m_pTableSync;
    }

    TableStorage&
    getTableStorage() override
    {
        return *m_pTableStorage;
    }

    TableAssistant&
    getTableAssistant() override
    {
        return *m_pTableAssistant;
    }

    ContractHelper&
    getContractHelper() override
    {
        return *m_pContractHelper;
    }

    TableTxAccumulator&
    getTableTxAccumulator() override
    {
        return *m_pTableTxAccumulator;
    }

    TxPool&
    getTxPool() override
    {
        return *m_pTxPool;
    }

    StateManager&
    getStateManager() override
    {
        return *m_pStateManager;
    }

    BloomManager&
    getBloomManager() override
    {
        return *m_pBloomManager;
    }

    ConnectionPool&
    getConnectionPool() override
    {
        return *m_pConnectionPool;
    }

    NetworkOPs&
    getOPs() override
    {
        return *m_networkOPs;
    }
    LedgerMaster&
    getLedgerMaster() override
    {
        return *m_ledgerMaster;
    }

    InboundLedgers&
    getInboundLedgers() override
    {
        return *m_inboundLedgers;
    }

    InboundTransactions&
    getInboundTransactions() override
    {
        return *m_inboundTransactions;
    }

    TaggedCache<uint256, AcceptedLedger>&
    getAcceptedLedgerCache() override
    {
        return m_acceptedLedgerCache;
    }

    void
    gotTXSet(std::shared_ptr<SHAMap> const& set, bool fromAcquire)
    {
        if (set)
            m_networkOPs->mapComplete(set, fromAcquire);
    }
    TransactionMaster&
    getMasterTransaction() override
    {
        return m_txMaster;
    }
    NodeCache&
    getTempNodeCache() override
    {
        return m_tempNodeCache;
    }

    NodeStore::Database&
    getNodeStore() override
    {
        return *m_nodeStore;
    }

    NodeStore::DatabaseShard*
    getShardStore() override
    {
        return shardStore_.get();
    }

    PrometheusClient&
    getPrometheusClient() override
    {
        return *m_pPrometheusClient;
    }

    RPC::ShardArchiveHandler*
    getShardArchiveHandler(bool tryRecovery) override
    {
        static std::mutex handlerMutex;
        std::lock_guard lock(handlerMutex);

        // After constructing the handler, try to
        // initialize it. Log on error; set the
        // member variable on success.
        auto initAndSet =
            [this](std::unique_ptr<RPC::ShardArchiveHandler>&& handler) {
                if (!handler)
                    return false;

                if (!handler->init())
                {
                    JLOG(m_journal.error())
                        << "Failed to initialize ShardArchiveHandler.";

                    return false;
                }

                shardArchiveHandler_ = std::move(handler);
                return true;
            };

        // Need to resume based on state from a previous
        // run.
        if (tryRecovery)
        {
            if (shardArchiveHandler_ != nullptr)
            {
                JLOG(m_journal.error())
                    << "ShardArchiveHandler already created at startup.";

                return nullptr;
            }

            auto handler =
                RPC::ShardArchiveHandler::tryMakeRecoveryHandler(*this, *this);

            if (!initAndSet(std::move(handler)))
                return nullptr;
        }

        // Construct the ShardArchiveHandler
        if (shardArchiveHandler_ == nullptr)
        {
            auto handler =
                RPC::ShardArchiveHandler::makeShardArchiveHandler(*this, *this);

            if (!initAndSet(std::move(handler)))
                return nullptr;
        }

        return shardArchiveHandler_.get();
    }

    OrderBookDB&
    getOrderBookDB() override
    {
        return m_orderBookDB;
    }

    PathRequests&
    getPathRequests() override
    {
        return *m_pathRequests;
    }

    CachedSLEs&
    cachedSLEs() override
    {
        return cachedSLEs_;
    }

    AmendmentTable&
    getAmendmentTable() override
    {
        return *m_amendmentTable;
    }

    PreContractFace& 
    getPreContractFace() override
	{
		// return *m_preContractFace;
        return app_.getPreContractFace();
	}

    LoadFeeTrack&
    getFeeTrack() override
    {
        return *mFeeTrack;
    }

    HashRouter&
    getHashRouter() override
    {
        return *mHashRouter;
    }

    RCLValidations&
    getValidations() override
    {
        return mValidations;
    }

    ValidatorList&
    validators() override
    {
        return *validators_;
    }

    ValidatorSite&
    validatorSites() override
    {
        return *validatorSites_;
    }

    UserCertList&
    userCertList() override
    {
        return *userCertList_;
    }

    ManifestCache&
    validatorManifests() override
    {
        return *validatorManifests_;
    }

    ManifestCache&
    publisherManifests() override
    {
        return *publisherManifests_;
    }

    Cluster&
    cluster() override
    {
        return *cluster_;
    }

    PeerReservationTable&
    peerReservations() override
    {
        return *peerReservations_;
    }

    SHAMapStore&
    getSHAMapStore() override
    {
        return *m_shaMapStore;
    }

    PendingSaves&
    pendingSaves() override
    {
        return pendingSaves_;
    }

    AccountIDCache const&
    accountIDCache() const override
    {
        return accountIDCache_;
    }

    OpenLedger&
    openLedger() override
    {
        return *openLedger_;
    }

    OpenLedger const&
    openLedger() const override
    {
        return *openLedger_;
    }

    virtual OpenLedger&
    checkedOpenLedger() override
    {
        m_ledgerMaster->checkUpdateOpenLedger();
        return *openLedger_;
    }

    PeerManager&
    peerManager() override
    {
        return *m_peerManager;
    }

    TxQ&
    getTxQ() override
    {
        assert(txQ_.get() != nullptr);
        return *txQ_;
    }

    TxnDBCon&
    getTxnDB() override
    {
        assert(mTxnDB.get() != nullptr);
        return *mTxnDB;
    }

    DatabaseCon&
    getLedgerDB() override
    {
        assert(mLedgerDB.get() != nullptr);
        return *mLedgerDB;
    }

    DatabaseCon&
    getWalletDB() override
    {
        assert(mWalletDB.get() != nullptr);
        return *mWalletDB;
    }

    bool
    initSqliteDbs()
    {
        assert(mTxnDB.get() == nullptr);
        assert(mLedgerDB.get() == nullptr);
        assert(mWalletDB.get() == nullptr);

        try
        {
            auto setup = setup_DatabaseCon(*config_, m_journal);

            if (config_->useTxTables())
            {
                // transaction database
                mTxnDB = std::make_unique<TxnDBCon>(
                    setup,
                    TxDBName,
                    TxDBPragma,
                    TxDBInit,
                    DatabaseCon::CheckpointerSetup{
                        &app_.getJobQueue(),&doJobCounter(), &logs()});
                mTxnDB->getSession() << boost::str(
                    boost::format("PRAGMA cache_size=-%d;") %
                    kilobytes(config_->getValueFor(SizedItem::txnDBCache)));

                std::string cid, name, type;
                std::size_t notnull, dflt_value, pk;
                soci::indicator ind;
                {
                    // Check if Transactions has field "TxResult"
                    soci::statement st =
                        (mTxnDB->getSession().prepare
                             << ("PRAGMA table_info(Transactions);"),
                         soci::into(cid),
                         soci::into(name),
                         soci::into(type),
                         soci::into(notnull),
                         soci::into(dflt_value, ind),
                         soci::into(pk));

                    st.execute();
                    while (st.fetch())
                    {
                        if (name == "TxResult")
                        {
                            mTxnDB->setHasTxResult(true);
                            break;
                        }
                    }
                }
                if (!setup.standAlone || setup.startUp == Config::LOAD ||
                    setup.startUp == Config::LOAD_FILE ||
                    setup.startUp == Config::REPLAY)
                {
                    // Check if AccountTransactions has primary key
                    soci::statement st =
                        (mTxnDB->getSession().prepare
                             << ("PRAGMA table_info(AccountTransactions);"),
                         soci::into(cid),
                         soci::into(name),
                         soci::into(type),
                         soci::into(notnull),
                         soci::into(dflt_value, ind),
                         soci::into(pk));

                    st.execute();
                    while (st.fetch())
                    {
                        if (pk == 1)
                        {
                            JLOG(m_journal.fatal())
                                << "AccountTransactions database "
                                   "should not have a primary key";
                            return false;
                        }
                    }
                }

            }

            // ledger database
            mLedgerDB = std::make_unique<DatabaseCon>(
                setup,
                LgrDBName,
                LgrDBPragma,
                LgrDBInit,
                DatabaseCon::CheckpointerSetup{&app_.getJobQueue(), &doJobCounter(), &logs()});
            mLedgerDB->getSession() << boost::str(
                boost::format("PRAGMA cache_size=-%d;") %
                kilobytes(config_->getValueFor(SizedItem::lgrDBCache)));
            {
                std::string cid, name, type;
                std::size_t notnull, dflt_value, pk;
                soci::indicator ind;
                {
                    // Check if Transactions has field "TxResult"
                    soci::statement st =
                        (mLedgerDB->getSession().prepare
                             << ("PRAGMA table_info(Ledgers);"),
                         soci::into(cid),
                         soci::into(name),
                         soci::into(type),
                         soci::into(notnull),
                         soci::into(dflt_value, ind),
                         soci::into(pk));

                    st.execute();
                    bool bHasBloom = false;
                    while (st.fetch())
                    {
                        if (name == "Bloom")
                        {
                            bHasBloom = true;
                            break;
                        }
                    }
                    
                    if (!bHasBloom)
                    {
                        try
                        {
                            soci::statement st =
                                mLedgerDB->getSession().prepare
                                << LedgerAddBloom;
                            st.execute(true);
                        }
                        catch (soci::soci_error&)
                        {
                            JLOG(m_journal.fatal())
                                << "Ledgers database "
                                   "add bloom field failed.";
                            return false;
                            // ignore errors
                        }
                    }
                }
            }
            // wallet database
            setup.useGlobalPragma = false;
            mWalletDB = std::make_unique<DatabaseCon>(
                setup,
                WalletDBName,
                std::array<char const*, 0>(),
                WalletDBInit);
        }
        catch (std::exception const& e)
        {
            JLOG(m_journal.fatal())
                << "Failed to initialize SQLite databases: " << e.what();
            return false;
        }

        return true;
    }

    bool
    initNodeStore()
    {
        if (config_->doImport)
        {
            auto j = SchemaImp::journal("NodeObject");
            NodeStore::DummyScheduler dummyScheduler;
            RootStoppable dummyRoot{"DummyRoot"};
            std::unique_ptr<NodeStore::Database> source =
                NodeStore::Manager::instance().make_Database(
                    "NodeStore.import",
                    dummyScheduler,
                    0,
                    dummyRoot,
                    config_->section(ConfigSection::importNodeDatabase()),
                    j);

            JLOG(j.warn()) << "Starting node import from '" << source->getName()
                           << "' to '" << m_nodeStore->getName() << "'.";

            using namespace std::chrono;
            auto const start = steady_clock::now();

            m_nodeStore->import(*source);

            auto const elapsed =
                duration_cast<seconds>(steady_clock::now() - start);
            JLOG(j.warn()) << "Node import from '" << source->getName()
                           << "' took " << elapsed.count() << " seconds.";
        }

        // tune caches
        using namespace std::chrono;
        m_nodeStore->tune(
            config_->getValueFor(SizedItem::nodeCacheSize),
            seconds{config_->getValueFor(SizedItem::nodeCacheAge)});

        m_ledgerMaster->tune(
            config_->getValueFor(SizedItem::ledgerSize),
            seconds{config_->getValueFor(SizedItem::ledgerAge)});

        m_txMaster.tune (
            config_->getValueFor(SizedItem::transactionSize), 
            config_->getValueFor(SizedItem::transactionAge));
        return true;
    }

    void
    doSweep() override
    {
        if (!config_->standalone())
        {
            boost::filesystem::space_info space =
                boost::filesystem::space(config_->legacy("database_path"));

            if (space.available < megabytes(512))
            {
                JLOG(m_journal.fatal())
                    << "Remaining free disk space is less than 512MB";
                app_.signalStop();
            }

            //if (config_->useTxTables())
            //{
            //    DatabaseCon::Setup dbSetup = setup_DatabaseCon(*config_);
            //    boost::filesystem::path dbPath = dbSetup.dataDir / TxDBName;
            //    boost::system::error_code ec;
            //    boost::optional<std::uint64_t> dbSize =
            //        boost::filesystem::file_size(dbPath, ec);
            //    if (ec)
            //    {
            //        JLOG(m_journal.error())
            //            << "Error checking transaction db file size: "
            //            << ec.message();
            //        dbSize.reset();
            //    }

            //    auto db = mTxnDB->checkoutDb();
            //    static auto const pageSize = [&] {
            //        std::uint32_t ps;
            //        *db << "PRAGMA page_size;", soci::into(ps);
            //        return ps;
            //    }();
            //    static auto const maxPages = [&] {
            //        std::uint32_t mp;
            //        *db << "PRAGMA max_page_count;", soci::into(mp);
            //        return mp;
            //    }();
            //    std::uint32_t pageCount;
            //    *db << "PRAGMA page_count;", soci::into(pageCount);
            //    std::uint32_t freePages = maxPages - pageCount;
            //    std::uint64_t freeSpace =
            //        safe_cast<std::uint64_t>(freePages) * pageSize;
            //    // JLOG(m_journal.info())
            //    //	<< "Transaction DB pathname: " << dbPath.string()
            //    //	<< "; file size: " << dbSize.value_or(-1) << " bytes"
            //    //	<< "; SQLite page size: " << pageSize << " bytes"
            //    //	<< "; Free pages: " << freePages
            //    //	<< "; Free space: " << freeSpace << " bytes; "
            //    //	<< "Note that this does not take into account available
            //    // disk " 	"space.";

            //    if (freeSpace < megabytes(512))
            //    {
            //        JLOG(m_journal.fatal())
            //            << "Free SQLite space for transaction db is less than "
            //               "512MB. To fix this, rippled must be executed with "
            //               "the "
            //               "\"--vacuum\" parameter before restarting. "
            //               "Note that this activity can take multiple days, "
            //               "depending on database size.";
            //        app_.signalStop();
            //    }
            //}
        }

        // VFALCO NOTE Does the order of calls matter?
        // VFALCO TODO fix the dependency inversion using an observer,
        //         have listeners register for "onSweep ()" notification.

        nodeFamily_.sweep();
        if (shardFamily_)
            shardFamily_->sweep();
        getMasterTransaction().sweep();
        getNodeStore().sweep();
        if (shardStore_)
            shardStore_->sweep();
        getLedgerMaster().sweep();
        getTempNodeCache().sweep();
        getValidations().expire();
        getInboundLedgers().sweep();
        getConnectionPool().sweep();
        m_acceptedLedgerCache.sweep();
        cachedSLEs_.expire();

        getTableSync().Sweep();
        getTxPool().sweep();
    }

    void
    doStart() override
    {
        prepare();
        start();
    }

    bool
    checkSigs() const override
    {
        return app_.checkSigs();
    }

    void
    checkSigs(bool check) override
    {
        app_.checkSigs(check);
    }

    void
    doStop() override
    {
        JLOG(m_journal.debug()) << "Flushing validations";
        mValidations.flush();
        JLOG(m_journal.debug()) << "Validations flushed";

        validatorSites_->stop();

        caCertSites_->stop();

        // TODO Store manifests in manifests.sqlite instead of wallet.db
        validatorManifests_->save(
            getWalletDB(),
            "ValidatorManifests",
            [this](PublicKey const& pubKey) {
                return validators().listed(pubKey);
            });

        publisherManifests_->save(
            getWalletDB(),
            "PublisherManifests",
            [this](PublicKey const& pubKey) {
                return validators().trustedPublisher(pubKey);
            });

        stop(m_journal);
    }

    bool
    isShutdown() override
    {
        
        return isStopped();
    }

    JobCounter&
    doJobCounter() override
    {
        return jobCounter();
    }

    LedgerIndex
    getMaxDisallowedLedger() override
    {
        return maxDisallowedLedger_;
    }

    SchemaParams
    getSchemaParams() override
    {
        return schema_params_;
    }

    SchemaID
    schemaId() override
    {
        return schema_params_.schemaId();
    }

    SchemaManager&
    getSchemaManager() override
    {
        return app_.getSchemaManager();
    }

    bool
    getWaitinBeginConsensus() override
    {
        return waitingBeginConsensus_;
    }

    Stoppable&
    getStoppable() override
    {
        return *this;
    }

private:
    // For a newly-started validator, this is the greatest persisted ledger
    // and new validations must be greater than this.
    std::atomic<LedgerIndex> maxDisallowedLedger_{0};

    void
    setMaxDisallowedLedger();
};

bool
SchemaImp::initBeforeSetup()
{
    assert(mTxnDB == nullptr);
    if (!initSqliteDbs() || !initNodeStore())
    {
        JLOG(m_journal.fatal()) << "Cannot create database connections!";
        return false;
    }
    return true;
}

bool
SchemaImp::available()
{
    return m_schemaAvailable;
}

bool
SchemaImp::setup()
{
    if (!setSynTable())
        return false;

    setMaxDisallowedLedger();

    if (shardStore_)
    {
        shardFamily_ =
            std::make_unique<ShardFamily>(*this, app_.getCollectorManager());

        if (!shardStore_->init())
            return false;
    }

    if (!peerReservations_->load(getWalletDB()))
    {
        JLOG(m_journal.fatal()) << "Cannot find peer reservations!";
        return false;
    }

    // Configure the amendments the server supports
    {
        auto const& sa = detail::supportedAmendments();
        std::vector<std::string> saHashes;
        saHashes.reserve(sa.size());
        for (auto const& name : sa)
        {
            auto const f = getRegisteredFeature(name);
            BOOST_ASSERT(f);
            if (f)
                saHashes.push_back(to_string(*f) + " " + name);
        }
        Section supportedAmendments("Supported Amendments");
        supportedAmendments.append(saHashes);

        Section enabledAmendments = config_->section(SECTION_AMENDMENTS);

        m_amendmentTable = make_AmendmentTable(
            config().AMENDMENT_MAJORITY_TIME,
            supportedAmendments,
            enabledAmendments,
            config_->section(SECTION_VETO_AMENDMENTS),
            SchemaImp::journal("Amendments"));
    }

    Pathfinder::initPathTable();
    
    getBloomManager().init();

    auto const startUp = config_->START_UP;
    if (startUp == Config::FRESH)
    {
        JLOG(m_journal.info()) << "Starting new Ledger";

        if (!startGenesisLedger())
            return false;
    }
    else if (
        startUp == Config::LOAD || startUp == Config::LOAD_FILE ||
        startUp == Config::REPLAY)
    {
        JLOG(m_journal.info()) << "Loading specified Ledger";

        if (!loadOldLedger(
                config_->START_LEDGER,
                startUp == Config::REPLAY,
                startUp == Config::LOAD_FILE))
        {
            JLOG(m_journal.error())
                << "The specified ledger could not be loaded.";
            return false;
        }
    }
    else if (startUp == Config::NETWORK)
    {
        // This should probably become the default once we have a stable
        // network.
        if (!config_->standalone())
            m_networkOPs->setNeedNetworkLedger();

        if (!startGenesisLedger())
            return false;
    }
    else
    {
        static const unsigned TRY_ANCESTOR = 3;

        unsigned offset = 0;
        for (offset = 0; offset < TRY_ANCESTOR; ++offset)
        {
            if (getLastFullLedger(offset) != nullptr)
            {
                if (loadOldLedger(
                        config_->START_LEDGER,
                        startUp == Config::REPLAY,
                        startUp == Config::LOAD_FILE,
                        offset))
                {
                    break;
                }

                JLOG(m_journal.error()) << "Load old ledger failed for schema: "
                                        << to_string(schemaId());

                if (config_->START_LEDGER.empty() ||
                    boost::iequals(config_->START_LEDGER, "latest"))
                {
                    JLOG(m_journal.warn()) << "Try ancestor " << offset + 1;
                }
                else
                {
                    return false;
                }
            }
        }

        if (offset >= TRY_ANCESTOR)
        {
            // try load firstly
            if (startUp == Config::NEWCHAIN_WITHSTATE)
            {
                auto validLedger = app_.getLedgerMaster().getLedgerByHash(
                    schema_params_.anchor_ledger_hash);

                if (validLedger == nullptr)
                {
                    JLOG(m_journal.warn())
                        << "start up new chain with state error: ledger "
                        << to_string(schema_params_.anchor_ledger_hash)
                        << " not found! ";
                    return false;
                }
                JLOG(m_journal.info())
                    << "NEWCHAIN_WITHSTATE from ledger="
                    << to_string(schema_params_.anchor_ledger_hash);

                if (!startGenesisLedger(validLedger))
                {
                    return false;
                }
            }
            else
            {
                if (!startGenesisLedger())
                    return false;
            }            
        }
    }

    //m_orderBookDB.setup(getLedgerMaster().getCurrentLedger());

    if (!cluster_->load(config().section(SECTION_CLUSTER_NODES)))
    {
        JLOG(m_journal.fatal()) << "Invalid entry in cluster configuration.";
        return false;
    }

    {
        if (app_.getValidatorKeys().configInvalid())
            return false;

        if (!validatorManifests_->load(
                getWalletDB(),
                "ValidatorManifests",
                app_.getValidatorKeys().manifest,
                config().section(SECTION_VALIDATOR_KEY_REVOCATION).values()))
        {
            JLOG(m_journal.fatal()) << "Invalid configured validator manifest.";
            return false;
        }

        publisherManifests_->load(getWalletDB(), "PublisherManifests");

        // Setup trusted validators
        if (!validators_->load(
                config_->ONLY_VALIDATE_FOR_SCHEMA
                    ? PublicKey()
                    : app_.getValidationPublicKey(),
                config().section(SECTION_VALIDATORS).values(),
                config().section(SECTION_VALIDATOR_LIST_KEYS).values()))
        {
            JLOG(m_journal.fatal())
                << "Invalid entry in validator configuration.";
            return false;
        }
    }

    if (schemaId() == beast::zero)
    {
        if (!validatorSites_->load(
                config().section(SECTION_VALIDATOR_LIST_SITES).values()))
        {
            JLOG(m_journal.fatal())
                << "Invalid entry in [" << SECTION_VALIDATOR_LIST_SITES << "]";
            return false;
        }
        else
        {
            m_networkOPs->setGenesisLedgerIndex(
                m_ledgerMaster->getClosedLedger()->info().seq);
            waitingBeginConsensus_ = true;
        }
    }

     if (!caCertSites_->load(
            config().section(SECTION_CACERTS_LIST_KEYS).values(),
            config().section(SECTION_CACERTS_LIST_SITES).values()))
    {
        JLOG(m_journal.fatal())
            << "Invalid entry in [" << SECTION_CACERTS_LIST_SITES << "]";
        return false;
    }
    //----------------------------------------------------------------------
    //
    // Server
    //
    //----------------------------------------------------------------------

    if (!config_->standalone())
    {
        // validation and node import require the sqlite db
        if (config_->nodeToShard && !nodeToShards())
            return false;

        if (config_->validateShards && !validateShards())
            return false;
    }

    if (schemaId() == beast::zero)
    {
        validatorSites_->start();
    }

    caCertSites_->start();

    // start first consensus round
    if (config().section(SECTION_VALIDATOR_LIST_SITES).values().size() == 0 ||
        schemaId() != beast::zero)
    {
        m_networkOPs->setGenesisLedgerIndex(
            m_ledgerMaster->getClosedLedger()->info().seq);
        if (!m_networkOPs->beginConsensus(
                m_ledgerMaster->getClosedLedger()->info().hash))
        {
            JLOG(m_journal.fatal()) << "Unable to start consensus";
            return false;
        }
    }

    // Begin connecting to network.
    if (!config_->standalone())
    {
        // Should this message be here, conceptually? In theory this sort
        // of message, if displayed, should be displayed from PeerFinder.
        if (config_->PEER_PRIVATE && config_->IPS_FIXED.empty())
        {
            JLOG(m_journal.warn())
                << "No outbound peer connections will be made";
        }

        // VFALCO NOTE the state timer resets the deadlock detector.
        //
        m_networkOPs->setStateTimer();
    }
    else
    {
        JLOG(m_journal.warn()) << "Running in standalone mode";

        m_networkOPs->setStandAlone();
    }

    if (config_->canSign())
    {
        JLOG(m_journal.warn()) << "*** The server is configured to allow the "
                                  "'sign' and 'sign_for'";
        JLOG(m_journal.warn()) << "*** commands. These commands have security "
                                  "implications and have";
        JLOG(m_journal.warn()) << "*** been deprecated. They will be removed "
                                  "in a future release of";
        JLOG(m_journal.warn()) << "*** rippled.";
        JLOG(m_journal.warn()) << "*** If you do not use them to sign "
                                  "transactions please edit your";
        JLOG(m_journal.warn())
            << "*** configuration file and remove the [enable_signing] stanza.";
        JLOG(m_journal.warn()) << "*** If you do use them to sign transactions "
                                  "please migrate to a";
        JLOG(m_journal.warn())
            << "*** standalone signing solution as soon as possible.";
    }

    //
    // Execute start up rpc commands.
    //
    for (auto cmd : config_->section(SECTION_RPC_STARTUP).lines())
    {
        Json::Reader jrReader;
        Json::Value jvCommand;

        if (!jrReader.parse(cmd, jvCommand))
        {
            JLOG(m_journal.fatal()) << "Couldn't parse entry in ["
                                    << SECTION_RPC_STARTUP << "]: '" << cmd;
        }

        if (!config_->quiet())
        {
            JLOG(m_journal.fatal())
                << "Startup RPC: " << jvCommand << std::endl;
        }

        Resource::Charge loadType = Resource::feeReferenceRPC;
        Resource::Consumer c;
        RPC::JsonContext context{{journal("RPCHandler"),
                                  *this,
                                  loadType,
                                  getOPs(),
                                  getLedgerMaster(),
                                  c,
                                  Role::ADMIN,
                                  {},
                                  {},
                                  RPC::ApiMaximumSupportedVersion},
                                 jvCommand};

        Json::Value jvResult;
        RPC::doCommand(context, jvResult);

        if (!config_->quiet())
        {
            JLOG(m_journal.fatal()) << "Result: " << jvResult << std::endl;
        }
    }

    m_pPrometheusClient->setup();
    m_schemaAvailable = true;

    doStart();

    return true;
}

bool
SchemaImp::startGenesisLedger()
{
    if (boost::none == config_->CHAINID)
    {
        JLOG(m_journal.fatal()) << "chainID mot configured in cfg.";
        return false;
    }
    std::vector<uint256> initialAmendments =
        (config_->START_UP == Config::FRESH) ? m_amendmentTable->getDesired()
                                             : std::vector<uint256>{};

    std::shared_ptr<Ledger> const genesis = std::make_shared<Ledger>(
        create_genesis, *config_, initialAmendments, nodeFamily_);
    m_ledgerMaster->storeLedger(genesis);

    genesis->setImmutable(*config_);
    openLedger_.emplace(genesis, cachedSLEs_, SchemaImp::journal("OpenLedger"));
    m_ledgerMaster->switchLCL(genesis);
    // set valid ledger
    m_ledgerMaster->initGenesisLedger(genesis);
    return true;
    /*
    auto const next = std::make_shared<Ledger>(
            *genesis, timeKeeper().closeTime());
    next->updateSkipList();

    //store ledger 2 account_node
    next->stateMap().flushDirty(
            hotACCOUNT_NODE, next->info().seq);
    next->setImmutable (config_);
    openLedger_.emplace(next, cachedSLEs_,
            SchemaImp::journal("OpenLedger"));
    m_ledgerMaster->storeLedger(next);
    m_ledgerMaster->switchLCL (next);
    */
}

std::shared_ptr<Ledger>
SchemaImp::getLastFullLedger(unsigned offset)
{
    auto j = app_.journal("Ledger");

    try
    {
        auto const [ledger, seq, hash] = loadLedgerHelper(
            (boost::format("order by LedgerSeq desc limit %1%,1") % offset)
                .str(),
            *this);

        if (!ledger)
            return {};

        ledger->setImmutable(*config_);

        if (getLedgerMaster().haveLedger(seq))
            ledger->setValidated();

        if (ledger->info().hash == hash)
        {
            JLOG(j.trace()) << "Loaded ledger: " << hash;
            return ledger;
        }

        if (auto stream = j.error())
        {
            stream << "Failed on ledger";
            Json::Value p;
            addJson(p, {*ledger, LedgerFill::full});
            stream << p;
        }
    }
    catch (SHAMapMissingNode& mn)
    {
        JLOG(j.warn()) << "Ledger with missing nodes in database: "
                       << mn.what();
    }
    return {};
}

std::shared_ptr<Ledger>
SchemaImp::loadLedgerFromFile(std::string const& name)
{
    try
    {
        std::ifstream ledgerFile(name, std::ios::in);

        if (!ledgerFile)
        {
            JLOG(m_journal.fatal()) << "Unable to open file '" << name << "'";
            return nullptr;
        }

        Json::Reader reader;
        Json::Value jLedger;

        if (!reader.parse(ledgerFile, jLedger))
        {
            JLOG(m_journal.fatal()) << "Unable to parse ledger JSON";
            return nullptr;
        }

        std::reference_wrapper<Json::Value> ledger(jLedger);

        // accept a wrapped ledger
        if (ledger.get().isMember("result"))
            ledger = ledger.get()["result"];

        if (ledger.get().isMember("ledger"))
            ledger = ledger.get()["ledger"];

        std::uint32_t seq = 1;
        auto closeTime = timeKeeper().closeTime();
        using namespace std::chrono_literals;
        auto closeTimeResolution = 30s;
        bool closeTimeEstimated = false;
        std::uint64_t totalDrops = 0;

        if (ledger.get().isMember("accountState"))
        {
            if (ledger.get().isMember(jss::ledger_index))
            {
                seq = ledger.get()[jss::ledger_index].asUInt();
            }

            if (ledger.get().isMember("close_time"))
            {
                using tp = NetClock::time_point;
                using d = tp::duration;
                closeTime = tp{d{ledger.get()["close_time"].asUInt()}};
            }
            if (ledger.get().isMember("close_time_resolution"))
            {
                using namespace std::chrono;
                closeTimeResolution =
                    seconds{ledger.get()["close_time_resolution"].asUInt()};
            }
            if (ledger.get().isMember("close_time_estimated"))
            {
                closeTimeEstimated =
                    ledger.get()["close_time_estimated"].asBool();
            }
            if (ledger.get().isMember("total_coins"))
            {
                totalDrops = beast::lexicalCastThrow<std::uint64_t>(
                    ledger.get()["total_coins"].asString());
            }

            ledger = ledger.get()["accountState"];
        }

        if (!ledger.get().isArrayOrNull())
        {
            JLOG(m_journal.fatal()) << "State nodes must be an array";
            return nullptr;
        }

        auto loadLedger =
            std::make_shared<Ledger>(seq, closeTime, *config_, nodeFamily_);
        loadLedger->setTotalDrops(totalDrops);

        for (Json::UInt index = 0; index < ledger.get().size(); ++index)
        {
            Json::Value& entry = ledger.get()[index];

            if (!entry.isObjectOrNull())
            {
                JLOG(m_journal.fatal()) << "Invalid entry in ledger";
                return nullptr;
            }

            uint256 uIndex;

            if (!uIndex.SetHex(entry[jss::index].asString()))
            {
                JLOG(m_journal.fatal()) << "Invalid entry in ledger";
                return nullptr;
            }

            entry.removeMember(jss::index);

            STParsedJSONObject stp("sle", ledger.get()[index]);

            if (!stp.object || uIndex.isZero())
            {
                JLOG(m_journal.fatal()) << "Invalid entry in ledger";
                return nullptr;
            }

            // VFALCO TODO This is the only place that
            //             constructor is used, try to remove it
            STLedgerEntry sle(*stp.object, uIndex);

            if (!loadLedger->addSLE(sle))
            {
                JLOG(m_journal.fatal())
                    << "Couldn't add serialized ledger: " << uIndex;
                return nullptr;
            }
        }

        loadLedger->stateMap().flushDirty(
            hotACCOUNT_NODE, loadLedger->info().seq);

        loadLedger->setAccepted(
            closeTime, closeTimeResolution, !closeTimeEstimated, *config_);

        return loadLedger;
    }
    catch (std::exception const& x)
    {
        JLOG(m_journal.fatal()) << "Ledger contains invalid data: " << x.what();
        return nullptr;
    }
}

bool
SchemaImp::loadOldLedger(
    std::string const& ledgerID,
    bool replay,
    bool isFileName,
    unsigned offset)
{
    try
    {
        std::shared_ptr<Ledger const> loadLedger, replayLedger;

        if (isFileName)
        {
            if (!ledgerID.empty())
                loadLedger = loadLedgerFromFile(ledgerID);
        }
        else if (ledgerID.length() == 64)
        {
            uint256 hash;

            if (hash.SetHex(ledgerID))
            {
                loadLedger = loadByHash(hash, *this);

                if (!loadLedger)
                {
                    // Try to build the ledger from the back end
                    auto il = std::make_shared<InboundLedger>(
                        *this,
                        hash,
                        0,
                        InboundLedger::Reason::GENERIC,
                        stopwatch());
                    if (il->checkLocal())
                        loadLedger = il->getLedger();
                }
            }
        }
        else if (ledgerID.empty() || boost::iequals(ledgerID, "latest"))
        {
            loadLedger = getLastFullLedger(offset);
        }
        else
        {
            // assume by sequence
            std::uint32_t index;

            if (beast::lexicalCastChecked(index, ledgerID))
                loadLedger = loadByIndex(index, *this);
        }

        if (!loadLedger)
            return false;

        if (replay)
        {
            // Replay a ledger close with same prior ledger and transactions

            // this ledger holds the transactions we want to replay
            replayLedger = loadLedger;

            JLOG(m_journal.info()) << "Loading parent ledger";

            loadLedger = loadByHash(replayLedger->info().parentHash, *this);
            if (!loadLedger)
            {
                JLOG(m_journal.info())
                    << "Loading parent ledger from node store";

                // Try to build the ledger from the back end
                auto il = std::make_shared<InboundLedger>(
                    *this,
                    replayLedger->info().parentHash,
                    0,
                    InboundLedger::Reason::GENERIC,
                    stopwatch());

                if (il->checkLocal())
                    loadLedger = il->getLedger();

                if (!loadLedger)
                {
                    JLOG(m_journal.fatal()) << "Replay ledger missing/damaged";
                    assert(false);
                    return false;
                }
            }
        }
        // using namespace std::chrono_literals;
        // using namespace date;
        // static constexpr NetClock::time_point ledgerWarnTimePoint{
        //	sys_days{January / 1 / 2018} -sys_days{January / 1 / 2000} };
        // if (loadLedger->info().closeTime < ledgerWarnTimePoint)
        //{
        //	JLOG(m_journal.fatal())
        //		<< "\n\n***  WARNING   ***\n"
        //		"You are replaying a ledger from before "
        //		<< to_string(ledgerWarnTimePoint)
        //		<< " UTC.\n"
        //		"This replay will not handle your ledger as it was "
        //		"originally "
        //		"handled.\nConsider running an earlier version of rippled
        //" 		"to " 		"get the older rules.\n*** CONTINUING ***\n";
        //}

        JLOG(m_journal.info()) << "Loading ledger " << loadLedger->info().hash
                               << " seq:" << loadLedger->info().seq;

        if (loadLedger->info().accountHash.isZero())
        {
            JLOG(m_journal.fatal()) << "Ledger is empty.";
            assert(false);
            return false;
        }

        m_ledgerMaster->setLoadLedger(loadLedger->info().seq);
        // if (!loadLedger->walkLedger(app_.journal("Ledger")))
        // {
        //     JLOG(m_journal.fatal()) << "Ledger is missing nodes.";
        //     assert(false);
        //     return false;
        // }
        

        if (!loadLedger->assertSane(app_.journal("Ledger")))
        {
            JLOG(m_journal.fatal()) << "Ledger is not sane.";
            assert(false);
            return false;
        }

        m_ledgerMaster->onLastFullLedgerLoaded(loadLedger);
        openLedger_.emplace(
            loadLedger, cachedSLEs_, SchemaImp::journal("OpenLedger"));

        if (replay)
        {
            // inject transaction(s) from the replayLedger into our open ledger
            // and build replay structure
            auto replayData =
                std::make_unique<LedgerReplay>(loadLedger, replayLedger);

            for (auto const& [_, tx] : replayData->orderedTxns())
            {
                (void)_;
                auto txID = tx->getTransactionID();

                auto s = std::make_shared<Serializer>();
                tx->add(*s);

                forceValidity(getHashRouter(), txID, Validity::SigGoodOnly);

                openLedger_->modify(
                    [&txID, &s](OpenView& view, beast::Journal j) {
                        view.rawTxInsert(txID, std::move(s), nullptr);
                        return true;
                    });
            }

            m_ledgerMaster->takeReplay(std::move(replayData));
        }
    }
    catch (SHAMapMissingNode const& mn)
    {
        JLOG(m_journal.fatal())
            << "While loading specified ledger: " << mn.what();
        return false;
    }
    catch (boost::bad_lexical_cast&)
    {
        JLOG(m_journal.fatal())
            << "Ledger specified '" << ledgerID << "' is not valid";
        return false;
    }

    return true;
}

bool
SchemaImp::startGenesisLedger(std::shared_ptr<Ledger const> loadLedger)
{
    assert(loadLedger);

    loadLedger->stateMap().invariants();

    if (!loadLedger->walkLedger(app_.journal("Ledger")))
    {
        JLOG(m_journal.fatal()) << "Ledger is missing nodes.";
        return false;
    }

    if (!loadLedger->assertSane(app_.journal("Ledger")))
    {
        JLOG(m_journal.fatal()) << "Ledger is not sane.";
        return false;
    }

    auto genesis = std::make_shared<Ledger>(*loadLedger, nodeFamily_, schema_params_.schemaId());
    genesis->setImmutable(*config_);

    openLedger_.emplace(genesis, cachedSLEs_, SchemaImp::journal("OpenLedger"));
    m_ledgerMaster->switchLCL(genesis);

    // set valid ledger
    m_ledgerMaster->initGenesisLedger(genesis);

    return true;
}

//static std::vector<std::string>
//getSchema(DatabaseCon& dbc, std::string const& dbName)
//{
//    std::vector<std::string> schema;
//    schema.reserve(32);
//
//    std::string sql = "SELECT sql FROM sqlite_master WHERE tbl_name='";
//    sql += dbName;
//    sql += "';";
//
//    std::string r;
//    soci::statement st = (dbc.getSession().prepare << sql, soci::into(r));
//    st.execute();
//    while (st.fetch())
//    {
//        schema.emplace_back(r);
//    }
//
//    return schema;
//}

//static bool
//schemaHas(
//    DatabaseCon& dbc,
//    std::string const& dbName,
//    int line,
//    std::string const& content,
//    beast::Journal j)
//{
//    std::vector<std::string> schema = getSchema(dbc, dbName);
//
//    if (static_cast<int>(schema.size()) <= line)
//    {
//        JLOG(j.fatal()) << "Schema for " << dbName << " has too few lines";
//        Throw<std::runtime_error>("bad schema");
//    }
//
//    return schema[line].find(content) != std::string::npos;
//}

bool
SchemaImp::nodeToShards()
{
    assert(!config_->standalone());

    if (config_->section(ConfigSection::shardDatabase()).empty())
    {
        JLOG(m_journal.fatal())
            << "The [shard_db] configuration setting must be set";
        return false;
    }
    if (!shardStore_)
    {
        JLOG(m_journal.fatal()) << "Invalid [shard_db] configuration";
        return false;
    }
    shardStore_->import(getNodeStore());
    return true;
}

bool
SchemaImp::validateShards()
{
    assert(!config_->standalone());

    if (config_->section(ConfigSection::shardDatabase()).empty())
    {
        JLOG(m_journal.fatal())
            << "The [shard_db] configuration setting must be set";
        return false;
    }
    if (!shardStore_)
    {
        JLOG(m_journal.fatal()) << "Invalid [shard_db] configuration";
        return false;
    }
    shardStore_->validate();
    return true;
}

void
SchemaImp::setMaxDisallowedLedger()
{
    boost::optional<LedgerIndex> seq;
    {
        auto db = getLedgerDB().checkoutDb();
        *db << "SELECT MAX(LedgerSeq) FROM Ledgers;", soci::into(seq);
    }
    if (seq)
        maxDisallowedLedger_ = *seq;

    JLOG(m_journal.trace())
        << "Max persisted ledger is " << maxDisallowedLedger_;
}

bool
SchemaImp::setSynTable()
{
    auto conn = m_pTxStoreDBConn->GetDBConn();

    DatabaseCon::Setup setup = ripple::setup_SyncDatabaseCon(*config_);
    // sync_db not configured
    if (setup.sync_db.name() == "" && setup.sync_db.lines().size() == 0)
        return true;

    // sync_db configured,but not connected
    if (conn == nullptr || conn->getSession().get_backend() == NULL)
    {
        m_pTableSync->SetHaveSyncFlag(false);
        m_pTableStorage->SetHaveSyncFlag(false);
        // JLOG(m_journal.trace()) << "make db backends error,please check
        // cfg!"; //can not log now!
        std::cerr << "make db backends error,please check cfg!" << std::endl;
        return false;
    }
    else
    {
        std::pair<std::string, bool> result = setup.sync_db.find("type");

        if (result.first.compare("sqlite") == 0 || result.first.empty() ||
            !result.second)
            m_pTableStatusDB =
                std::make_unique<TableStatusDBSQLite>(conn, this, m_journal);
        else
            m_pTableStatusDB =
                std::make_unique<TableStatusDBMySQL>(conn, this, m_journal);

        if (m_pTableStatusDB)
        {
            bool bInitRet = m_pTableStatusDB->InitDB(setup);
            if (bInitRet)
                // JLOG(m_journal.info()) << "InitDB success";
                std::cout << "InitDB success" << std::endl;
            else
            {
                // JLOG(m_journal.info()) << "InitDB error";
                std::cerr << "InitDB error" << std::endl;
                return false;
            }
        }
        else
        {
            m_pTableSync->SetHaveSyncFlag(false);
            m_pTableStorage->SetHaveSyncFlag(false);
            // JLOG(m_journal.info()) << "fail to create sycstate table calss.";
            std::cerr << "fail to create sycstate table calss." << std::endl;
            return false;
        }

        if (m_txMaster.getClientTxStoreDBConn().GetDBConn() == NULL ||
            m_txMaster.getConsensusTxStoreDBConn().GetDBConn() == NULL)
        {
            std::cerr << "db connection for consensus or tx check is null"
                      << std::endl;
            return false;
        }
    }
    return true;
}


std::shared_ptr<Schema>
make_Schema(
    SchemaParams const& params,
    std::shared_ptr<Config> config,
    Application& app,
    beast::Journal j)
{
    return std::make_shared<SchemaImp>(params, config, app, j);
}
}  // namespace ripple
