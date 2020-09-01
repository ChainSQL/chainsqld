//------------------------------------------------------------------------------
/*
This file is part of chainsqld: https://github.com/chainsql/chainsqld
Copyright (c) 2016-2018 Peersafe Technology Co., Ltd.

chainsqld is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

chainsqld is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
//==============================================================================

#include <peersafe/app/shard/Lookup.h>
#include <peersafe/app/shard/ShardManager.h>
#include <peersafe/app/shard/FinalLedger.h>
#include <peersafe/app/misc/TxPool.h>
#include <peersafe/app/consensus/ViewChange.h>
#include <peersafe/app/table/TableSync.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/ledger/OpenLedger.h>
#include <ripple/app/ledger/InboundLedgers.h>
#include <ripple/app/misc/HashRouter.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/app/misc/Transaction.h>
#include <ripple/app/consensus/RCLConsensus.h>
#include <ripple/app/consensus/RCLValidations.h>
#include <ripple/app/consensus/RCLCxLedger.h>
#include <ripple/basics/make_lock.h>
#include <ripple/basics/random.h>
#include <ripple/basics/Slice.h>
#include <ripple/core/Config.h>
#include <ripple/core/ConfigSections.h>
#include <ripple/overlay/Peer.h>
#include <ripple/overlay/impl/PeerImp.h>
#include <ripple/protocol/digest.h>


namespace ripple {

Lookup::Lookup(ShardManager& m, Application& app, Config& cfg, beast::Journal journal)
    : mShardManager(m)
    , app_(app)
    , journal_(journal)
    , cfg_(cfg)
	, mTimer(app_.getIOService())
{
    if (cfg_.LOOKUP_RELAY_INTERVAL)
    {
        mRelayInterval = cfg_.LOOKUP_RELAY_INTERVAL;
    }

	mValidators = std::make_unique<ValidatorList>(
			app_.validatorManifests(), app_.publisherManifests(), app_.timeKeeper(),
			journal_, cfg_.VALIDATION_QUORUM);

	// Setup validators
	if (!mValidators->load(
		app_.getValidationPublicKey(),
        cfg_.LOOKUP_PUBLIC_KEYS,
		cfg_.section(SECTION_VALIDATOR_LIST_KEYS).values(),
        mShardManager.myShardRole() & ShardManager::LOOKUP))
	{
        Throw<std::runtime_error>("Lookup validators load failed");
	}

    if (mShardManager.myShardRole() & ShardManager::LOOKUP)
    {
        setTimer();
    }
}

// ------------------------------------------------------------------------------
// Peers related

void Lookup::addActive(std::shared_ptr<PeerImp> const& peer)
{
    std::lock_guard <decltype(mPeersMutex)> lock(mPeersMutex);
    mPeers.emplace_back(std::move(peer));
}

void Lookup::eraseDeactivate()
{
    std::lock_guard <decltype(mPeersMutex)> lock(mPeersMutex);

    for (auto w = mPeers.begin(); w != mPeers.end();)
    {
        auto p = w->lock();
        if (!p)
        {
            w = mPeers.erase(w);
        }
        else
        {
            w++;
        }
    }
}

// ------------------------------------------------------------------------------
// Ledger persistence

void Lookup::checkSaveLedger(LedgerIndex netLedger)
{
    LedgerIndex validSeq = app_.getLedgerMaster().getValidLedgerIndex();

    {
        std::lock_guard <std::recursive_mutex> lock(mutex_);

        if (netLedger <= validSeq)
        {
            JLOG(journal_.warn()) << "Net ledger: " << netLedger << " is stale, currently need: " << validSeq + 1;
            mMapFinalLedger.erase(netLedger);
            mMapMicroLedgers.erase(netLedger);
            return;
        }

        if (!checkLedger(netLedger))
        {
            JLOG(journal_.info()) << "Net ledger " << netLedger << " collected not fully";
            return;
        }

        if (netLedger > mNetLedger)
        {
            // Save the newest seq we collected fully.
            mNetLedger = netLedger;
        }

        if (!mSaveLedgerThread.test_and_set(std::memory_order_relaxed))
        {
            app_.getJobQueue().addJob(
                jtSAVELEDGER, "saveLedger",
                [this](Job&) { saveLedgerThread(); });
        }
    }
}

void Lookup::saveLedgerThread()
{
    LedgerIndex saveOrAcquire = 0;
    bool shouldAcquire = findNewLedgerToSave(saveOrAcquire);

    while (shouldAcquire || saveOrAcquire)
    {
        if (shouldAcquire)
        {
            auto validIndex = app_.getLedgerMaster().getValidLedgerIndex();
            JLOG(journal_.warn()) << "SaveLedgerThread, I'm on "
                << validIndex + 1 << ", trying to acquiring  " << saveOrAcquire;
            {
                // do clear
                std::lock_guard <std::recursive_mutex> lock(mutex_);
                for (auto seq = validIndex + 1; seq <= saveOrAcquire; seq++)
                {
                    mMapFinalLedger.erase(seq);
                    mMapMicroLedgers.erase(seq);
                }
            }

            // Should acquire.
            auto const& hash = getFinalLedger(saveOrAcquire + 1)->parentHash();
            if (!doAcquire(hash, saveOrAcquire))
            {
                break;
            }
        }
        else
        {
            if (!mInitialized) mInitialized = true;
            JLOG(journal_.info()) << "Building ledger " << saveOrAcquire;
            // Reset tx-meta transactionIndex field
            auto timeStart = utcTime();
            uint32 count = resetMetaIndex(saveOrAcquire);
            JLOG(journal_.info()) << "resetMetaIndex tx count: " << count
                << " time used: " << utcTime() - timeStart << "ms";
            // Save ledger.
            timeStart = utcTime();
            saveLedger(saveOrAcquire);
            JLOG(journal_.info()) << "saveLedger time used:" << utcTime() - timeStart << "ms";

            {
                // do clear
                std::lock_guard <std::recursive_mutex> lock(mutex_);
                mMapFinalLedger.erase(saveOrAcquire);
                mMapMicroLedgers.erase(saveOrAcquire);
            }
        }

        shouldAcquire = findNewLedgerToSave(saveOrAcquire);
    }

    mSaveLedgerThread.clear(std::memory_order_relaxed);
}

bool Lookup::findNewLedgerToSave(LedgerIndex &toSaveOrAcquire)
{
    std::lock_guard <std::recursive_mutex> lock(mutex_);

    LedgerIndex validSeq = app_.getLedgerMaster().getValidLedgerIndex();

    if (checkLedger(validSeq + 1))
    {
        toSaveOrAcquire = validSeq + 1;
        return false;
    }

    for (LedgerIndex seq = mNetLedger; seq > validSeq; seq--)
    {
        if (!checkLedger(seq))
        {
            assert(seq < mNetLedger);
            toSaveOrAcquire = seq;
            return true;
        }
    }

    toSaveOrAcquire = 0;
    return false;
}

bool Lookup::checkLedger(LedgerIndex seq)
{
    std::lock_guard <std::recursive_mutex> lock(mutex_);

    if (mMapFinalLedger.count(seq) &&
        mMapMicroLedgers[seq].size() == mMapFinalLedger[seq]->getMicroLedgerCount())
    {
        return true;
    }

    return false;
}

uint32 Lookup::resetMetaIndex(LedgerIndex seq)
{
    uint32 metaIndex = 0;
    beast::Journal j = app_.journal("TxMeta");

    std::uint32_t shardCount = getFinalLedger(seq)->getMicroLedgerCount() - 1;

    for (uint32 shardIndex = 1; shardIndex <= shardCount; shardIndex++)
    {
        std::shared_ptr<MicroLedger> microLedger = getMicroLedger(seq, shardIndex);
        for (auto const& txHash : microLedger->txHashes())
        {
            microLedger->setMetaIndex(txHash, metaIndex++, j);
        }
    }

    std::shared_ptr<MicroLedger> microLedger = getMicroLedger(seq, 0);
    for (auto const& txHash : microLedger->txHashes())
    {
        microLedger->setMetaIndex(txHash, metaIndex++, j);
    }

    return metaIndex;
}

void Lookup::saveLedger(LedgerIndex seq)
{
	auto finalLedger = getFinalLedger(seq);
	//build new ledger
	auto const& previousLedger = app_.getLedgerMaster().getValidatedLedger();
	auto const& ledgerInfo = finalLedger->getLedgerInfo();
	auto ledgerToSave = std::make_shared<Ledger>(
        *previousLedger, ledgerInfo.closeTime);

    auto timeStart = utcTime();

	//apply state
    finalLedger->apply(*ledgerToSave, false);

    std::uint32_t shardCount = finalLedger->getMicroLedgerCount() - 1;

    for (uint32 shardIndex = 0; shardIndex <= shardCount; shardIndex++)
    {
        std::shared_ptr<MicroLedger> microLedger = getMicroLedger(seq, shardIndex);
        microLedger->apply(*ledgerToSave);
    }

    JLOG(journal_.info()) << "Aplly state and tx map time used:" << utcTime() - timeStart << "ms";

    ledgerToSave->updateAmendments(app_);
	ledgerToSave->updateSkipList();
	{
		// Write the final version of all modified SHAMap
		// nodes to the node store to preserve the new Ledger
		int asf = ledgerToSave->stateMap().flushDirty(
			hotACCOUNT_NODE, ledgerToSave->info().seq);
		int tmf = ledgerToSave->txMap().flushDirty(
			hotTRANSACTION_NODE, ledgerToSave->info().seq);
		JLOG(journal_.debug()) << "Flushed " << asf << " accounts and " << tmf
			<< " transaction nodes";
	}

    ledgerToSave->unshare();
    ledgerToSave->setAccepted(ledgerInfo.closeTime, ledgerInfo.closeTimeResolution, true, app_.config());

    if (ledgerInfo.accountHash != ledgerToSave->stateMap().getHash().as_uint256() ||
        ledgerInfo.txHash != ledgerToSave->txMap().getHash().as_uint256() ||
        ledgerInfo.hash != ledgerToSave->info().hash)
    {
        JLOG(journal_.warn()) << "Final ledger txs/accounts shamap root hash missmatch";
        return;
    }

    app_.getLedgerMaster().storeLedger(ledgerToSave);

    app_.getOPs().getConsensus().adaptor_.notify(protocol::neACCEPTED_LEDGER, RCLCxLedger{ ledgerToSave }, true);

    //save ledger
    app_.getLedgerMaster().accept(ledgerToSave);

    // Build new open ledger for newRound
    {
        auto lock = make_lock(app_.getMasterMutex(), std::defer_lock);
        auto sl = make_lock(app_.getLedgerMaster().peekMutex(), std::defer_lock);
        std::lock(lock, sl);

        auto const& lastVal = app_.getLedgerMaster().getValidatedLedger();
        boost::optional<Rules> rules;
        rules.emplace(*lastVal, app_.config().features);

        app_.openLedger().accept(*rules, ledgerToSave);
    }

    //app_.getLedgerMaster().setClosedLedger(ledgerToSave);
}

bool Lookup::doAcquire(LedgerHash const& hash, LedgerIndex seq)
{
    if (auto const& ledger = app_.getInboundLedgers().acquire(
        hash, 0, InboundLedger::fcCURRENT))
    {
        JLOG(journal_.warn()) << "Acquired " << seq << ", try local successed";
        if (mInitialized)
        {
            app_.getLedgerMaster().accept(ledger);
        }
        else
        {
            mInitialized = true;
            app_.getLedgerMaster().setFullLedger(ledger, false, true);
            app_.getLedgerMaster().setPubLedger(ledger);
            app_.getLedgerMaster().tryAdvance();
            if (app_.getShardManager().myShardRole() & ShardManager::SYNC)
            {
                app_.getTableSync().TryTableSync();
            }
        }
        app_.getOPs().switchLastClosedLedger(ledger);

        return true;
    }

    return false;
}

// ------------------------------------------------------------------------------
// Message interface

void Lookup::onMessage(std::shared_ptr<protocol::TMMicroLedgerSubmit> const& m)
{
	auto microWithMeta = std::make_shared<MicroLedger>(*m);

    if (!app_.getHashRouter().shouldRelay(microWithMeta->ledgerHash()))
    {
        JLOG(journal_.info()) << "MicroLeger: duplicate";
        return;
    }

    if (microWithMeta->seq() <= app_.getLedgerMaster().getValidLedgerIndex())
    {
        JLOG(journal_.info()) << "MicroLeger: stale";
        return;
    }

    bool valid = false;
    if (microWithMeta->shardID())
    {
        valid = microWithMeta->checkValidity(
            mShardManager.node().shardValidators()[microWithMeta->shardID()],
            true);
    }
    else
    {
        valid = microWithMeta->checkValidity(
            mShardManager.committee().validatorsPtr(),
            true);
    }
	if (!valid)
	{
        JLOG(journal_.info()) << "Microledger signature verification failed";
		return;
	}

    saveMicroLedger(microWithMeta);
	checkSaveLedger(microWithMeta->seq());
}

void Lookup::onMessage(std::shared_ptr<protocol::TMFinalLedgerSubmit> const& m)
{
	auto finalLedger = std::make_shared<FinalLedger>(*m);

    if (!app_.getHashRouter().shouldRelay(finalLedger->ledgerHash()))
    {
        JLOG(journal_.info()) << "FinalLeger: duplicate";
        return;
    }

    if (finalLedger->seq() <= app_.getLedgerMaster().getValidLedgerIndex())
    {
        JLOG(journal_.info()) << "FinalLedger: stale";
        return;
    }

	bool valid = finalLedger->checkValidity(mShardManager.committee().validatorsPtr());

	if (!valid)
	{
        JLOG(journal_.info()) << "FinalLeger signature verification failed";
        return;
	}

    saveFinalLedger(finalLedger);
	checkSaveLedger(finalLedger->seq());

    mShardManager.checkValidatorLists();
}

void Lookup::onMessage(std::shared_ptr<protocol::TMCommitteeViewChange> const& m)
{
    auto committeeVC = std::make_shared<CommitteeViewChange>(*m);

    if (!app_.getHashRouter().shouldRelay(committeeVC->suppressionID()))
    {
        JLOG(journal_.info()) << "Committee view change: duplicate";
        return;
    }

    if (!committeeVC->checkValidity(mShardManager.committee().validatorsPtr()))
    {
        JLOG(journal_.info()) << "Committee view change signature verification failed";
        return;
    }

    if (committeeVC->preSeq() > app_.getLedgerMaster().getValidLedgerIndex())
    {
        auto seq = committeeVC->preSeq();
        if (checkLedger(seq))
        {
            JLOG(journal_.warn()) << "Got CommitteeViewChange preSeq " << seq
                << ", I'm on " << app_.getLedgerMaster().getValidLedgerIndex()
                << ", saveLedger job is lagging";
            checkSaveLedger(seq);
        }
        else
        {
            JLOG(journal_.warn()) << "Got CommitteeViewChange, I'm on "
                << app_.getLedgerMaster().getValidLedgerIndex()
                << ", trying to acquiring  " << seq;

            doAcquire(committeeVC->preHash(), seq);
        }
    }
    else if (committeeVC->preSeq() == app_.getLedgerMaster().getValidLedgerIndex())
    {
        app_.getLedgerMaster().onViewChanged(false, app_.getLedgerMaster().getValidatedLedger());

        app_.getOPs().getConsensus().adaptor_.notify(
            protocol::neACCEPTED_LEDGER,
            RCLCxLedger{ app_.getLedgerMaster().getValidatedLedger() },
            true);

        mShardManager.checkValidatorLists();
    }
    else
    {
        JLOG(journal_.warn()) << "Committee view change: stale";
    }

    return;
}

void Lookup::sendMessage(std::shared_ptr<Message> const &m)
{
    std::lock_guard<std::recursive_mutex> _(mPeersMutex);

    for (auto w : mPeers)
    {
        if (auto p = w.lock())
        {
            JLOG(journal_.info()) << "sendMessage "
                << TrafficCount::getName(static_cast<TrafficCount::category>(m->getCategory()))
                << " to lookup[" << p->getShardRole() << ":" << p->getShardIndex()
                << ":" << p->getRemoteAddress() << "]";
            p->send(m);
        }
    }
}

void Lookup::distributeMessage(std::shared_ptr<Message> const &m, bool forceBroadcast)
{
    if (forceBroadcast)
    {
        sendMessage(m);
        return;
    }

    auto senderCounts = mShardManager.nodeBase().validatorsPtr()->validators().size();
    auto quorum = mShardManager.nodeBase().quorum();
    auto maybeInsane = senderCounts - quorum;
    auto groupMemberCounts = maybeInsane + 1;   // Make sure that each group has at least one sane node.
    auto groupCounts = senderCounts / groupMemberCounts;

    if (groupCounts <= 1)
    {
        sendMessage(m);
        return;
    }

    auto myPubkeyIndex = mShardManager.nodeBase().getPubkeyIndex(app_.getValidationPublicKey());
    auto myGroupIndex = myPubkeyIndex != -1 ? myPubkeyIndex / groupMemberCounts : rand_int(groupCounts - 1);
    if (myGroupIndex >= groupCounts)
    {
        myGroupIndex = groupCounts - 1;
    }
    JLOG(journal_.info()) << "myPubkeyIndex:" << myPubkeyIndex << " myGroupIndex:" << myGroupIndex;

    std::vector<std::shared_ptr<Peer>> lookupPeers;
    {
        std::lock_guard<std::recursive_mutex> lock(mPeersMutex);
        for (auto w : mPeers)
        {
            if (auto p = w.lock())
            {
                lookupPeers.emplace_back(std::move(p));
            }
        }
    }

    std::sort(lookupPeers.begin(), lookupPeers.end(),
        [](std::shared_ptr<Peer const> const& p1, std::shared_ptr<Peer const> const& p2) {
        return publicKeyComp(p1->getNodePublic(), p2->getNodePublic());
    });

    auto dstGroupMemberCounts = lookupPeers.size() / groupCounts;
    if (lookupPeers.size() % groupCounts > 0)
    {
        dstGroupMemberCounts++;
    }

    auto myDstPeersLo = myGroupIndex * dstGroupMemberCounts;
    auto myDstPeersHi = myDstPeersLo + dstGroupMemberCounts;
    if (myDstPeersHi > lookupPeers.size())
    {
        myDstPeersHi = lookupPeers.size();
    }
    JLOG(journal_.info()) << "myDstPeersLo:" << myDstPeersLo << " myDstPeersHi:" << myDstPeersHi;

    for (auto i = myDstPeersLo; i < myDstPeersHi; i++)
    {
        JLOG(journal_.info()) << "distributeMessage "
            << TrafficCount::getName(static_cast<TrafficCount::category>(m->getCategory()))
            << " to lookup[" << lookupPeers[i]->getShardRole() << ":" << lookupPeers[i]->getShardIndex()
            << ":" << lookupPeers[i]->getRemoteAddress() << "]";
        lookupPeers[i]->send(m);
    }
}


// ------------------------------------------------------------------------------
// Transactions relay

void Lookup::setTimer()
{
	mTimer.expires_from_now(std::chrono::milliseconds(mRelayInterval));
	mTimer.async_wait(std::bind(&Lookup::onTimer, this, std::placeholders::_1));
}

void Lookup::onTimer(boost::system::error_code const& ec)
{
	if (ec == boost::asio::error::operation_aborted)
		return;

	if (!app_.getTxPool().isEmpty()) {

		app_.getJobQueue().addJob(jtRELAYTXS, "Lookup.relayTxs",
			[this](Job&) {
			relayTxs();
		});
	}

	setTimer();
}

std::uint32_t Lookup::getShardIndex(AccountID const& fromAddr) const
{
    std::uint32_t shardCount = mShardManager.shardCount();

    if (shardCount == 0) return 0;

    std::string sAccountID = toBase58(fromAddr);
    std::uint32_t len = sAccountID.size();
    assert(len >= 4);

    std::uint32_t x = 0;

    // Take the last four bytes of the address
    for (std::uint32_t i = 0; i < 4; i++)
    {
        x = (x << 8) | sAccountID[len - 4 + i];
    }

    return x % shardCount + 1;
}

void Lookup::relayTxs()
{
    std::lock_guard <decltype(mTransactionsMutex)> lock(mTransactionsMutex);

    auto timeStart = utcTime();

    std::vector< std::shared_ptr<Transaction> > txs;

    auto const& hTxVector = app_.getTxPool().topTransactions(MinTxsInLedgerAdvance);
    app_.getTxPool().getTransactions(hTxVector, txs);

    std::map <std::uint32_t, std::vector<std::shared_ptr<Transaction>> > mapShardIndexTxs;

    // tx shard
    for (auto& tx : txs)
    {
        mapShardIndexTxs[tx->getShardIndex()].push_back(tx);
    }

    // send to shard
    for (auto const& it : mapShardIndexTxs)
    {
        protocol::TMTransactions ts;
        sha512_half_hasher signHash;

        ts.set_nodepubkey(app_.getValidationPublicKey().data(),
            app_.getValidationPublicKey().size());

        for (auto const& tx : it.second)
        {
            protocol::TMTransaction& t = *ts.add_transactions();
            Serializer s;
            tx->getSTransaction()->add(s);
            t.set_rawtransaction(s.data(), s.size());
            t.set_status(protocol::tsCURRENT);
            //t.set_receivetimestamp(app_.timeKeeper().now().time_since_epoch().count());

            hash_append(signHash, tx->getID());
        }

        auto sign = signDigest(app_.getValidationPublicKey(),
            app_.getValidationSecretKey(),
            static_cast<typename sha512_half_hasher::result_type>(signHash));
        ts.set_signature(sign.data(), sign.size());

        auto const m = std::make_shared<Message>(
            ts, protocol::mtTRANSACTIONS);

        if (it.first)
        {
            mShardManager.node().sendMessage(it.first, m);
        }
        else
        {
            mShardManager.committee().sendMessage(m);
        }
    }

    for (auto const& delHash : hTxVector) {
        app_.getTxPool().removeTx(delHash);
    }

    JLOG(journal_.info()) << "relayTxs " << hTxVector.size()
        << " txs, time used: " << utcTime() - timeStart << "ms";
}


}
