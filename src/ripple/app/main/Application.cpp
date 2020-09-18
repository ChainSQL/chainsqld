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
#include <ripple/basics/Sustain.h>
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
#include <ripple/beast/asio/io_latency_probe.h>
#include <ripple/beast/core/LexicalCast.h>
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
#include <peersafe/app/misc/StateManager.h>
#include <peersafe/schema/SchemaManager.h>
#include <peersafe/schema/Schema.h>
#include <openssl/evp.h>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>

namespace ripple {



//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

// VFALCO TODO Move the function definitions into the class declaration
class ApplicationImp
    : public Application
    , public RootStoppable
    , public BasicApp
{
private:
    class io_latency_sampler
    {
    private:
        beast::insight::Event m_event;
        beast::Journal m_journal;
        beast::io_latency_probe <std::chrono::steady_clock> m_probe;
        std::atomic<std::chrono::milliseconds> lastSample_;

    public:
        io_latency_sampler (
            beast::insight::Event ev,
            beast::Journal journal,
            std::chrono::milliseconds interval,
            boost::asio::io_service& ios)
            : m_event (ev)
            , m_journal (journal)
            , m_probe (interval, ios)
            , lastSample_ {}
        {
        }

        void
        start()
        {
            m_probe.sample (std::ref(*this));
        }

        template <class Duration>
        void operator() (Duration const& elapsed)
        {
            using namespace std::chrono;
            auto const lastSample = date::ceil<milliseconds>(elapsed);

            lastSample_ = lastSample;

            if (lastSample >= 10ms)
                m_event.notify (lastSample);
            if (lastSample >= 500ms)
            {
                JLOG(m_journal.warn()) <<
                    "io_service latency = " << lastSample.count();
            }
        }

        std::chrono::milliseconds
        get () const
        {
            return lastSample_.load();
        }

        void
        cancel ()
        {
            m_probe.cancel ();
        }

        void cancel_async ()
        {
            m_probe.cancel_async ();
        }
    };

public:
	std::unique_ptr<Config>		config_;
	std::unique_ptr<Logs> logs_;
	beast::Journal m_journal;
	std::unique_ptr<TimeKeeper> timeKeeper_;
	std::unique_ptr <CollectorManager>	m_collectorManager;
	std::unique_ptr <Resource::Manager> m_resourceManager;
	std::unique_ptr <SchemaManager>		m_schemaManager;
	std::unique_ptr <NodeStoreScheduler>	m_nodeStoreScheduler;
	std::unique_ptr <perf::PerfLog>		perfLog_;
	std::unique_ptr <JobQueue>			m_jobQueue;
	std::unique_ptr <LoadManager>		m_loadManager;
	std::unique_ptr <ServerHandler>		serverHandler_;
	std::unique_ptr <ResolverAsio>		m_resolver;

    Application::MutexType m_masterMutex;

    ClosureCounter<void, boost::system::error_code const&> waitHandlerCounter_;
    boost::asio::steady_timer sweepTimer_;
    boost::asio::steady_timer entropyTimer_;
    bool startTimers_;
    boost::asio::signal_set m_signals;
    std::condition_variable cv_;
    std::mutex mut_;
    bool isTimeToStop = false;
    std::atomic<bool> checkSigs_;
    io_latency_sampler m_io_latency_sampler;
	std::pair<PublicKey, SecretKey>		nodeIdentity_;
	ValidatorKeys const validatorKeys_;
    //--------------------------------------------------------------------------

    static
    std::size_t
    numberOfThreads(Config const& config)
    {
    #if RIPPLE_SINGLE_IO_SERVICE_THREAD
        return 1;
    #else
        return (config.NODE_SIZE >= 2) ? 2 : 1;
    #endif
    }

    //--------------------------------------------------------------------------

	ApplicationImp(
		std::unique_ptr<Config> config,
		std::unique_ptr<Logs> logs,
		std::unique_ptr<TimeKeeper> timeKeeper)
		: RootStoppable("Application")
		, BasicApp(numberOfThreads(*config))
		, config_(std::move(config))
		, logs_(std::move(logs))
		, m_journal(logs_->journal("Application"))

		, timeKeeper_(std::move(timeKeeper))
		// PerfLog must be started before any other threads are launched.
		, perfLog_(perf::make_PerfLog(
			perf::setup_PerfLog(config_->section("perf"), config_->CONFIG_DIR),
			*this, logs_->journal("PerfLog"), [this]() { signalStop(); }))

		, m_collectorManager(CollectorManager::New(
			config_->section(SECTION_INSIGHT), logs_->journal("Collector")))

		, m_resourceManager(Resource::make_Manager(
			m_collectorManager->collector(), logs_->journal("Resource")))

		// The JobQueue has to come pretty early since
		// almost everything is a Stoppable child of the JobQueue.
		//
		, m_jobQueue(std::make_unique<JobQueue>(
			m_collectorManager->group("jobq"), *m_nodeStoreScheduler,
			logs_->journal("JobQueue"), *logs_, *perfLog_))

		, serverHandler_(make_ServerHandler(*this, *this, get_io_service(),
			*m_jobQueue, *m_resourceManager,
			*m_collectorManager))

		, m_nodeStoreScheduler(std::make_unique<NodeStoreScheduler>(*this))

		, m_loadManager(make_LoadManager(*this, *this, logs_->journal("LoadManager")))

		, m_schemaManager(std::make_unique<SchemaManager>(
			*this, logs_->journal("SchemaManager")))

        , sweepTimer_ (get_io_service())

        , entropyTimer_ (get_io_service())

        , startTimers_ (false)

        , m_signals (get_io_service())

        , checkSigs_(true)

		, validatorKeys_(*config_, m_journal)

        , m_resolver (ResolverAsio::New (get_io_service(), logs_->journal("Resolver")))

        , m_io_latency_sampler (m_collectorManager->collector()->make_event ("ios_latency"),
            logs_->journal("Application"), std::chrono::milliseconds (100), get_io_service())
    {

        add (m_resourceManager.get ());

        //
        // VFALCO - READ THIS!
        //
        //  Do not start threads, open sockets, or do any sort of "real work"
        //  inside the constructor. Put it in onStart instead. Or if you must,
        //  put it in setup (but everything in setup should be moved to onStart
        //  anyway.
        //
        //  The reason is that the unit tests require an Application object to
        //  be created. But we don't actually start all the threads, sockets,
        //  and services when running the unit tests. Therefore anything which
        //  needs to be stopped will not get stopped correctly if it is
        //  started in this constructor.
        //

        // VFALCO HACK
        m_nodeStoreScheduler->setJobQueue (*m_jobQueue);

        //add (m_ledgerMaster->getPropertySource ());

		logs_->setApplication(this);
    }

    //--------------------------------------------------------------------------

    bool setup() override;
    void doStart(bool withTimers) override;
    void run() override;
    bool isShutdown() override;
    void signalStop() override;
    bool checkSigs() const override;
    void checkSigs(bool) override;
    int fdlimit () const override;

    //--------------------------------------------------------------------------

    Logs&
    logs() override
    {
        return *logs_;
    }

	std::shared_ptr<Schema> getSchema(SchemaID const& id)override
	{
		if (m_schemaManager->contains(id))
			return m_schemaManager->getSchema(id);
		else
			return nullptr;
	}

    Config&
    config(SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
        return m_schemaManager->getSchema(id)->config();
    }

    CollectorManager& getCollectorManager () override
    {
        return *m_collectorManager;
    }

	NodeStoreScheduler& nodeStoreScheduler() override
	{
		return *m_nodeStoreScheduler;
	}

	Family& family(SchemaID const& id) override
	{
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->family();
	}

	Family* shardFamily(SchemaID const& id) override
	{
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->shardFamily();
	}

    TimeKeeper&
    timeKeeper() override
    {
        return *timeKeeper_;
    }

    JobQueue& getJobQueue () override
    {
        return *m_jobQueue;
    }

    std::pair<PublicKey, SecretKey> const&
    nodeIdentity () override
    {
        return nodeIdentity_;
    }

	TxStoreDBConn& getTxStoreDBConn(SchemaID const& id) override
	{
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->getTxStoreDBConn();
	}

	TxStore& getTxStore(SchemaID const& id) override
	{
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->getTxStore();
	}

    TableStatusDB& getTableStatusDB(SchemaID const& id) override
	{
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->getTableStatusDB();
	}

    TableSync& getTableSync(SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->getTableSync();
    }

    TableStorage& getTableStorage(SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->getTableStorage();
    }

	TableAssistant& getTableAssistant(SchemaID const& id) override
	{
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->getTableAssistant();
	}

	ContractHelper& getContractHelper(SchemaID const& id) override
	{
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->getContractHelper();
	}

	TableTxAccumulator& getTableTxAccumulator(SchemaID const& id) override
	{
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->getTableTxAccumulator();
	}

    TxPool& getTxPool(SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->getTxPool();
    }

	StateManager& getStateManager(SchemaID const& id) override
	{
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->getStateManager();
	}

    virtual
    PublicKey const &
    getValidationPublicKey() const override
    {
        return validatorKeys_.publicKey;
    }

	ValidatorKeys const& getValidatorKeys()const override
	{
		return validatorKeys_;
	}

	ResolverAsio& getResolver() override
	{
		return *m_resolver;
	}

	ServerHandler& getServerHandler()override
	{
		return *serverHandler_;
	}

	SchemaManager&	getSchemaManager() override
	{
		return *m_schemaManager;
	}

    NetworkOPs& getOPs (SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->getOPs();
    }

    boost::asio::io_service& getIOService () override
    {
        return get_io_service();
    }

    std::chrono::milliseconds getIOLatency () override
    {
        return m_io_latency_sampler.get ();
    }

    LedgerMaster& getLedgerMaster (SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->getLedgerMaster();
    }

    InboundLedgers& getInboundLedgers (SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->getInboundLedgers();
    }

    InboundTransactions& getInboundTransactions (SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->getInboundTransactions();
    }

    TaggedCache <uint256, AcceptedLedger>& getAcceptedLedgerCache (SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->getAcceptedLedgerCache();
    }

    TransactionMaster& getMasterTransaction (SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->getMasterTransaction();
    }

    perf::PerfLog& getPerfLog () override
    {
        return *perfLog_;
    }

    NodeCache& getTempNodeCache (SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->getTempNodeCache();
    }

    NodeStore::Database& getNodeStore (SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->getNodeStore();
    }

    NodeStore::DatabaseShard* getShardStore (SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->getShardStore();
    }

    Application::MutexType& getMasterMutex () override
    {
        return m_masterMutex;
    }

    LoadManager& getLoadManager () override
    {
        return *m_loadManager;
    }

    Resource::Manager& getResourceManager () override
    {
        return *m_resourceManager;
    }

    OrderBookDB& getOrderBookDB (SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->getOrderBookDB();
    }

    PathRequests& getPathRequests (SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->getPathRequests();
    }

    CachedSLEs&
    cachedSLEs(SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->cachedSLEs();
    }

    AmendmentTable& getAmendmentTable(SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->getAmendmentTable();
    }

    LoadFeeTrack& getFeeTrack (SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->getFeeTrack();
    }

    HashRouter& getHashRouter (SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->getHashRouter();
    }

    RCLValidations& getValidations (SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->getValidations();
    }

    ValidatorList& validators (SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->validators();
    }

    ValidatorSite& validatorSites (SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->validatorSites();
    }

	CertList& certList(SchemaID const& id) override
	{
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->certList();;
	}
	
    ManifestCache& validatorManifests(SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->validatorManifests();
    }

    ManifestCache& publisherManifests(SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->publisherManifests();
    }

    Cluster& cluster (SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->cluster();
    }

    SHAMapStore& getSHAMapStore (SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->getSHAMapStore();
    }

    PendingSaves& pendingSaves(SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->pendingSaves();
    }

    AccountIDCache const&
    accountIDCache(SchemaID const& id) const override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->accountIDCache();
    }

    OpenLedger&
    openLedger(SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->openLedger();
    }

    OpenLedger const&
    openLedger(SchemaID const& id) const override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->openLedger();
    }

    Overlay& overlay (SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->overlay();
    }

    TxQ& getTxQ(SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->getTxQ();
    }

    DatabaseCon& getTxnDB (SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->getTxnDB();
    }
    DatabaseCon& getLedgerDB (SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->getLedgerDB();
    }
    DatabaseCon& getWalletDB (SchemaID const& id) override
    {
		assert(m_schemaManager->contains(id));
		return m_schemaManager->getSchema(id)->getWalletDB();
    }

    bool serverOkay (std::string& reason) override;

    beast::Journal journal (std::string const& name) override;

    //--------------------------------------------------------------------------
    void signalled(const boost::system::error_code& ec, int signal_number)
    {
        if (ec == boost::asio::error::operation_aborted)
        {
            // Indicates the signal handler has been aborted
            // do nothing
        }
        else if (ec)
        {
            JLOG(m_journal.error()) << "Received signal: " << signal_number
                                  << " with error: " << ec.message();
        }
        else
        {
            JLOG(m_journal.debug()) << "Received signal: " << signal_number;
            signalStop();
        }
    }

    //--------------------------------------------------------------------------
    //
    // Stoppable
    //

    void onPrepare() override
    {
    }

    void onStart () override
    {
        JLOG(m_journal.info())
            << "Application starting. Version is " << BuildInfo::getVersionString();

        using namespace std::chrono_literals;
        if(startTimers_)
        {
            setSweepTimer();
            setEntropyTimer();
        }

        m_io_latency_sampler.start();

        m_resolver->start ();
    }

    // Called to indicate shutdown.
    void onStop () override
    {
        JLOG(m_journal.debug()) << "Application stopping";

        m_io_latency_sampler.cancel_async ();

        // VFALCO Enormous hack, we have to force the probe to cancel
        //        before we stop the io_service queue or else it never
        //        unblocks in its destructor. The fix is to make all
        //        io_objects gracefully handle exit so that we can
        //        naturally return from io_service::run() instead of
        //        forcing a call to io_service::stop()
        m_io_latency_sampler.cancel ();

        m_resolver->stop_async ();

        // NIKB This is a hack - we need to wait for the resolver to
        //      stop. before we stop the io_server_queue or weird
        //      things will happen.
        m_resolver->stop ();

        {
            boost::system::error_code ec;
            sweepTimer_.cancel (ec);
            if (ec)
            {
                JLOG (m_journal.error())
                    << "Application: sweepTimer cancel error: "
                    << ec.message();
            }

            ec.clear();
            entropyTimer_.cancel (ec);
            if (ec)
            {
                JLOG (m_journal.error())
                    << "Application: entropyTimer cancel error: "
                    << ec.message();
            }
        }
        // Make sure that any waitHandlers pending in our timers are done
        // before we declare ourselves stopped.
        using namespace std::chrono_literals;
        waitHandlerCounter_.join("Application", 1s, m_journal);

		//foreach schema
		for (auto iter = m_schemaManager->begin(); iter != m_schemaManager->end(); iter++)
		{
			auto schema = (*iter).second;
			schema->onStop();
		}
        stopped ();
    }

    //--------------------------------------------------------------------------
    //
    // PropertyStream
    //

    void onWrite (beast::PropertyStream::Map& stream) override
    {
    }

    //--------------------------------------------------------------------------

    void setSweepTimer ()
    {
        // Only start the timer if waitHandlerCounter_ is not yet joined.
        if (auto optionalCountedHandler = waitHandlerCounter_.wrap (
            [this] (boost::system::error_code const& e)
            {
                if ((e.value() == boost::system::errc::success) &&
                    (! m_jobQueue->isStopped()))
                {
                    m_jobQueue->addJob(
                        jtSWEEP, "sweep", [this] (Job&) { doSweep(); });
                }
                // Recover as best we can if an unexpected error occurs.
                if (e.value() != boost::system::errc::success &&
                    e.value() != boost::asio::error::operation_aborted)
                {
                    // Try again later and hope for the best.
                    JLOG (m_journal.error())
                       << "Sweep timer got error '" << e.message()
                       << "'.  Restarting timer.";
                    setSweepTimer();
                }
            }))
        {
            using namespace std::chrono;
            sweepTimer_.expires_from_now(
                seconds{config_->getSize(siSweepInterval)});
            sweepTimer_.async_wait (std::move (*optionalCountedHandler));
        }
    }

    void setEntropyTimer ()
    {
        // Only start the timer if waitHandlerCounter_ is not yet joined.
        if (auto optionalCountedHandler = waitHandlerCounter_.wrap (
            [this] (boost::system::error_code const& e)
            {
                if (e.value() == boost::system::errc::success)
                {
                    crypto_prng().mix_entropy();
                    setEntropyTimer();
                }
                // Recover as best we can if an unexpected error occurs.
                if (e.value() != boost::system::errc::success &&
                    e.value() != boost::asio::error::operation_aborted)
                {
                    // Try again later and hope for the best.
                    JLOG (m_journal.error())
                       << "Entropy timer got error '" << e.message()
                       << "'.  Restarting timer.";
                    setEntropyTimer();
                }
            }))
        {
            using namespace std::chrono_literals;
            entropyTimer_.expires_from_now (5min);
            entropyTimer_.async_wait (std::move (*optionalCountedHandler));
        }
    }

    void doSweep ()
    {
		//by ljl: foreach schema do sweep
		for (auto iter = m_schemaManager->begin(); iter != m_schemaManager->end(); iter++)
		{
			auto schema = (*iter).second;
			schema->doSweep();
		}
        // Set timer to do another sweep later.
        setSweepTimer();
    }

};

//------------------------------------------------------------------------------

// VFALCO TODO Break this function up into many small initialization segments.
//             Or better yet refactor these initializations into RAII classes
//             which are members of the Application object.
//
bool ApplicationImp::setup()
{   
	auto schema_main = m_schemaManager->createSchemaMain(*config_);

	schema_main->initBeforeSetup();

	nodeIdentity_ = loadNodeIdentity(*this);

    // VFALCO NOTE: 0 means use heuristics to determine the thread count.
    m_jobQueue->setThreadCount (config_->WORKERS, config_->standalone());

    // We want to intercept and wait for CTRL-C to terminate the process
    m_signals.add (SIGINT);

    m_signals.async_wait(std::bind(&ApplicationImp::signalled, this,
        std::placeholders::_1, std::placeholders::_2));

    auto debug_log = config_->getDebugLogFile ();

    if (!debug_log.empty ())
    {
        // Let debug messages go to the file but only WARNING or higher to
        // regular output (unless verbose)

        if (!logs_->open(debug_log))
            std::cerr << "Can't open log file " << debug_log << '\n';

        using namespace beast::severities;
        if (logs_->threshold() > kDebug)
            logs_->threshold (kDebug);
    }
    JLOG(m_journal.info()) << "process starting: "
        << BuildInfo::getFullVersionString();

    // Optionally turn off logging to console.
    logs_->silent (config_->silent());

    if (!config_->standalone())
        timeKeeper_->run(config_->SNTP_SERVERS);

	{
		try
		{
			auto setup = setup_ServerHandler(
				*config_, beast::logstream{ m_journal.error() });
			setup.makeContexts();
			serverHandler_->setup(setup, m_journal);
		}
		catch (std::exception const& e)
		{
			if (auto stream = m_journal.fatal())
			{
				stream << "Unable to setup server handler";
				if (std::strlen(e.what()) > 0)
					stream << ": " << e.what();
			}
			return false;
		}
	}

	schema_main->setup();

    return true;
}

void
ApplicationImp::doStart(bool withTimers)
{
    startTimers_ = withTimers;
    prepare ();
    start ();
}

void
ApplicationImp::run()
{
    if (!config_->standalone())
    {
        // VFALCO NOTE This seems unnecessary. If we properly refactor the load
        //             manager then the deadlock detector can just always be "armed"
        //
        getLoadManager ().activateDeadlockDetector ();
    }

    {
        std::unique_lock<std::mutex> lk{mut_};
        cv_.wait(lk, [this]{return isTimeToStop;});
    }

    // Stop the server. When this returns, all
    // Stoppable objects should be stopped.
    JLOG(m_journal.info()) << "Received shutdown request";
    stop (m_journal);
    JLOG(m_journal.info()) << "Done.";
    StopSustain();
}

void
ApplicationImp::signalStop()
{
    // Unblock the main thread (which is sitting in run()).
    //
    std::lock_guard<std::mutex> lk{mut_};
    isTimeToStop = true;
    cv_.notify_all();
}

bool
ApplicationImp::isShutdown()
{
    // from Stoppable mixin
    return isStopped();
}

bool ApplicationImp::checkSigs() const
{
    return checkSigs_;
}

void ApplicationImp::checkSigs(bool check)
{
    checkSigs_ = check;
}

int ApplicationImp::fdlimit() const
{
    // Standard handles, config file, misc I/O etc:
    int needed = 128;

	for (auto iter = m_schemaManager->begin(); iter != m_schemaManager->end(); iter++)
	{
		auto schema = (*iter).second;
		// 1.5 times the configured peer limit for peer connections:
		needed += static_cast<int>(0.5 + (1.5 * schema->overlay().limit()));
		// the number of fds needed by the backend (internally
		// doubled if online delete is enabled).
		needed += std::max(5, schema->getSHAMapStore().fdlimit());
	}

    // One fd per incoming connection a port can accept, or
    // if no limit is set, assume it'll handle 256 clients.
    for(auto const& p : serverHandler_->setup().ports)
        needed += std::max (256, p.limit);

    // The minimum number of file descriptors we need is 1024:
    return std::max(1024, needed);
}

//------------------------------------------------------------------------------



bool ApplicationImp::serverOkay (std::string& reason)
{
    if (! config(beast::zero).ELB_SUPPORT)
        return true;

    if (isShutdown ())
    {
        reason = "Server is shutting down";
        return false;
    }

    if (getOPs (beast::zero).isNeedNetworkLedger ())
    {
        reason = "Not synchronized with network yet";
        return false;
    }

    if (getOPs (beast::zero).getOperatingMode () < NetworkOPs::omSYNCING)
    {
        reason = "Not synchronized with network";
        return false;
    }

    if (!getLedgerMaster(beast::zero).isCaughtUp(reason))
        return false;

    if (getFeeTrack (beast::zero).isLoadedLocal ())
    {
        reason = "Too much load";
        return false;
    }

    if (getOPs (beast::zero).isAmendmentBlocked ())
    {
        reason = "Server version too old";
        return false;
    }

    return true;
}

beast::Journal
ApplicationImp::journal (std::string const& name)
{
    return logs_->journal (name);
}

//VFALCO TODO clean this up since it is just a file holding a single member function definition



//------------------------------------------------------------------------------

Application::Application ()
    : beast::PropertyStream::Source ("app")
{
}

//------------------------------------------------------------------------------

std::unique_ptr<Application>
make_Application (
    std::unique_ptr<Config> config,
    std::unique_ptr<Logs> logs,
    std::unique_ptr<TimeKeeper> timeKeeper)
{
    return std::make_unique<ApplicationImp> (
        std::move(config), std::move(logs),
            std::move(timeKeeper));
}

}
