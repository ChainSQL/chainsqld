#pragma once

#include <ripple/shamap/TreeNodeCache.h>
#include <ripple/basics/TaggedCache.h>
#include <ripple/core/Config.h>
#include <peersafe/schema/SchemaParams.h>
#include <ripple/protocol/Protocol.h>
#include <boost/asio.hpp>
#include <ripple/core/Stoppable.h>
namespace ripple {

namespace perf {
class PerfLog;
}

namespace Resource {
class Manager;
}

namespace NodeStore {
class Database;
class DatabaseShard;
}  // namespace NodeStore

class Application;
class Config;
class AmendmentTable;
class CachedSLEs;
class CollectorManager;
class Family;
class HashRouter;
class Logs;
class LoadFeeTrack;
class JobQueue;
class InboundLedgers;
class InboundTransactions;
class AcceptedLedger;
class LedgerMaster;
class ManifestCache;
class NetworkOPs;
class OpenLedger;
class OrderBookDB;
class PeerManager;
class PathRequests;
class PendingSaves;
class PublicKey;
class SecretKey;
class AccountIDCache;
class STLedgerEntry;
class TimeKeeper;
class TransactionMaster;
class TxQ;
class ValidatorList;
class ValidatorSite;
class UserCertList;
class Cluster;
class NodeStoreScheduler;
class TxStoreDBConn;
class PreContractFace;
class TxStore;
class TableStatusDB;
class TableSync;
class TableStorage;
class TableAssistant;
class ContractHelper;
class TableTxAccumulator;
class TxPool;
class StateManager;
class LoadManager;
class ValidatorKeys;
class NodeFamily;
class ShardFamily;
class PeerReservationTable;
class DatabaseCon;
class TxnDBCon;
class SHAMapStore;
class SchemaManager;
class ConnectionPool;
class PrometheusClient;
using NodeCache = TaggedCache<SHAMapHash, Blob>;

template <class StalePolicy, class Adaptor>
class Validations;
class RCLValidationsPolicy;
class RCLValidationsAdaptor;
using RCLValidations = Validations<RCLValidationsPolicy, RCLValidationsAdaptor>;

namespace RPC {
class ShardArchiveHandler;
}

class Schema
{
public:
    Schema() {}
    virtual ~Schema() = default;

public:
    virtual bool
    setup() = 0;
    virtual void
    doSweep() = 0;
    virtual bool
    isShutdown() = 0;
    virtual JobCounter&
    doJobCounter() = 0;
    virtual void
    doStop() = 0;
    virtual void
    doStart() = 0;
    virtual bool
    checkSigs() const = 0;
    virtual void
    checkSigs(bool) = 0;

    virtual std::recursive_mutex&
    getMasterMutex() = 0;
    virtual beast::Journal
    journal(std::string const& name) = 0;

    // Share the same one from Application
    virtual Application&
    app() = 0;
    virtual Logs&
    logs() = 0;
    virtual CollectorManager&
    getCollectorManager() = 0;
    virtual TimeKeeper&
    timeKeeper() = 0;
    virtual boost::asio::io_service&
    getIOService() = 0;
    virtual JobQueue&
    getJobQueue() = 0;
    virtual LoadManager&
    getLoadManager() = 0;
    virtual perf::PerfLog&
    getPerfLog() = 0;
    virtual Resource::Manager&
    getResourceManager() = 0;
    virtual std::pair<PublicKey, SecretKey> const&
    nodeIdentity() = 0;
    virtual PublicKey const&
    getValidationPublicKey() const = 0;
    virtual ValidatorKeys const&
    getValidatorKeys() const = 0;

    // Different shema by schema
    virtual Config&
    config() = 0;
    virtual Family&
    getNodeFamily() = 0;
    virtual Family*
    getShardFamily() = 0;
    virtual AmendmentTable&
    getAmendmentTable() = 0;
    virtual HashRouter&
    getHashRouter() = 0;
    virtual LoadFeeTrack&
    getFeeTrack() = 0;
    virtual PeerManager&
    peerManager() = 0;
    virtual TxQ&
    getTxQ() = 0;
    virtual ValidatorList&
    validators() = 0;
    virtual ValidatorSite&
    validatorSites() = 0;
    virtual UserCertList&
    userCertList() = 0;
    virtual ManifestCache&
    validatorManifests() = 0;
    virtual ManifestCache&
    publisherManifests() = 0;
    virtual Cluster&
    cluster() = 0;
    virtual RCLValidations&
    getValidations() = 0;
    virtual NodeStore::Database&
    getNodeStore() = 0;
    virtual NodeStore::DatabaseShard*
    getShardStore() = 0;
    virtual RPC::ShardArchiveHandler*
    getShardArchiveHandler(bool tryRecovery = false) = 0;
    virtual InboundLedgers&
    getInboundLedgers() = 0;
    virtual InboundTransactions&
    getInboundTransactions() = 0;
    virtual TaggedCache<uint256, AcceptedLedger>&
    getAcceptedLedgerCache() = 0;
    virtual LedgerMaster&
    getLedgerMaster() = 0;
    virtual NetworkOPs&
    getOPs() = 0;
    virtual PeerReservationTable&
    peerReservations() = 0;
    virtual OrderBookDB&
    getOrderBookDB() = 0;
    virtual TransactionMaster&
    getMasterTransaction() = 0;
    virtual TxStoreDBConn&
    getTxStoreDBConn() = 0;
    virtual PreContractFace&		
    getPreContractFace() = 0;
    virtual TxStore&
    getTxStore() = 0;
    virtual TableStatusDB&
    getTableStatusDB() = 0;
    virtual TableSync&
    getTableSync() = 0;
    virtual TableStorage&
    getTableStorage() = 0;
    virtual TableAssistant&
    getTableAssistant() = 0;
    virtual ContractHelper&
    getContractHelper() = 0;
    virtual TableTxAccumulator&
    getTableTxAccumulator() = 0;
    virtual TxPool&
    getTxPool() = 0;
    virtual StateManager&
    getStateManager() = 0;
    virtual ConnectionPool&
    getConnectionPool() = 0;
    virtual PrometheusClient&
    getPrometheusClient() = 0;

    virtual PathRequests&
    getPathRequests() = 0;
    virtual SHAMapStore&
    getSHAMapStore() = 0;
    virtual PendingSaves&
    pendingSaves() = 0;
    virtual AccountIDCache const&
    accountIDCache() const = 0;
    virtual OpenLedger&
    openLedger() = 0;
    virtual OpenLedger const&
    openLedger() const = 0;
    virtual OpenLedger&
    checkedOpenLedger() = 0;
    virtual NodeCache&
    getTempNodeCache() = 0;
    virtual CachedSLEs&
    cachedSLEs() = 0;

    virtual TxnDBCon&
    getTxnDBCHECK() = 0;
    virtual DatabaseCon&
    getLedgerDB() = 0;
    virtual DatabaseCon&
    getWalletDB() = 0;
    /** Ensure that a newly-started validator does not sign proposals older
     * than the last ledger it persisted. */
    virtual LedgerIndex
    getMaxDisallowedLedger() = 0;
    virtual SchemaParams
    getSchemaParams() = 0;
    virtual bool
    initBeforeSetup() = 0;
    virtual bool
    available() = 0;
    virtual SchemaID
    schemaId() = 0;
    virtual SchemaManager&
    getSchemaManager() = 0;
    virtual bool
    getWaitinBeginConsensus() = 0;
    virtual Stoppable&
    getStoppable() = 0;
};

std::shared_ptr<Schema>
make_Schema(
    SchemaParams const& params,
    std::shared_ptr<Config> config,
    Application& app,
    beast::Journal j);


}  // namespace ripple
