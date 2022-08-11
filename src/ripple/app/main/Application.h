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

#ifndef RIPPLE_APP_MAIN_APPLICATION_H_INCLUDED
#define RIPPLE_APP_MAIN_APPLICATION_H_INCLUDED

#include <ripple/shamap/FullBelowCache.h>
#include <ripple/shamap/TreeNodeCache.h>
#include <ripple/basics/TaggedCache.h>
#include <ripple/rpc/ServerHandler.h>
#include <ripple/core/Config.h>
#include <ripple/protocol/Protocol.h>
#include <ripple/beast/utility/PropertyStream.h>
#include <peersafe/gmencrypt/GmEncryptObj.h>
#include <peersafe/schema/SchemaParams.h>
#include <ripple/protocol/Protocol.h>
#include <boost/asio.hpp>
#include <memory>
#include <mutex>
#include <peersafe/app/prometh/PrometheusClient.h>
namespace ripple {

namespace unl {
class Manager;
}

namespace Resource {
class Manager;
}

namespace NodeStore {
class Database;
class DatabaseShard;
}  // namespace NodeStore

namespace perf {
class PerfLog;
}

// VFALCO TODO Fix forward declares required for header dependency loops
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
class LoadManager;
class ManifestCache;
class NetworkOPs;
class OpenLedger;
class OrderBookDB;
class Overlay;
class PathRequests;
class PendingSaves;
class PublicKey;
class SecretKey;
class ValidatorKeys;
class AccountIDCache;
class STLedgerEntry;
class TimeKeeper;
class TransactionMaster;
class TxQ;
class ValidatorList;
class ValidatorSite;
class UserCertList;
class PeerCertList;
class Cluster;
class PreContractFace;
class TxStoreDBConn;
class TxStore;
class TableStatusDB;
class TableSync;
class TableStorage;
class TableAssistant;
class ContractHelper;
class TableTxAccumulator;
class TxPool;
class StateManager;
class NodeStoreScheduler;
class DatabaseCon;
class TxnDBCon;
class SHAMapStore;
class ResolverAsio;
class ValidatorKeys;
class SchemaManager;
class PeerManager;
class PeerReservationTable;

using NodeCache = TaggedCache<SHAMapHash, Blob>;

template <class StalePolicy, class Adaptor>
class Validations;
class RCLValidationsPolicy;
class RCLValidationsAdaptor;
using RCLValidations = Validations<RCLValidationsPolicy, RCLValidationsAdaptor>;

namespace RPC {
class ShardArchiveHandler;
}

class Application : public beast::PropertyStream::Source
{
public:
    /* VFALCO NOTE

        The master mutex protects:

        - The open ledger
        - Server global state
            * What the last closed ledger is
            * State of the consensus engine

        other things
    */
    using MutexType = std::recursive_mutex;
    virtual MutexType&
    getMasterMutex() = 0;

public:
    Application();

    virtual ~Application() = default;

    virtual bool
    setup() = 0;
    virtual void
    doStart(bool withTimers) = 0;
    virtual void
    run() = 0;
    virtual bool
    isShutdown() = 0;
    virtual void
    signalStop() = 0;
    virtual bool
    checkSigs() const = 0;
    virtual void
    checkSigs(bool) = 0;

    virtual void
    doStopSchema(SchemaID schemaID) = 0;

    //
    // ---
    //

    virtual Logs&
    logs() = 0;
    virtual boost::asio::io_service&
    getIOService() = 0;
    virtual CollectorManager&
    getCollectorManager() = 0;
    virtual TimeKeeper&
    timeKeeper() = 0;
    virtual JobQueue&
    getJobQueue() = 0;
    virtual LoadManager&
    getLoadManager() = 0;
    virtual Overlay&
    overlay() = 0;
    virtual PeerCertList&
    peerCertList() = 0;
    virtual perf::PerfLog&
    getPerfLog() = 0;
    virtual Resource::Manager&
    getResourceManager() = 0;
    virtual NodeStoreScheduler&
    nodeStoreScheduler() = 0;
    virtual std::pair<PublicKey, SecretKey> const&
    nodeIdentity() = 0;
    virtual PublicKey const&
    getValidationPublicKey() const = 0;
    virtual ValidatorKeys const&
    getValidatorKeys() const = 0;
    virtual ResolverAsio&
    getResolver() = 0;
    virtual ServerHandler&
    getServerHandler() = 0;
    virtual SchemaManager&
    getSchemaManager() = 0;

    virtual bool
    hasSchema(SchemaID const& id = beast::zero) = 0;
    virtual Schema&
    getSchema(SchemaID const& id = beast::zero) = 0;
    virtual PeerManager&
    peerManager(SchemaID const& id = beast::zero) = 0;
    virtual Config&
    config(SchemaID const& id = beast::zero) = 0;
    virtual Family&
    getNodeFamily(SchemaID const& id = beast::zero) = 0;
    virtual Family*
    getShardFamily(SchemaID const& id = beast::zero) = 0;
    virtual NodeCache&
    getTempNodeCache(SchemaID const& id = beast::zero) = 0;
    virtual CachedSLEs&
    cachedSLEs(SchemaID const& id = beast::zero) = 0;
    virtual AmendmentTable&
    getAmendmentTable(SchemaID const& id = beast::zero) = 0;
    virtual HashRouter&
    getHashRouter(SchemaID const& id = beast::zero) = 0;
    virtual LoadFeeTrack&
    getFeeTrack(SchemaID const& id = beast::zero) = 0;
    virtual TxQ&
    getTxQ(SchemaID const& id = beast::zero) = 0;
    virtual ValidatorList&
    validators(SchemaID const& id = beast::zero) = 0;
    virtual ValidatorSite&
    validatorSites(SchemaID const& id = beast::zero) = 0;
    virtual UserCertList&
    userCertList(SchemaID const& id = beast::zero) = 0;
    virtual ManifestCache&
    validatorManifests(SchemaID const& id = beast::zero) = 0;
    virtual ManifestCache&
    publisherManifests(SchemaID const& id = beast::zero) = 0;
    virtual Cluster&
    cluster(SchemaID const& id = beast::zero) = 0;
    virtual RCLValidations&
    getValidations(SchemaID const& id = beast::zero) = 0;
    virtual NodeStore::Database&
    getNodeStore(SchemaID const& id = beast::zero) = 0;
    virtual NodeStore::DatabaseShard*
    getShardStore(SchemaID const& id = beast::zero) = 0;
    virtual RPC::ShardArchiveHandler*
    getShardArchiveHandler(
        SchemaID const& id = beast::zero,
        bool tryRecovery = false) = 0;
    virtual InboundLedgers&
    getInboundLedgers(SchemaID const& id = beast::zero) = 0;
    virtual InboundTransactions&
    getInboundTransactions(SchemaID const& id = beast::zero) = 0;
    virtual TaggedCache<uint256, AcceptedLedger>&
    getAcceptedLedgerCache(SchemaID const& id = beast::zero) = 0;
    virtual LedgerMaster&
    getLedgerMaster(SchemaID const& id = beast::zero) = 0;
    virtual NetworkOPs&
    getOPs(SchemaID const& id = beast::zero) = 0;
    virtual PeerReservationTable&
    peerReservations(SchemaID const& id = beast::zero) = 0;
    virtual OrderBookDB&
    getOrderBookDB(SchemaID const& id = beast::zero) = 0;
    virtual TransactionMaster&
    getMasterTransaction(SchemaID const& id = beast::zero) = 0;

    virtual PreContractFace&		
    getPreContractFace() = 0;
    virtual PromethExposer& 
    getPromethExposer() = 0;
    virtual TxStoreDBConn&
    getTxStoreDBConn(SchemaID const& id = beast::zero) = 0;
    virtual TxStore&
    getTxStore(SchemaID const& id = beast::zero) = 0;
    virtual TableStatusDB&
    getTableStatusDB(SchemaID const& id = beast::zero) = 0;
    virtual TableSync&
    getTableSync(SchemaID const& id = beast::zero) = 0;
    virtual TableStorage&
    getTableStorage(SchemaID const& id = beast::zero) = 0;
    virtual TableAssistant&
    getTableAssistant(SchemaID const& id = beast::zero) = 0;
    virtual ContractHelper&
    getContractHelper(SchemaID const& id = beast::zero) = 0;
    virtual TableTxAccumulator&
    getTableTxAccumulator(SchemaID const& id = beast::zero) = 0;
    virtual TxPool&
    getTxPool(SchemaID const& id = beast::zero) = 0;
    virtual StateManager&
    getStateManager(SchemaID const& id = beast::zero) = 0;
    virtual PathRequests&
    getPathRequests(SchemaID const& id = beast::zero) = 0;
    virtual SHAMapStore&
    getSHAMapStore(SchemaID const& id = beast::zero) = 0;
    virtual PendingSaves&
    pendingSaves(SchemaID const& id = beast::zero) = 0;
    virtual AccountIDCache const&
    accountIDCache(SchemaID const& id = beast::zero) const = 0;
    virtual OpenLedger&
    openLedger(SchemaID const& id = beast::zero) = 0;
    virtual OpenLedger const&
    openLedger(SchemaID const& id = beast::zero) const = 0;
    virtual TxnDBCon&
    getTxnDBCHECK(SchemaID const& id = beast::zero) = 0;
    virtual DatabaseCon&
    getLedgerDB(SchemaID const& id = beast::zero) = 0;
    /** Retrieve the "wallet database" */
    virtual DatabaseCon&
    getWalletDB(SchemaID const& id = beast::zero) = 0;

    virtual std::chrono::milliseconds
    getIOLatency() = 0;

    virtual bool
    serverOkay(std::string& reason) = 0;

    virtual beast::Journal
    journal(std::string const& name) = 0;

    virtual int
    fdRequired() const = 0;
};

std::unique_ptr<Application>
make_Application(
    std::shared_ptr<Config> config,
    std::unique_ptr<Logs> logs,
    std::unique_ptr<TimeKeeper> timeKeeper);

}  // namespace ripple

#endif
