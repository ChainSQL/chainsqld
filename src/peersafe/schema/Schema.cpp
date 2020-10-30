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
#include <peersafe/schema/Schema.h>
#include <peersafe/schema/PeerManager.h>
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
	// 204/256 about 80%
	static int const MAJORITY_FRACTION(204);

	namespace detail {

		class AppFamily : public Family
		{
		private:
			Schema& app_;
			TreeNodeCache treecache_;
			FullBelowCache fullbelow_;
			NodeStore::Database& db_;
			bool const shardBacked_;
			beast::Journal j_;

			// missing node handler
			LedgerIndex maxSeq = 0;
			std::mutex maxSeqLock;

			void acquire(
				uint256 const& hash,
				std::uint32_t seq)
			{
				if (hash.isNonZero())
				{
					auto j = app_.journal("Ledger");

					JLOG(j.error()) <<
						"Missing node in " << to_string(hash);

					app_.getInboundLedgers().acquire(
						hash, seq, shardBacked_ ?
						InboundLedger::Reason::SHARD :
						InboundLedger::Reason::GENERIC);
				}
			}

		public:
			AppFamily(AppFamily const&) = delete;
			AppFamily& operator= (AppFamily const&) = delete;

			AppFamily(Schema& app, NodeStore::Database& db,
				CollectorManager& collectorManager)
				: app_(app)
				, treecache_("TreeNodeCache", 65536, std::chrono::minutes{ 1 },
					stopwatch(), app.journal("TaggedCache"))
				, fullbelow_("full_below", stopwatch(),
					collectorManager.collector(),
					fullBelowTargetSize, fullBelowExpiration)
				, db_(db)
				, shardBacked_(
					dynamic_cast<NodeStore::DatabaseShard*>(&db) != nullptr)
				, j_(app.journal("SHAMap"))
			{
			}

			beast::Journal const&
				journal() override
			{
				return j_;
			}

			FullBelowCache&
				fullbelow() override
			{
				return fullbelow_;
			}

			FullBelowCache const&
				fullbelow() const override
			{
				return fullbelow_;
			}

			TreeNodeCache&
				treecache() override
			{
				return treecache_;
			}

			TreeNodeCache const&
				treecache() const override
			{
				return treecache_;
			}

			NodeStore::Database&
				db() override
			{
				return db_;
			}

			NodeStore::Database const&
				db() const override
			{
				return db_;
			}

			bool
				isShardBacked() const override
			{
				return shardBacked_;
			}

			void
				missing_node(std::uint32_t seq) override
			{
				auto j = app_.journal("Ledger");

				JLOG(j.error()) <<
					"Missing node in " << seq;

				// prevent recursive invocation
				std::unique_lock <std::mutex> lock(maxSeqLock);

				if (maxSeq == 0)
				{
					maxSeq = seq;

					do
					{
						// Try to acquire the most recent missing ledger
						seq = maxSeq;

						lock.unlock();

						// This can invoke the missing node handler
						acquire(
							app_.getLedgerMaster().getHashBySeq(seq),
							seq);

						lock.lock();
					} while (maxSeq != seq);
				}
				else if (maxSeq < seq)
				{
					// We found a more recent ledger with a
					// missing node
					maxSeq = seq;
				}
			}

			void
				missing_node(uint256 const& hash, std::uint32_t seq) override
			{
				acquire(hash, seq);
			}

			void
				reset() override
			{
				{
					std::lock_guard<std::mutex> lock(maxSeqLock);
					maxSeq = 0;
				}
				fullbelow_.reset();
				treecache_.reset();
			}
		};

	} // detail
	class SchemaImp 
		:public Schema
		,public RootStoppable
	{
	public:

		bool setup() override;    
		bool initBeforeSetup() override;
		void addTxnSeqField();
		void addValidationSeqFields();
		bool updateTables();
		bool nodeToShards();
		bool validateShards();
		void startGenesisLedger();
		bool setSynTable();

		bool checkCertificate();

		std::shared_ptr<Ledger>
			getLastFullLedger();

		std::shared_ptr<Ledger>
			loadLedgerFromFile(
				std::string const& ledgerID);

		bool loadOldLedger(
			std::string const& ledgerID,
			bool replay,
			bool isFilename);

		void
			startGenesisLedger(
				std::shared_ptr<Ledger const> curLedger);
	private:
		Application& app_;
		beast::Journal m_journal;
		SchemaParams schema_params_;

		std::shared_ptr<Config>		config_;
		TransactionMaster			m_txMaster;


		std::unique_ptr <SHAMapStore> m_shaMapStore;
		PendingSaves				pendingSaves_;
		AccountIDCache				accountIDCache_;
		boost::optional<OpenLedger> openLedger_;

		// These are not Stoppable-derived
		NodeCache m_tempNodeCache;
		CachedSLEs cachedSLEs_;


		// These are Stoppable-related		
		std::unique_ptr <NodeStore::Database> m_nodeStore;
		std::unique_ptr <NodeStore::DatabaseShard> shardStore_;
		detail::AppFamily family_;
		std::unique_ptr <detail::AppFamily> sFamily_;
		// VFALCO TODO Make OrderBookDB abstract
		OrderBookDB m_orderBookDB;
		std::unique_ptr <PathRequests> m_pathRequests;
		std::unique_ptr <LedgerMaster> m_ledgerMaster;
		std::unique_ptr <InboundLedgers> m_inboundLedgers;
		std::unique_ptr <InboundTransactions> m_inboundTransactions;
		TaggedCache <uint256, AcceptedLedger> m_acceptedLedgerCache;
		std::unique_ptr <NetworkOPs> m_networkOPs;
		std::unique_ptr <Cluster> cluster_;
		std::unique_ptr <ManifestCache> validatorManifests_;
		std::unique_ptr <ManifestCache> publisherManifests_;
		std::unique_ptr <ValidatorList> validators_;
		std::unique_ptr <ValidatorSite> validatorSites_;
		std::unique_ptr <CertList>          certList_;
		std::unique_ptr <CACertSite>    caCertSites_;
		std::unique_ptr <AmendmentTable> m_amendmentTable;
		std::unique_ptr <LoadFeeTrack> mFeeTrack;
		std::unique_ptr <HashRouter> mHashRouter;
		RCLValidations mValidations;
		std::unique_ptr <TxQ> txQ_;
		std::unique_ptr <TxStoreDBConn> m_pTxStoreDBConn;
		std::unique_ptr <TxStore> m_pTxStore;
		std::unique_ptr <TableStatusDB> m_pTableStatusDB;
		std::unique_ptr <TableSync> m_pTableSync;
		std::unique_ptr <TableStorage> m_pTableStorage;
		std::unique_ptr <TableAssistant> m_pTableAssistant;
		std::unique_ptr <ContractHelper> m_pContractHelper;
		std::unique_ptr <TableTxAccumulator> m_pTableTxAccumulator;
		std::unique_ptr <TxPool> m_pTxPool;
		std::unique_ptr <StateManager> m_pStateManager;
		ClosureCounter<void, boost::system::error_code const&> waitHandlerCounter_;

		std::unique_ptr <DatabaseCon> mTxnDB;
		std::unique_ptr <DatabaseCon> mLedgerDB;
		std::unique_ptr <DatabaseCon> mWalletDB;
		std::unique_ptr <PeerManager> m_peerManager;

	public:
	SchemaImp(SchemaParams const& params, std::shared_ptr<Config> config, Application & app, beast::Journal j)
		: RootStoppable("Schema")
		, schema_params_(params)
		, app_(app)
		, m_journal(j)
		, config_(config)

		, m_txMaster(*this)

		, m_shaMapStore(make_SHAMapStore(*this, setup_SHAMapStore(*config_),
			dynamic_cast<Stoppable&>(app_), app_.nodeStoreScheduler(), SchemaImp::journal("SHAMapStore"),
			SchemaImp::journal("NodeObject"), m_txMaster, *config_))

		, accountIDCache_(128000)

		, m_tempNodeCache("NodeCache", 16384, std::chrono::seconds{ 90 },
			stopwatch(), SchemaImp::journal("TaggedCache"))


		, cachedSLEs_(std::chrono::minutes(1), stopwatch())


		//
		// Anything which calls addJob must be a descendant of the JobQueue
		//
		, m_nodeStore(
			m_shaMapStore->makeDatabase("NodeStore.main", 4, *this))

		, shardStore_(
			m_shaMapStore->makeDatabaseShard("ShardStore", 4, *this))

		, family_(*this, *m_nodeStore, app.getCollectorManager())

		, m_orderBookDB(*this, *this)

		, m_pathRequests(std::make_unique<PathRequests>(
			*this, SchemaImp::journal("PathRequest"), app.getCollectorManager().collector()))

		, m_ledgerMaster(std::make_unique<LedgerMaster>(*this, stopwatch(),
			app_.getJobQueue(), app.getCollectorManager().collector(),
			SchemaImp::journal("LedgerMaster")))


		// VFALCO NOTE must come before NetworkOPs to prevent a crash due
		//             to dependencies in the destructor.
		//
		, m_inboundLedgers(make_InboundLedgers(*this, stopwatch(),
			*this, app.getCollectorManager().collector()))

		, m_inboundTransactions(make_InboundTransactions
			(*this, stopwatch()
				, *this
				, app.getCollectorManager().collector()
				, [this](std::shared_ptr <SHAMap> const& set,
					bool fromAcquire)
				{
					gotTXSet(set, fromAcquire);
				}))

		, m_acceptedLedgerCache("AcceptedLedger", /*4*/16, std::chrono::minutes{ 10 }, stopwatch(),
			SchemaImp::journal("TaggedCache"))

		, m_peerManager(make_PeerManager(*this))
					
		, m_networkOPs(make_NetworkOPs(*this, stopwatch(),
			config_->standalone(), config_->NETWORK_QUORUM, config_->START_VALID,
			app_.getJobQueue(), *m_ledgerMaster, app_.getJobQueue(), app_.getValidatorKeys(),
			dynamic_cast<BasicApp&>(app_).get_io_service(), SchemaImp::journal("NetworkOPs")))

		, cluster_(std::make_unique<Cluster>(
			SchemaImp::journal("Overlay")))

		, validatorManifests_(std::make_unique<ManifestCache>(
			SchemaImp::journal("ManifestCache")))

		, publisherManifests_(std::make_unique<ManifestCache>(
			SchemaImp::journal("ManifestCache")))

		, validators_(std::make_unique<ValidatorList>(
			*validatorManifests_, *publisherManifests_, app_.timeKeeper(),
			SchemaImp::journal("ValidatorList"), config_->VALIDATION_QUORUM))

		, validatorSites_(std::make_unique<ValidatorSite>(
			*validatorManifests_, dynamic_cast<BasicApp&>(app_).get_io_service(), *validators_, SchemaImp::journal("ValidatorSite")))

		, caCertSites_(std::make_unique<CACertSite>(
			*validatorManifests_, *publisherManifests_, app_.timeKeeper(),
			dynamic_cast<BasicApp&>(app_).get_io_service(), config_->ROOT_CERTIFICATES, SchemaImp::journal("CACertSite")))

		, certList_(std::make_unique<CertList>(config_->ROOT_CERTIFICATES, SchemaImp::journal("CertList")))

		, mFeeTrack(std::make_unique<LoadFeeTrack>(SchemaImp::journal("LoadManager")))

		, mHashRouter(std::make_unique<HashRouter>(
			stopwatch(), HashRouter::getDefaultHoldTime(),
			HashRouter::getDefaultRecoverLimit()))

		, mValidations(ValidationParms(), stopwatch(), *this, SchemaImp::journal("Validations"))

		, txQ_(make_TxQ(setup_TxQ(*config_), SchemaImp::journal("TxQ")))

		, m_pTxStoreDBConn(std::make_unique<TxStoreDBConn>(*config_))

		, m_pTxStore(std::make_unique<TxStore>(m_pTxStoreDBConn->GetDBConn(), *config_, SchemaImp::journal("TxStore")))

		, m_pTableSync(std::make_unique<TableSync>(*this, *config_, SchemaImp::journal("TableSync")))

		, m_pTableStorage(std::make_unique<TableStorage>(*this, *config_, SchemaImp::journal("TableStorage")))

		, m_pTableAssistant(std::make_unique<TableAssistant>(*this, *config_, SchemaImp::journal("TableAssistant")))

		, m_pContractHelper(std::make_unique<ContractHelper>(*this))

		, m_pTableTxAccumulator(std::make_unique<TableTxAccumulator>(*this))

		, m_pTxPool(std::make_unique<TxPool>(*this, SchemaImp::journal("TxPool")))

		, m_pStateManager(std::make_unique<StateManager>(*this, SchemaImp::journal("StateManager")))
	{
		if (shardStore_)
			sFamily_ = std::make_unique<detail::AppFamily>(
				*this, *shardStore_, app_.getCollectorManager());
	}

	Application& app() override
	{
		return app_;
	}

	Logs& logs() override
	{
		return app_.logs();
	}

	beast::Journal journal(std::string const& name)
	{
		std::string prefix = strHex(schema_params_.schema_id.begin(), schema_params_.schema_id.begin()+2);
		return app_.logs().journal("[" + prefix +"]" + name);
	}

	CollectorManager& getCollectorManager() override
	{
		return app_.getCollectorManager();
	}

	TimeKeeper&
		timeKeeper() override
	{
		return app_.timeKeeper();
	}

	JobQueue& getJobQueue() override
	{
		return app_.getJobQueue();
	}

	LoadManager& getLoadManager() override
	{
		return app_.getLoadManager();
	}

	Resource::Manager& getResourceManager() override
	{
		return app_.getResourceManager();
	}

	perf::PerfLog& getPerfLog() override
	{
		return app_.getPerfLog();
	}

	std::pair<PublicKey, SecretKey> const&
		nodeIdentity() override
	{
		return app_.nodeIdentity();
	}

	virtual
		PublicKey const &
		getValidationPublicKey() const override
	{
		return app_.getValidationPublicKey();
	}

	ValidatorKeys const& getValidatorKeys()const override
	{
		return app_.getValidatorKeys();
	}

	Config&
		config() override
	{
		return *config_;
	}
	Family& family() override
	{
		return family_;
	}

	Family* shardFamily() override
	{
		return sFamily_.get();
	}

	TxStoreDBConn& getTxStoreDBConn() override
	{
		return *m_pTxStoreDBConn;
	}

	TxStore& getTxStore() override
	{
		return *m_pTxStore;
	}

	TableStatusDB& getTableStatusDB() override
	{
		return *m_pTableStatusDB;
	}

	TableSync& getTableSync() override
	{
		return *m_pTableSync;
	}

	TableStorage& getTableStorage() override
	{
		return *m_pTableStorage;
	}

	TableAssistant& getTableAssistant() override
	{
		return *m_pTableAssistant;
	}

	ContractHelper& getContractHelper() override
	{
		return *m_pContractHelper;
	}

	TableTxAccumulator& getTableTxAccumulator() override
	{
		return *m_pTableTxAccumulator;
	}

	TxPool& getTxPool() override
	{
		return *m_pTxPool;
	}

	StateManager& getStateManager() override
	{
		return *m_pStateManager;
	}

	NetworkOPs& getOPs() override
	{
		return *m_networkOPs;
	}
	LedgerMaster& getLedgerMaster() override
	{
		return *m_ledgerMaster;
	}

	InboundLedgers& getInboundLedgers() override
	{
		return *m_inboundLedgers;
	}

	InboundTransactions& getInboundTransactions() override
	{
		return *m_inboundTransactions;
	}

	TaggedCache <uint256, AcceptedLedger>& getAcceptedLedgerCache() override
	{
		return m_acceptedLedgerCache;
	}

	void gotTXSet(std::shared_ptr<SHAMap> const& set, bool fromAcquire)
	{
		if (set)
			m_networkOPs->mapComplete(set, fromAcquire);
	}
	TransactionMaster& getMasterTransaction() override
	{
		return m_txMaster;
	}
	NodeCache& getTempNodeCache() override
	{
		return m_tempNodeCache;
	}

	NodeStore::Database& getNodeStore() override
	{
		return *m_nodeStore;
	}

	NodeStore::DatabaseShard* getShardStore() override
	{
		return shardStore_.get();
	}
	OrderBookDB& getOrderBookDB() override
	{
		return m_orderBookDB;
	}

	PathRequests& getPathRequests() override
	{
		return *m_pathRequests;
	}

	CachedSLEs&
		cachedSLEs() override
	{
		return cachedSLEs_;
	}

	AmendmentTable& getAmendmentTable() override
	{
		return *m_amendmentTable;
	}

	LoadFeeTrack& getFeeTrack() override
	{
		return *mFeeTrack;
	}

	HashRouter& getHashRouter() override
	{
		return *mHashRouter;
	}

	RCLValidations& getValidations() override
	{
		return mValidations;
	}

	ValidatorList& validators() override
	{
		return *validators_;
	}

	ValidatorSite& validatorSites() override
	{
		return *validatorSites_;
	}

	CertList& certList() override
	{
		return *certList_;
	}

	ManifestCache& validatorManifests() override
	{
		return *validatorManifests_;
	}

	ManifestCache& publisherManifests() override
	{
		return *publisherManifests_;
	}

	Cluster& cluster() override
	{
		return *cluster_;
	}

	SHAMapStore& getSHAMapStore() override
	{
		return *m_shaMapStore;
	}

	PendingSaves& pendingSaves() override
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

	PeerManager& peerManager() override
	{
		return *m_peerManager;
	}

	TxQ& getTxQ() override
	{
		assert(txQ_.get() != nullptr);
		return *txQ_;
	}

	DatabaseCon& getTxnDB() override
	{
		assert(mTxnDB.get() != nullptr);
		return *mTxnDB;
	}
	DatabaseCon& getLedgerDB() override
	{
		assert(mLedgerDB.get() != nullptr);
		return *mLedgerDB;
	}
	DatabaseCon& getWalletDB() override
	{
		assert(mWalletDB.get() != nullptr);
		return *mWalletDB;
	}

	bool initSqliteDbs()
	{
		assert(mTxnDB.get() == nullptr);
		assert(mLedgerDB.get() == nullptr);
		assert(mWalletDB.get() == nullptr);

		DatabaseCon::Setup setup = setup_DatabaseCon(*config_);
		mTxnDB = std::make_unique <DatabaseCon>(setup, "transaction.db",
			TxnDBInit, TxnDBCount);
		mLedgerDB = std::make_unique <DatabaseCon>(setup, "ledger.db",
			LedgerDBInit, LedgerDBCount);
		mWalletDB = std::make_unique <DatabaseCon>(setup, "wallet.db",
			WalletDBInit, WalletDBCount);

		return
			mTxnDB.get() != nullptr &&
			mLedgerDB.get() != nullptr &&
			mWalletDB.get() != nullptr;
	}

	void doSweep() override
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

			DatabaseCon::Setup dbSetup = setup_DatabaseCon(*config_);
			boost::filesystem::path dbPath = dbSetup.dataDir / TxnDBName;
			boost::system::error_code ec;
			boost::optional<std::uint64_t> dbSize = boost::filesystem::file_size(dbPath, ec);
			if (ec)
			{
				JLOG(m_journal.error())
					<< "Error checking transaction db file size: "
					<< ec.message();
				dbSize.reset();
			}

			auto db = mTxnDB->checkoutDb();
			static auto const pageSize = [&] {
				std::uint32_t ps;
				*db << "PRAGMA page_size;", soci::into(ps);
				return ps;
			}();
			static auto const maxPages = [&] {
				std::uint32_t mp;
				*db << "PRAGMA max_page_count;", soci::into(mp);
				return mp;
			}();
			std::uint32_t pageCount;
			*db << "PRAGMA page_count;", soci::into(pageCount);
			std::uint32_t freePages = maxPages - pageCount;
			std::uint64_t freeSpace =
				safe_cast<std::uint64_t>(freePages) * pageSize;
			//JLOG(m_journal.info())
			//	<< "Transaction DB pathname: " << dbPath.string()
			//	<< "; file size: " << dbSize.value_or(-1) << " bytes"
			//	<< "; SQLite page size: " << pageSize << " bytes"
			//	<< "; Free pages: " << freePages
			//	<< "; Free space: " << freeSpace << " bytes; "
			//	<< "Note that this does not take into account available disk "
			//	"space.";
			if (freeSpace < megabytes(512))
			{
				JLOG(m_journal.fatal())
					<< "Free SQLite space for transaction db is less than "
					"512MB. To fix this, rippled must be executed with the "
					"vacuum <sqlitetmpdir> parameter before restarting. "
					"Note that this activity can take multiple days, "
					"depending on database size.";
				app_.signalStop();
			}
		}

		// VFALCO NOTE Does the order of calls matter?
		// VFALCO TODO fix the dependency inversion using an observer,
		//         have listeners register for "onSweep ()" notification.

		family().fullbelow().sweep();
		if (sFamily_)
			sFamily_->fullbelow().sweep();
		getMasterTransaction().sweep();
		getNodeStore().sweep();
		if (shardStore_)
			shardStore_->sweep();
		getLedgerMaster().sweep();
		getTempNodeCache().sweep();
		getValidations().expire();
		getInboundLedgers().sweep();
		getTableSync().sweep();
		m_acceptedLedgerCache.sweep();
		family().treecache().sweep();
		if (sFamily_)
			sFamily_->treecache().sweep();
		cachedSLEs_.expire();
	}

	void doStart() override
	{
		prepare();
		start();
	}

	void doStop() override
	{
		JLOG(m_journal.debug()) << "Flushing validations";
		mValidations.flush();
		JLOG(m_journal.debug()) << "Validations flushed";

		validatorSites_->stop();

		caCertSites_->stop();

		// TODO Store manifests in manifests.sqlite instead of wallet.db
		validatorManifests_->save(getWalletDB(), "ValidatorManifests",
			[this](PublicKey const& pubKey)
		{
			return validators().listed(pubKey);
		});

		publisherManifests_->save(getWalletDB(), "PublisherManifests",
			[this](PublicKey const& pubKey)
		{
			return validators().trustedPublisher(pubKey);
		});

		stop(m_journal);
	}

	LedgerIndex getMaxDisallowedLedger() override
	{
		return maxDisallowedLedger_;
	}

	SchemaParams	getSchemaParams() override
	{
		return schema_params_;
	}

	SchemaID schemaId()
	{
		return schema_params_.schemaId();
	}
private:
	// For a newly-started validator, this is the greatest persisted ledger
	// and new validations must be greater than this.
	std::atomic<LedgerIndex> maxDisallowedLedger_{ 0 };

	void setMaxDisallowedLedger();

	};

	bool SchemaImp::initBeforeSetup()
	{
		assert(mTxnDB == nullptr);
		if (!initSqliteDbs())
		{
			JLOG(m_journal.fatal()) << "Cannot create database connections!";
			return false;
		}
		return true;
	}

	bool SchemaImp::setup()
	{
		if (!setSynTable())  return false;

		if (!checkCertificate())  return false;

		setMaxDisallowedLedger();

		getLedgerDB().getSession()
			<< boost::str(boost::format("PRAGMA cache_size=-%d;") %
			(config_->getSize(siLgrDBCache) * kilobytes(1)));

		getTxnDB().getSession()
			<< boost::str(boost::format("PRAGMA cache_size=-%d;") %
			(config_->getSize(siTxnDBCache) * kilobytes(1)));

		mTxnDB->setupCheckpointing(&app_.getJobQueue(), app_.logs());
		mLedgerDB->setupCheckpointing(&app_.getJobQueue(), app_.logs());

		if (!updateTables())
			return false;

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
				weeks{ 2 },//std::chrono::minutes{10},
				MAJORITY_FRACTION,
				supportedAmendments,
				enabledAmendments,
				config_->section(SECTION_VETO_AMENDMENTS),
				SchemaImp::journal("Amendments"));
		}

		Pathfinder::initPathTable();

		auto const startUp = config_->START_UP;
		if (startUp == Config::FRESH)
		{
			JLOG(m_journal.info()) << "Starting new Ledger";

			startGenesisLedger();
		}
		else if (startUp == Config::LOAD ||
			startUp == Config::LOAD_FILE ||
			startUp == Config::REPLAY)
		{
			JLOG(m_journal.info()) <<
				"Loading specified Ledger";

			if (!loadOldLedger(config_->START_LEDGER,
				startUp == Config::REPLAY,
				startUp == Config::LOAD_FILE))
			{
				JLOG(m_journal.error()) <<
					"The specified ledger could not be loaded.";
				return false;
			}
		}
		else if (startUp == Config::NETWORK)
		{
			// This should probably become the default once we have a stable network.
			if (!config_->standalone())
				m_networkOPs->setNeedNetworkLedger();

			startGenesisLedger();
		}
		else if (startUp == Config::NEWCHAIN_WITHSTATE) {

			auto validLedger = app_.getLedgerMaster().getLedgerByHash(schema_params_.anchor_ledger_hash);
			JLOG(m_journal.info()) << "NEWCHAIN_WITHSTATE from ledger=" << to_string(schema_params_.anchor_ledger_hash);
			startGenesisLedger(validLedger);
		}
		//else if (startUp == Config::NEWCHAIN_LOAD) {
		//	if (!loadOldLedger(config_->START_LEDGER,
		//		startUp == Config::REPLAY,
		//		startUp == Config::LOAD_FILE))
		//	{
		//		JLOG(m_journal.fatal()) << "Invalid NEWCHAIN_LOAD.";
		//		return false;
		//	}
		//}
		else
		{
			if (getLastFullLedger() != nullptr)
			{
				if (!loadOldLedger(config_->START_LEDGER,
					startUp == Config::REPLAY,
					startUp == Config::LOAD_FILE))
				{
					JLOG(m_journal.fatal()) << "Load old ledger failed for schema:" << to_string(schemaId());
					return false;
				}
			}
			else
			{
				startGenesisLedger();
			}
			
		}

		m_orderBookDB.setup(getLedgerMaster().getCurrentLedger());

		if (!cluster_->load(config().section(SECTION_CLUSTER_NODES)))
		{
			JLOG(m_journal.fatal()) << "Invalid entry in cluster configuration.";
			return false;
		}

		{
			if (app_.getValidatorKeys().configInvalid())
				return false;

			if (!validatorManifests_->load(
				getWalletDB(), "ValidatorManifests", app_.getValidatorKeys().manifest,
				config().section(SECTION_VALIDATOR_KEY_REVOCATION).values()))
			{
				JLOG(m_journal.fatal()) << "Invalid configured validator manifest.";
				return false;
			}

			publisherManifests_->load(
				getWalletDB(), "PublisherManifests");

			// Setup trusted validators
			if (!validators_->load(
				config_->ONLY_VALIDATE_FOR_SCHEMA ? PublicKey(): app_.getValidationPublicKey(),
				config().section(SECTION_VALIDATORS).values(),
				config().section(SECTION_VALIDATOR_LIST_KEYS).values()))
			{
				JLOG(m_journal.fatal()) <<
					"Invalid entry in validator configuration.";
				return false;
			}
		}

		if (!validatorSites_->load(
			config().section(SECTION_VALIDATOR_LIST_SITES).values()))
		{
			JLOG(m_journal.fatal()) <<
				"Invalid entry in [" << SECTION_VALIDATOR_LIST_SITES << "]";
			return false;
		}
		else {
			validatorSites_->setWaitinBeginConsensus();
		}

		if (!caCertSites_->load(
			config().section(SECTION_CACERTS_LIST_KEYS).values(),
			config().section(SECTION_CACERTS_LIST_SITES).values()))
		{
			JLOG(m_journal.fatal()) <<
				"Invalid entry in [" << SECTION_CACERTS_LIST_SITES << "]";
			return false;
		}

		using namespace std::chrono;
		m_nodeStore->tune(config_->getSize(siNodeCacheSize),
			seconds{ config_->getSize(siNodeCacheAge) });
		m_ledgerMaster->tune(config_->getSize(siLedgerSize),
			seconds{ config_->getSize(siLedgerAge) });
		family().treecache().setTargetSize(config_->getSize(siTreeCacheSize));
		family().treecache().setTargetAge(
			seconds{ config_->getSize(siTreeCacheAge) });
		if (shardStore_)
		{
			shardStore_->tune(config_->getSize(siNodeCacheSize),
				seconds{ config_->getSize(siNodeCacheAge) });
			sFamily_->treecache().setTargetSize(config_->getSize(siTreeCacheSize));
			sFamily_->treecache().setTargetAge(
				seconds{ config_->getSize(siTreeCacheAge) });
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

		validatorSites_->start();

		caCertSites_->start();

		// start first consensus round
		if (config().section(SECTION_VALIDATOR_LIST_SITES).values().size() == 0)
		{
			if (!m_networkOPs->beginConsensus(m_ledgerMaster->getClosedLedger()->info().hash))
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
			JLOG(m_journal.warn()) <<
				"*** The server is configured to allow the 'sign' and 'sign_for'";
			JLOG(m_journal.warn()) <<
				"*** commands. These commands have security implications and have";
			JLOG(m_journal.warn()) <<
				"*** been deprecated. They will be removed in a future release of";
			JLOG(m_journal.warn()) <<
				"*** rippled.";
			JLOG(m_journal.warn()) <<
				"*** If you do not use them to sign transactions please edit your";
			JLOG(m_journal.warn()) <<
				"*** configuration file and remove the [enable_signing] stanza.";
			JLOG(m_journal.warn()) <<
				"*** If you do use them to sign transactions please migrate to a";
			JLOG(m_journal.warn()) <<
				"*** standalone signing solution as soon as possible.";
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
				JLOG(m_journal.fatal()) <<
					"Couldn't parse entry in [" << SECTION_RPC_STARTUP <<
					"]: '" << cmd;
			}

			if (!config_->quiet())
			{
				JLOG(m_journal.fatal()) << "Startup RPC: " << jvCommand << std::endl;
			}

			Resource::Charge loadType = Resource::feeReferenceRPC;
			Resource::Consumer c;
			RPC::Context context{ app_.journal("RPCHandler"), jvCommand, *this,
				loadType, getOPs(), getLedgerMaster(), c, Role::ADMIN };

			Json::Value jvResult;
			RPC::doCommand(context, jvResult);

			if (!config_->quiet())
			{
				JLOG(m_journal.fatal()) << "Result: " << jvResult << std::endl;
			}
		}

		return true;
	}

	void
		SchemaImp::startGenesisLedger()
	{
		std::vector<uint256> initialAmendments =
			(config_->START_UP == Config::FRESH) ?
			m_amendmentTable->getDesired() :
			std::vector<uint256>{};

		std::shared_ptr<Ledger> const genesis =
			std::make_shared<Ledger>(
				create_genesis,
				*config_,
				initialAmendments,
				family());
		m_ledgerMaster->storeLedger(genesis);

		genesis->setImmutable(*config_);
		openLedger_.emplace(genesis, cachedSLEs_,
			SchemaImp::journal("OpenLedger"));
		m_ledgerMaster->switchLCL(genesis);
		//set valid ledger
		m_ledgerMaster->initGenesisLedger(genesis);
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
		SchemaImp::getLastFullLedger()
	{
		auto j = app_.journal("Ledger");

		//int count = 0;
		//while (count < 3)
		//{
		try
		{
			std::shared_ptr<Ledger> ledger;
			std::uint32_t seq;
			uint256 hash;

			//int index = 1 + count++;
			std::stringstream ss;
			//ss << "order by LedgerSeq desc limit " << index << ",1";
			//std::string loadSql = ss.str();
			std::string loadSql = "order by LedgerSeq desc limit 1";
			std::tie(ledger, seq, hash) = loadLedgerHelper(loadSql, *this);

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
				addJson(p, { *ledger, LedgerFill::full });
				stream << p;
			}

			return {};
		}
		catch (SHAMapMissingNode& sn)
		{
			JLOG(j.warn()) <<
				"Ledger with missing nodes in database: " << sn;
			//continue;
		}
		//}
		return{};
	}

	std::shared_ptr<Ledger>
		SchemaImp::loadLedgerFromFile(
			std::string const& name)
	{
		try
		{
			std::ifstream ledgerFile(name, std::ios::in);

			if (!ledgerFile)
			{
				JLOG(m_journal.fatal()) <<
					"Unable to open file '" << name << "'";
				return nullptr;
			}

			Json::Reader reader;
			Json::Value jLedger;

			if (!reader.parse(ledgerFile, jLedger))
			{
				JLOG(m_journal.fatal()) <<
					"Unable to parse ledger JSON";
				return nullptr;
			}

			std::reference_wrapper<Json::Value> ledger(jLedger);

			// accept a wrapped ledger
			if (ledger.get().isMember("result"))
				ledger = ledger.get()["result"];

			if (ledger.get().isMember("ledger"))
				ledger = ledger.get()["ledger"];

			std::uint32_t seq = 1;
			auto closeTime = app_.timeKeeper().closeTime();
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
					closeTime = tp{ d{ledger.get()["close_time"].asUInt()} };
				}
				if (ledger.get().isMember("close_time_resolution"))
				{
					using namespace std::chrono;
					closeTimeResolution = seconds{
						ledger.get()["close_time_resolution"].asUInt() };
				}
				if (ledger.get().isMember("close_time_estimated"))
				{
					closeTimeEstimated =
						ledger.get()["close_time_estimated"].asBool();
				}
				if (ledger.get().isMember("total_coins"))
				{
					totalDrops =
						beast::lexicalCastThrow<std::uint64_t>
						(ledger.get()["total_coins"].asString());
				}

				ledger = ledger.get()["accountState"];
			}

			if (!ledger.get().isArrayOrNull())
			{
				JLOG(m_journal.fatal())
					<< "State nodes must be an array";
				return nullptr;
			}

			auto loadLedger = std::make_shared<Ledger>(
				seq, closeTime, *config_, family());
			loadLedger->setTotalDrops(totalDrops);

			for (Json::UInt index = 0; index < ledger.get().size(); ++index)
			{
				Json::Value& entry = ledger.get()[index];

				if (!entry.isObjectOrNull())
				{
					JLOG(m_journal.fatal())
						<< "Invalid entry in ledger";
					return nullptr;
				}

				uint256 uIndex;

				if (!uIndex.SetHex(entry[jss::index].asString()))
				{
					JLOG(m_journal.fatal())
						<< "Invalid entry in ledger";
					return nullptr;
				}

				entry.removeMember(jss::index);

				STParsedJSONObject stp("sle", ledger.get()[index]);

				if (!stp.object || uIndex.isZero())
				{
					JLOG(m_journal.fatal())
						<< "Invalid entry in ledger";
					return nullptr;
				}

				// VFALCO TODO This is the only place that
				//             constructor is used, try to remove it
				STLedgerEntry sle(*stp.object, uIndex);

				if (!loadLedger->addSLE(sle))
				{
					JLOG(m_journal.fatal())
						<< "Couldn't add serialized ledger: "
						<< uIndex;
					return nullptr;
				}
			}

			loadLedger->stateMap().flushDirty(
				hotACCOUNT_NODE, loadLedger->info().seq);

			loadLedger->setAccepted(closeTime,
				closeTimeResolution, !closeTimeEstimated,
				*config_);

			return loadLedger;
		}
		catch (std::exception const& x)
		{
			JLOG(m_journal.fatal()) <<
				"Ledger contains invalid data: " << x.what();
			return nullptr;
		}
	}

	bool SchemaImp::loadOldLedger(
		std::string const& ledgerID, bool replay, bool isFileName)
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
						auto il = std::make_shared <InboundLedger>(
							*this, hash, 0, InboundLedger::Reason::GENERIC,
							stopwatch());
						if (il->checkLocal())
							loadLedger = il->getLedger();
					}
				}
			}
            else if (ledgerID.empty() || boost::beast::iequals(ledgerID, "latest"))
			{
				loadLedger = getLastFullLedger();
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
					JLOG(m_journal.info()) << "Loading parent ledger from node store";

					// Try to build the ledger from the back end
					auto il = std::make_shared <InboundLedger>(
						*this, replayLedger->info().parentHash,
						0, InboundLedger::Reason::GENERIC, stopwatch());

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

			JLOG(m_journal.info()) <<
				"Loading ledger " << loadLedger->info().hash <<
				" seq:" << loadLedger->info().seq;

			if (loadLedger->info().accountHash.isZero())
			{
				JLOG(m_journal.fatal()) << "Ledger is empty.";
				assert(false);
				return false;
			}

			if (!loadLedger->walkLedger(app_.journal("Ledger")))
			{
				JLOG(m_journal.fatal()) << "Ledger is missing nodes.";
				assert(false);
				return false;
			}

			if (!loadLedger->assertSane(app_.journal("Ledger")))
			{
				JLOG(m_journal.fatal()) << "Ledger is not sane.";
				assert(false);
				return false;
			}

			m_ledgerMaster->setLedgerRangePresent(
				loadLedger->info().seq,
				loadLedger->info().seq);

			m_ledgerMaster->switchLCL(loadLedger);
			loadLedger->setValidated();
			m_ledgerMaster->setFullLedger(loadLedger, true, false);
			openLedger_.emplace(loadLedger, cachedSLEs_,
				SchemaImp::journal("OpenLedger"));
			if (replay)
			{
				// inject transaction(s) from the replayLedger into our open ledger
				// and build replay structure
				auto replayData =
					std::make_unique<LedgerReplay>(loadLedger, replayLedger);

				for (auto const& it : replayData->orderedTxns())
				{
					std::shared_ptr<STTx const> const& tx = it.second;
					auto txID = tx->getTransactionID();

					auto s = std::make_shared <Serializer>();
					tx->add(*s);

					forceValidity(getHashRouter(),
						txID, Validity::SigGoodOnly);

					openLedger_->modify(
						[&txID, &s](OpenView& view, beast::Journal j)
					{
						view.rawTxInsert(txID, std::move(s), nullptr);
						return true;
					});
				}

				m_ledgerMaster->takeReplay(std::move(replayData));
			}
		}
		catch (SHAMapMissingNode&)
		{
			JLOG(m_journal.fatal()) <<
				"Data is missing for selected ledger";
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

	void SchemaImp::startGenesisLedger(std::shared_ptr<Ledger const> curLedger)
	{
		auto genesis = std::make_shared<Ledger>(*curLedger, family_);
		genesis->setImmutable(*config_);
	
		openLedger_.emplace(genesis, cachedSLEs_,
			SchemaImp::journal("OpenLedger"));
		m_ledgerMaster->switchLCL(genesis);

		//set valid ledger
		m_ledgerMaster->initGenesisLedger(genesis);
	}

	static
		std::vector<std::string>
		getSchema(DatabaseCon& dbc, std::string const& dbName)
	{
		std::vector<std::string> schema;
		schema.reserve(32);

		std::string sql = "SELECT sql FROM sqlite_master WHERE tbl_name='";
		sql += dbName;
		sql += "';";

		std::string r;
		soci::statement st = (dbc.getSession().prepare << sql,
			soci::into(r));
		st.execute();
		while (st.fetch())
		{
			schema.emplace_back(r);
		}

		return schema;
	}

	static bool schemaHas(
		DatabaseCon& dbc, std::string const& dbName, int line,
		std::string const& content, beast::Journal j)
	{
		std::vector<std::string> schema = getSchema(dbc, dbName);

		if (static_cast<int> (schema.size()) <= line)
		{
			JLOG(j.fatal()) << "Schema for " << dbName << " has too few lines";
			Throw<std::runtime_error>("bad schema");
		}

		return schema[line].find(content) != std::string::npos;
	}

	void SchemaImp::addTxnSeqField()
	{
		if (schemaHas(getTxnDB(), "AccountTransactions", 0, "TxnSeq", m_journal))
			return;

		JLOG(m_journal.warn()) << "Transaction sequence field is missing";

		auto& session = getTxnDB().getSession();

		std::vector< std::pair<uint256, int> > txIDs;
		txIDs.reserve(300000);

		JLOG(m_journal.info()) << "Parsing transactions";
		int i = 0;
		uint256 transID;

		boost::optional<std::string> strTransId;
		soci::blob sociTxnMetaBlob(session);
		soci::indicator tmi;
		Blob txnMeta;

		soci::statement st =
			(session.prepare <<
				"SELECT TransID, TxnMeta FROM Transactions;",
				soci::into(strTransId),
				soci::into(sociTxnMetaBlob, tmi));

		st.execute();
		while (st.fetch())
		{
			if (soci::i_ok == tmi)
				convert(sociTxnMetaBlob, txnMeta);
			else
				txnMeta.clear();

			std::string tid = strTransId.value_or("");
			transID.SetHex(tid, true);

			if (txnMeta.size() == 0)
			{
				txIDs.push_back(std::make_pair(transID, -1));
				JLOG(m_journal.info()) << "No metadata for " << transID;
			}
			else
			{
				TxMeta _(transID, 0, txnMeta, app_.journal("TxMeta"));
				txIDs.push_back(std::make_pair(transID, _.getIndex()));
			}

			if ((++i % 1000) == 0)
			{
				JLOG(m_journal.info()) << i << " transactions read";
			}
		}

		JLOG(m_journal.info()) << "All " << i << " transactions read";

		soci::transaction tr(session);

		JLOG(m_journal.info()) << "Dropping old index";
		session << "DROP INDEX AcctTxIndex;";

		JLOG(m_journal.info()) << "Altering table";
		session << "ALTER TABLE AccountTransactions ADD COLUMN TxnSeq INTEGER;";

		boost::format fmt("UPDATE AccountTransactions SET TxnSeq = %d WHERE TransID = '%s';");
		i = 0;
		for (auto& t : txIDs)
		{
			session << boost::str(fmt % t.second % to_string(t.first));

			if ((++i % 1000) == 0)
			{
				JLOG(m_journal.info()) << i << " transactions updated";
			}
		}

		JLOG(m_journal.info()) << "Building new index";
		session << "CREATE INDEX AcctTxIndex ON AccountTransactions(Account, LedgerSeq, TxnSeq, TransID);";

		tr.commit();
	}

	void SchemaImp::addValidationSeqFields()
	{
		if (schemaHas(getLedgerDB(), "Validations", 0, "LedgerSeq", m_journal))
		{
			assert(schemaHas(getLedgerDB(), "Validations", 0, "InitialSeq", m_journal));
			return;
		}

		JLOG(m_journal.warn()) << "Validation sequence fields are missing";
		assert(!schemaHas(getLedgerDB(), "Validations", 0, "InitialSeq", m_journal));

		auto& session = getLedgerDB().getSession();

		soci::transaction tr(session);

		JLOG(m_journal.info()) << "Altering table";
		session << "ALTER TABLE Validations "
			"ADD COLUMN LedgerSeq       BIGINT UNSIGNED;";
		session << "ALTER TABLE Validations "
			"ADD COLUMN InitialSeq      BIGINT UNSIGNED;";

		// Create the indexes, too, so we don't have to
		// wait for the next startup, which may be a while.
		// These should be identical to those in LedgerDBInit
		JLOG(m_journal.info()) << "Building new indexes";
		session << "CREATE INDEX IF NOT EXISTS "
			"ValidationsBySeq ON Validations(LedgerSeq);";
		session << "CREATE INDEX IF NOT EXISTS ValidationsByInitialSeq "
			"ON Validations(InitialSeq, LedgerSeq);";

		tr.commit();
	}

	bool SchemaImp::updateTables()
	{
		if (config_->section(ConfigSection::nodeDatabase()).empty())
		{
			JLOG(m_journal.fatal()) << "The [node_db] configuration setting has been updated and must be set";
			return false;
		}

		// perform any needed table updates
		assert(schemaHas(getTxnDB(), "AccountTransactions", 0, "TransID", m_journal));
		assert(!schemaHas(getTxnDB(), "AccountTransactions", 0, "foobar", m_journal));
		addTxnSeqField();

		if (schemaHas(getTxnDB(), "AccountTransactions", 0, "PRIMARY", m_journal))
		{
			JLOG(m_journal.fatal()) << "AccountTransactions database should not have a primary key";
			return false;
		}

		addValidationSeqFields();

		if (config_->doImport)
		{
			auto j = SchemaImp::journal("NodeObject");
			NodeStore::DummyScheduler scheduler;
			std::unique_ptr <NodeStore::Database> source =
				NodeStore::Manager::instance().make_Database("NodeStore.import",
					scheduler, 0, app_.getJobQueue(),
					config_->section(ConfigSection::importNodeDatabase()), j);

			JLOG(j.warn())
				<< "Node import from '" << source->getName() << "' to '"
				<< getNodeStore().getName() << "'.";

			getNodeStore().import(*source);
		}

		return true;
	}

	bool SchemaImp::nodeToShards()
	{
		assert(!config_->standalone());

		if (config_->section(ConfigSection::shardDatabase()).empty())
		{
			JLOG(m_journal.fatal()) <<
				"The [shard_db] configuration setting must be set";
			return false;
		}
		if (!shardStore_)
		{
			JLOG(m_journal.fatal()) <<
				"Invalid [shard_db] configuration";
			return false;
		}
		shardStore_->import(getNodeStore());
		return true;
	}

	bool SchemaImp::validateShards()
	{
		assert(!config_->standalone());

		if (config_->section(ConfigSection::shardDatabase()).empty())
		{
			JLOG(m_journal.fatal()) <<
				"The [shard_db] configuration setting must be set";
			return false;
		}
		if (!shardStore_)
		{
			JLOG(m_journal.fatal()) <<
				"Invalid [shard_db] configuration";
			return false;
		}
		shardStore_->validate();
		return true;
	}

	void SchemaImp::setMaxDisallowedLedger()
	{
		boost::optional <LedgerIndex> seq;
		{
			auto db = getLedgerDB().checkoutDb();
			*db << "SELECT MAX(LedgerSeq) FROM Ledgers;", soci::into(seq);
		}
		if (seq)
			maxDisallowedLedger_ = *seq;

		JLOG(m_journal.trace()) << "Max persisted ledger is "
			<< maxDisallowedLedger_;
	}

	bool SchemaImp::setSynTable()
	{
		auto conn = m_pTxStoreDBConn->GetDBConn();

		DatabaseCon::Setup setup = ripple::setup_SyncDatabaseCon(*config_);
		//sync_db not configured
		if (setup.sync_db.name() == "" && setup.sync_db.lines().size() == 0)
			return true;

		//sync_db configured,but not connected
		if (conn == nullptr || conn->getSession().get_backend() == NULL)
		{
			m_pTableSync->SetHaveSyncFlag(false);
			m_pTableStorage->SetHaveSyncFlag(false);
			//JLOG(m_journal.trace()) << "make db backends error,please check cfg!"; //can not log now!
			std::cerr << "make db backends error,please check cfg!" << std::endl;
			return false;
		}
		else
		{
			std::pair<std::string, bool> result = setup.sync_db.find("type");

			if (result.first.compare("sqlite") == 0 || result.first.empty() || !result.second)
				m_pTableStatusDB = std::make_unique<TableStatusDBSQLite>(conn, this, m_journal);
			else
				m_pTableStatusDB = std::make_unique<TableStatusDBMySQL>(conn, this, m_journal);

			if (m_pTableStatusDB)
			{
				bool bInitRet = m_pTableStatusDB->InitDB(setup);
				if (bInitRet)
					//JLOG(m_journal.info()) << "InitDB success";
					std::cout << "InitDB success" << std::endl;
				else
				{
					//JLOG(m_journal.info()) << "InitDB error";
					std::cerr << "InitDB error" << std::endl;
					return false;
				}
			}
			else
			{
				m_pTableSync->SetHaveSyncFlag(false);
				m_pTableStorage->SetHaveSyncFlag(false);
				//JLOG(m_journal.info()) << "fail to create sycstate table calss.";
				std::cerr << "fail to create sycstate table calss." << std::endl;
				return false;
			}

			if (m_txMaster.getClientTxStoreDBConn().GetDBConn() == NULL || m_txMaster.getConsensusTxStoreDBConn().GetDBConn() == NULL)
			{
				std::cerr << "db connection for consensus or tx check is null" << std::endl;
				return false;
			}
		}
		return true;
	}

	bool SchemaImp::checkCertificate()
	{
		auto const vecCrtPath = config_->section("x509_crt_path").values();
		if (vecCrtPath.empty()) {
			return true;
		}
		else if (!config_->ROOT_CERTIFICATES.empty()) {

			OpenSSL_add_all_algorithms();
			return true;
		}
		else {

			std::cerr << "Root certificate configuration error ,please check cfg!" << std::endl;
			return false;
		}
	}

	std::shared_ptr <Schema>
		make_Schema(
			SchemaParams const& params,
			std::shared_ptr<Config> config,
			Application& app,
			beast::Journal j)
	{

		return std::make_shared<SchemaImp>(params, config, app, j);
	}
}
