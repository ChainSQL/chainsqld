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
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/ledger/OpenLedger.h>
#include <ripple/app/misc/HashRouter.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/app/misc/Transaction.h>
#include <ripple/app/consensus/RCLConsensus.h>
#include <ripple/app/consensus/RCLCxLedger.h>
#include <ripple/basics/make_lock.h>
#include <ripple/basics/Slice.h>
#include <ripple/core/Config.h>
#include <ripple/core/ConfigSections.h>
#include <ripple/overlay/Peer.h>
#include <ripple/overlay/impl/PeerImp.h>
#include <ripple/protocol/digest.h>


namespace ripple {

// Timeout interval in milliseconds
auto constexpr ML_RELAYTXS_TIMEOUT = 300ms;

Lookup::Lookup(ShardManager& m, Application& app, Config& cfg, beast::Journal journal)
    : mShardManager(m)
    , app_(app)
    , journal_(journal)
    , cfg_(cfg)
	, mTimer(app_.getIOService())
{
    // TODO initial peers and validators

	mValidators = std::make_unique<ValidatorList>(
			app_.validatorManifests(), app_.publisherManifests(), app_.timeKeeper(),
			journal_, cfg_.VALIDATION_QUORUM);

    std::vector<std::string> & lookupValidators = cfg_.LOOKUP_PUBLIC_KEYS;
	std::vector<std::string>  publisherKeys;
	// Setup trusted validators
	if (!mValidators->load(
		app_.getValidationPublicKey(),
        lookupValidators,
		publisherKeys,
        mShardManager.myShardRole() & ShardManager::LOOKUP))
	{
        Throw<std::runtime_error>("Lookup validators load failed");
	}

    mValidators->onConsensusStart(app_.getValidations().getCurrentPublicKeys());

    if (mShardManager.myShardRole() & ShardManager::LOOKUP)
    {
        setTimer();
    }
}

void Lookup::checkSaveLedger()
{
    LedgerIndex ledgerIndexToSave = app_.getLedgerMaster().getValidLedgerIndex() + 1;

    {
        std::lock_guard <std::recursive_mutex> lock(mutex_);

        if (!checkLedger(ledgerIndexToSave))
        {
            JLOG(journal_.info()) << "Ledger " << ledgerIndexToSave << " collected not fully";
            return;
        }

        if (app_.getLedgerMaster().getBuildingLedger() == ledgerIndexToSave)
        {
            JLOG(journal_.warn()) << "Currently build ledger " << ledgerIndexToSave;
            return;
        }

        app_.getLedgerMaster().setBuildingLedger(ledgerIndexToSave);
    }

    JLOG(journal_.info()) << "Building ledger " << ledgerIndexToSave;

	//reset tx-meta transactionIndex field
	resetMetaIndex(ledgerIndexToSave);

	//save this ledger
    saveLedger(ledgerIndexToSave);

    {
        // do clear
        std::lock_guard <std::recursive_mutex> lock(mutex_);
        mMapFinalLedger.erase(ledgerIndexToSave);
        mMapMicroLedgers.erase(ledgerIndexToSave);
    }

    // Next ledger maybe collected fully
    checkSaveLedger();
}

bool Lookup::checkLedger(LedgerIndex seq)
{
    std::lock_guard <std::recursive_mutex> lock(mutex_);
    if (mMapMicroLedgers[seq].size() == mShardManager.shardCount() &&
        mMapFinalLedger.count(seq))
    {
        return true;
    }

    return false;
}

void Lookup::resetMetaIndex(LedgerIndex seq)
{
    auto timeStart = utcTime();

    std::shared_ptr<FinalLedger> finalLedger = getFinalLedger(seq);

	auto vecHashes = finalLedger->getTxHashes();
	for (int i = 0; i < vecHashes.size(); i++)
	{
		for (int shardIndex = 1; shardIndex <= mShardManager.shardCount(); shardIndex++)
		{
            std::shared_ptr<MicroLedger> &microLedger = getMicroLedger(seq, shardIndex);
			if (microLedger->hasTxWithMeta(vecHashes[i]))
                microLedger->setMetaIndex(vecHashes[i], i, app_.journal("TxMeta"));
		}
	}

    JLOG(journal_.info()) << "resetMetaIndex tx count: " << vecHashes.size()
        << " time used: " << utcTime() - timeStart << "ms";
}

void Lookup::saveLedger(LedgerIndex seq)
{
	auto finalLedger = getFinalLedger(seq);
	//build new ledger
	auto previousLedger = app_.getLedgerMaster().getValidatedLedger();
	auto ledgerInfo = finalLedger->getLedgerInfo();
	auto ledgerToSave = std::make_shared<Ledger>(
        *previousLedger, ledgerInfo.closeTime);

	//apply state
    finalLedger->apply(*ledgerToSave);

    auto timeStart = utcTime();
	//build tx-map
	for (auto const& txID : finalLedger->getTxHashes())
	{
		for (int shardIndex = 1; shardIndex <= mShardManager.shardCount(); shardIndex++)
		{
            std::shared_ptr<MicroLedger> &microLedger = getMicroLedger(seq, shardIndex);
			if (microLedger->hasTxWithMeta(txID))
			{
				auto txWithMeta = microLedger->getTxWithMeta(txID);
				auto tx = txWithMeta.first;
				auto meta = txWithMeta.second;

                Serializer s(tx->getDataLength() +
                    meta->getDataLength() + 16);
                s.addVL(tx->peekData());
                s.addVL(meta->peekData());
                auto item = std::make_shared<SHAMapItem const>(txID, std::move(s));
                if (!ledgerToSave->txMap().updateGiveItem(std::move(item), true, true))
                    LogicError("Ledger::rawReplace: key not found");
			}
		}
	}
    JLOG(journal_.info()) << "update tx map time used:" << utcTime() - timeStart << "ms";

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

        auto const lastVal = app_.getLedgerMaster().getValidatedLedger();
        boost::optional<Rules> rules;
        rules.emplace(*lastVal, app_.config().features);

        app_.openLedger().accept(*rules, ledgerToSave);
    }

    //app_.getLedgerMaster().setClosedLedger(ledgerToSave);
}


void Lookup::relayTxs()
{
	std::lock_guard <decltype(mTransactionsMutex)> lock(mTransactionsMutex);

	std::vector< std::shared_ptr<Transaction> > txs;

    auto hTxSet = app_.getTxPool().topTransactions(std::numeric_limits<std::uint64_t>::max());
	app_.getTxPool().getTransactions(hTxSet, txs);

    std::map < unsigned int, std::vector<std::shared_ptr<Transaction>> > mapShardIndexTxs;

    // tx shard
	for (auto tx : txs)
    {
		auto txCur = tx->getSTransaction();
		auto account = txCur->getAccountID(sfAccount);
		std::string strAccountID = toBase58(account);
		auto shardIndex			 = getTxShardIndex(strAccountID, mShardManager.shardCount());

        mapShardIndexTxs[shardIndex].push_back(tx);
	}

    // send to shard
    for (auto it : mapShardIndexTxs)
    {
        protocol::TMTransactions ts;
        sha512_half_hasher signHash;

        ts.set_nodepubkey(app_.getValidationPublicKey().data(),
            app_.getValidationPublicKey().size());

        for (auto tx : it.second)
        {
            protocol::TMTransaction& t = *ts.add_transactions();
            Serializer s;
            tx->getSTransaction()->add(s);
            t.set_rawtransaction(s.data(), s.size());
            t.set_status(protocol::tsCURRENT);
            t.set_receivetimestamp(app_.timeKeeper().now().time_since_epoch().count());

            hash_append(signHash, tx->getID());
        }

        auto sign = signDigest(app_.getValidationPublicKey(),
            app_.getValidationSecretKey(),
            static_cast<typename sha512_half_hasher::result_type>(signHash));
        ts.set_signature(sign.data(), sign.size());

        auto const m = std::make_shared<Message>(
            ts, protocol::mtTRANSACTIONS);

        mShardManager.node().sendMessage(it.first, m);
    }

	for (auto delHash : hTxSet) {
		app_.getTxPool().removeTx(delHash);
	}
}

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

	bool valid = microWithMeta->checkValidity(
        mShardManager.node().shardValidators()[microWithMeta->shardID()],
		true);
	if (!valid)
	{
        JLOG(journal_.info()) << "Microledger signature verification failed";
		return;
	}

    saveMicroLedger(microWithMeta);
	checkSaveLedger();
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
	checkSaveLedger();
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


void Lookup::setTimer()
{
	mTimer.expires_from_now(ML_RELAYTXS_TIMEOUT);
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


}
