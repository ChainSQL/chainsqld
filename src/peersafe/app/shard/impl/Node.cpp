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

#include <peersafe/app/shard/Node.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/app/misc/HashRouter.h>
#include <ripple/app/misc/LoadFeeTrack.h>
#include <ripple/app/misc/AmendmentTable.h>
#include <ripple/app/consensus/RCLConsensus.h>
#include <ripple/app/consensus/RCLValidations.h>
#include <peersafe/app/shard/FinalLedger.h>
#include <peersafe/app/shard/ShardManager.h>
#include <peersafe/app/consensus/ViewChange.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/protocol/digest.h>
#include <ripple/overlay/impl/TrafficCount.h>
#include <ripple/basics/make_lock.h>
#include <ripple/basics/random.h>
#include <ripple/app/ledger/OpenLedger.h>

#include <ripple/overlay/Peer.h>
#include <ripple/overlay/impl/PeerImp.h>

namespace ripple {

Node::Node(ShardManager& m, Application& app, Config& cfg, beast::Journal journal)
    : NodeBase(m, app, cfg, journal)
    , mCachedMLs(std::chrono::seconds(60), stopwatch())
{
    mShardID = cfg_.SHARD_INDEX;

	std::vector< std::vector<std::string> >& shardValidators = cfg_.SHARD_VALIDATORS;
	for (size_t i = 0; i < shardValidators.size(); i++)
    {
		// shardIndex = index + 1
		mMapOfShardValidators[i+1] = std::make_unique<ValidatorList>(
			app_.validatorManifests(), app_.publisherManifests(), app_.timeKeeper(),
			journal_, cfg_.VALIDATION_QUORUM);

		// Setup validators
		if (!mMapOfShardValidators[i+1]->load(
			    app_.getValidationPublicKey(),
			    shardValidators[i],
                cfg_.section(SECTION_VALIDATOR_LIST_KEYS).values(),
                (mShardManager.myShardRole() == ShardManager::SHARD && mShardID == i + 1)))
        {
			mMapOfShardValidators.erase(i + 1);
            Throw<std::runtime_error>("Shard validators load failed");
		}
	}
}

void Node::addActive(std::shared_ptr<PeerImp> const& peer)
{
	std::lock_guard <decltype(mPeersMutex)> lock(mPeersMutex);


	std::uint32_t index = peer->getShardIndex();

	auto iter = mMapOfShardPeers.find(index);
	if (iter == mMapOfShardPeers.end()) {

		std::vector<std::weak_ptr <PeerImp>>		peers;
		peers.emplace_back(std::move(peer));

		mMapOfShardPeers.emplace(
			std::piecewise_construct,
			std::make_tuple(index),
			std::make_tuple(peers));
	}
	else {

		iter->second.emplace_back(std::move(peer));
	}


}

void Node::eraseDeactivate(uint32 shardIndex)
{
	std::lock_guard <decltype(mPeersMutex)> lock(mPeersMutex);

    if (mMapOfShardPeers.find(shardIndex) != mMapOfShardPeers.end())
    {
        for (auto w = mMapOfShardPeers[shardIndex].begin(); w != mMapOfShardPeers[shardIndex].end();)
        {
            auto p = w->lock();
            if (!p)
            {
                w = mMapOfShardPeers[shardIndex].erase(w);
            }
            else
            {
                w++;
            }
        }
    }
}

bool Node::isLeader(PublicKey const& pubkey, LedgerIndex curSeq, uint64 view)
{
    if (mMapOfShardValidators.find(mShardID) != mMapOfShardValidators.end())
    {
        auto const& validators = mMapOfShardValidators[mShardID]->validators();
        assert(validators.size());
        int index = (view + curSeq) % validators.size();
        return pubkey == validators[index];
    }

    return false;
}

inline auto Node::validatorsPtr()
	-> std::unique_ptr<ValidatorList>&
{
    if (mMapOfShardValidators.find(mShardID) == mMapOfShardValidators.end())
    {
        mMapOfShardValidators[mShardID] = std::make_unique<ValidatorList>(
            app_.validatorManifests(), app_.publisherManifests(), app_.timeKeeper(),
            journal_, cfg_.VALIDATION_QUORUM);
        mMapOfShardValidators[mShardID]->load(
            app_.getValidationPublicKey(),
            std::vector<std::string>{},
            cfg_.section(SECTION_VALIDATOR_LIST_KEYS).values(),
            (mShardManager.myShardRole() == ShardManager::SHARD));
        mShardManager.checkValidatorLists();
    }

    return mMapOfShardValidators[mShardID];
}

std::size_t Node::quorum()
{
    if (mMapOfShardValidators.find(mShardID) != mMapOfShardValidators.end())
    {
        return mMapOfShardValidators[mShardID]->quorum();
    }

    return std::numeric_limits<std::size_t>::max();
}

std::int32_t Node::getPubkeyIndex(PublicKey const& pubkey)
{
    if (mMapOfShardValidators.find(mShardID) != mMapOfShardValidators.end())
    {
        auto const & validators = mMapOfShardValidators[mShardID]->validators();
        for (std::int32_t idx = 0; idx < validators.size(); idx++)
        {
            if (validators[idx] == pubkey)
            {
                return idx;
            }
        }
    }

    return -1;
}

void Node::onConsensusStart(LedgerIndex seq, uint64 view, PublicKey const pubkey)
{
    mIsLeader = isLeader(pubkey, seq, view);
    mPreSeq = seq - 1;

    if (view == 0)
    {
        mCachedMLs.clear();
    }
}

void Node::doAccept(
    RCLTxSet const& set,
    RCLCxLedger const& previousLedger,
    NetClock::time_point closeTime)
{
    closeTime = std::max<NetClock::time_point>(closeTime, previousLedger.closeTime() + 1s);

    auto buildLCL = std::make_shared<Ledger>(*previousLedger.ledger_, closeTime);

    OpenView accum(&*buildLCL);
    assert(!accum.open());

    // Normal case, we are not replaying a ledger close
    applyTransactions(
        app_, set, accum, [&buildLCL](uint256 const& txID) {
        return !buildLCL->txExists(txID);
    });

    auto microLedger = std::make_shared<MicroLedger>(
        app_.getOPs().getConsensus().getView(),
        mShardID,
        mShardManager.shardCount(),
        accum.info().seq,
        accum);
    app_.getOPs().getConsensus().setPhase(ConsensusPhase::validating);

    JLOG(journal_.info()) << "MicroLedger: " << microLedger->ledgerHash();

    microLedger = mCachedMLs.emplace(microLedger->ledgerHash(), microLedger);

    if (!microLedger)
    {
        JLOG(journal_.warn()) << "Cache(emplace) MicroLedger failed";
        return;
    }

    validate(*microLedger);

    // See if we can submit this micro ledger.
    checkAccept(microLedger->ledgerHash());
}

void Node::validate(MicroLedger const& microLedger)
{
    RCLConsensus::Adaptor& adaptor = app_.getOPs().getConsensus().adaptor_;
    auto validationTime = app_.timeKeeper().closeTime();
    if (validationTime <= adaptor.lastValidationTime_)
        validationTime = adaptor.lastValidationTime_ + 1s;
    adaptor.lastValidationTime_ = validationTime;

    // Build validation
    auto v = std::make_shared<STValidation>(
        microLedger.ledgerHash(), validationTime, adaptor.valPublic_, true);

    v->setFieldU32(sfLedgerSequence, microLedger.seq());
    v->setFieldU32(sfShardID, microLedger.shardID());

    // Add our load fee to the validation
    auto const& feeTrack = app_.getFeeTrack();
    std::uint32_t fee =
        std::max(feeTrack.getLocalFee(), feeTrack.getClusterFee());

    if (fee > feeTrack.getLoadBase())
        v->setFieldU32(sfLoadFee, fee);

    if (((microLedger.seq() + 1) % 256) == 0)
    // next ledger is flag ledger
    {
        // Suggest fee changes and new features
        std::shared_ptr<Ledger const> ledger = app_.getLedgerMaster().getLedgerBySeq(mPreSeq);
        adaptor.feeVote_->doValidation(ledger, *v);
        app_.getAmendmentTable().doValidation(ledger, *v);
    }

    auto const signingHash = v->sign(adaptor.valSecret_);
    v->setTrusted();

    // suppress it if we receive it - FIXME: wrong suppression
    app_.getHashRouter().addSuppression(signingHash);

    handleNewValidation(app_, v, "local");

    Blob validation = v->getSerialized();
    protocol::TMValidation val;
    val.set_validation(&validation[0], validation.size());

    // Send signed validation to all of our directly connected peers
    auto const m = std::make_shared<Message>(
        val, protocol::mtVALIDATION);
    sendMessage(m);
}

void Node::checkAccept(LedgerHash microLedgerHash)
{
    assert(mMapOfShardValidators.find(mShardID) != mMapOfShardValidators.end());

    auto microLedger = mCachedMLs.fetch(microLedgerHash);
    if (!microLedger)
    {
        JLOG(journal_.info()) << "microledger " << microLedgerHash << " hasn't built yet";
        return;
    }

    size_t minVal = mMapOfShardValidators[mShardID]->quorum();
    size_t signCount = app_.getValidations().numTrustedForLedger(microLedgerHash);

    JLOG(journal_.info()) << "MicroLedger sign count: " << signCount << " quorum: " << minVal;

    if (signCount >= minVal)
    {
        submitMicroLedger(microLedger->ledgerHash(), false);

        JLOG(journal_.info()) << "Set consensus phase to waitingFinalLedger";

        app_.getOPs().getConsensus().setPhase(ConsensusPhase::waitingFinalLedger);
    }
}

std::shared_ptr<MicroLedger const> Node::submitMicroLedger(LedgerHash microLedgerHash, bool withTxMeta)
{
    auto microLedger = mCachedMLs.fetch(microLedgerHash);
    if (!microLedger)
    {
        JLOG(journal_.info()) << "Can't Submit microledger " << microLedgerHash
            << ", because I didn't built it";
        return nullptr;
    }

    auto suppressionKey = sha512Half(
        microLedger->ledgerHash(),
        withTxMeta);

    if (!app_.getHashRouter().shouldRelay(suppressionKey))
    {
        JLOG(journal_.info()) << "Repeat submit microledger, suppressed";
        return nullptr;
    }

    JLOG(journal_.info()) << "Submit microledger(seq:" << microLedger->seq() << ") to "
        << (withTxMeta ? "lookup" : "committee");

    if (microLedger->signatures().size() == 0)
    {
        auto const& vals = app_.getValidations().getTrustedForLedger2(microLedgerHash);
        for (auto const& it : vals)
        {
            microLedger->addSignature(it.first, it.second->getFieldVL(sfMicroLedgerSign));
        }
    }

    protocol::TMMicroLedgerSubmit ms;

    microLedger->compose(ms, withTxMeta);

    auto const m = std::make_shared<Message>(
        ms, protocol::mtMICROLEDGER_SUBMIT);

    if (withTxMeta)
    {
        mShardManager.lookup().distributeMessage(m);
    }
    else
    {
        mShardManager.committee().distributeMessage(m);
    }

    return microLedger;
}

// To specified shard
void Node::sendMessage(uint32 shardID, std::shared_ptr<Message> const &m)
{
    std::lock_guard<std::recursive_mutex> _(mPeersMutex);

    if (mMapOfShardPeers.find(shardID) != mMapOfShardPeers.end())
    {
        for (auto w : mMapOfShardPeers[shardID])
        {
            if (auto p = w.lock())
            {
                JLOG(journal_.info()) << "sendMessage "
                    << TrafficCount::getName(static_cast<TrafficCount::category>(m->getCategory()))
                    << " to shard[" << p->getShardRole() << ":" << p->getShardIndex()
                    << ":" << p->getRemoteAddress() << "]";
                p->send(m);
            }
        }
    }
}

// To all shard
void Node::sendMessageToAllShard(std::shared_ptr<Message> const &m)
{
    std::lock_guard<std::recursive_mutex> _(mPeersMutex);

    for (auto const& it : mMapOfShardPeers)
    {
        sendMessage(it.first, m);
    }
}

// To our shard
void Node::sendMessage(std::shared_ptr<Message> const &m)
{
    std::lock_guard<std::recursive_mutex> _(mPeersMutex);

    if (mMapOfShardPeers.find(mShardID) != mMapOfShardPeers.end())
    {
        for (auto w : mMapOfShardPeers[mShardID])
        {
            if (auto p = w.lock())
            {
                JLOG(journal_.info()) << "sendMessage "
                    << TrafficCount::getName(static_cast<TrafficCount::category>(m->getCategory()))
                    << " to our shard[" << p->getShardRole() << ":" << p->getShardIndex()
                    << ":" << p->getRemoteAddress() << "]";
                p->send(m);
            }
        }
    }
}

// To our shard and skip suppression
void Node::relay(
    boost::optional<std::set<HashRouter::PeerShortID>> toSkip,
    std::shared_ptr<Message> const &m)
{
    assert(toSkip);
    std::lock_guard<std::recursive_mutex> _(mPeersMutex);

    if (mMapOfShardPeers.find(mShardID) != mMapOfShardPeers.end())
    {
        for (auto w : mMapOfShardPeers[mShardID])
        {
            if (auto p = w.lock())
            {
                if (toSkip->find(p->id()) == toSkip->end())
                {
                    JLOG(journal_.info()) << "relay "
                        << TrafficCount::getName(static_cast<TrafficCount::category>(m->getCategory()))
                        << " to our shard[" << p->getShardRole() << ":" << p->getShardIndex()
                        << ":" << p->getRemoteAddress() << "]";
                    p->send(m);
                }
            }
        }
    }
}

void Node::distributeMessage(std::shared_ptr<Message> const &m, bool forceBroadcast)
{
    if (forceBroadcast)
    {
        sendMessageToAllShard(m);
        return;
    }

    auto senderCounts = mShardManager.nodeBase().validatorsPtr()->validators().size();
    auto quorum = mShardManager.nodeBase().quorum();
    auto maybeInsane = senderCounts - quorum;
    auto groupMemberCounts = maybeInsane + 1;   // Make sure that each group has at least one sane node.
    auto groupCounts = senderCounts / groupMemberCounts;

    if (groupCounts <= 1)
    {
        sendMessageToAllShard(m);
        return;
    }

    auto myPubkeyIndex = mShardManager.nodeBase().getPubkeyIndex(app_.getValidationPublicKey());
    auto myGroupIndex = myPubkeyIndex != -1 ? myPubkeyIndex / groupMemberCounts : rand_int(groupCounts - 1);
    if (myGroupIndex >= groupCounts)
    {
        myGroupIndex = groupCounts - 1;
    }
    JLOG(journal_.info()) << "myPubkeyIndex:" << myPubkeyIndex << " myGroupIndex:" << myGroupIndex;

    std::vector<std::shared_ptr<Peer>> shardPeers;
    {
        std::lock_guard<std::recursive_mutex> lock(mPeersMutex);
        for (auto const& it : mMapOfShardPeers)
        {
            for (auto w : it.second)
            {
                if (auto p = w.lock())
                {
                    shardPeers.emplace_back(std::move(p));
                }
            }
        }
    }

    std::sort(shardPeers.begin(), shardPeers.end(),
        [](std::shared_ptr<Peer const> const& p1, std::shared_ptr<Peer const> const& p2) {
        return publicKeyComp(p1->getNodePublic(), p2->getNodePublic());
    });

    auto shardGroupMemberCounts = shardPeers.size() / groupCounts;
    if (shardPeers.size() % groupCounts > 0)
    {
        shardGroupMemberCounts++;
    }

    auto myShardPeersLo = myGroupIndex * shardGroupMemberCounts;
    auto myShardPeersHi = myShardPeersLo + shardGroupMemberCounts;
    if (myShardPeersHi > shardPeers.size())
    {
        myShardPeersHi = shardPeers.size();
    }
    JLOG(journal_.info()) << "myDstPeersLo:" << myShardPeersLo << " myDstPeersHi:" << myShardPeersHi;

    for (auto i = myShardPeersLo; i < myShardPeersHi; i++)
    {
        JLOG(journal_.info()) << "distributeMessage "
            << TrafficCount::getName(static_cast<TrafficCount::category>(m->getCategory()))
            << " to shard[" << shardPeers[i]->getShardRole() << ":" << shardPeers[i]->getShardIndex()
            << ":" << shardPeers[i]->getRemoteAddress() << "]";
        shardPeers[i]->send(m);
    }
}

Overlay::PeerSequence Node::getActivePeers(uint32 shardID)
{
    Overlay::PeerSequence ret;

    std::lock_guard<std::recursive_mutex> lock(mPeersMutex);

    if (mMapOfShardPeers.find(shardID) != mMapOfShardPeers.end())
    {
        ret.reserve(mMapOfShardPeers[shardID].size());

        for (auto w : mMapOfShardPeers[shardID])
        {
            if (auto p = w.lock())
            {
                ret.emplace_back(std::move(p));
            }
        }
    }

    return ret;
}

void Node::onMessage(std::shared_ptr<protocol::TMFinalLedgerSubmit> const& m)
{
	auto finalLedger = std::make_shared<FinalLedger>(*m);

    if (!app_.getHashRouter().shouldRelay(finalLedger->ledgerHash()))
    {
        JLOG(journal_.info()) << "FinalLeger: duplicate";
        return;
    }

    if (finalLedger->seq() < mPreSeq + 1)
    {
        JLOG(journal_.info()) << "FinalLeger: stale";
        return;
    }

    if (!finalLedger->checkValidity(mShardManager.committee().validatorsPtr()))
    {
        JLOG(journal_.info()) << "FinalLeger signature verification failed";
        return;
    }

    if (finalLedger->seq() > mPreSeq + 1)
    {
        JLOG(journal_.warn()) << "Got finalLeger, I'm on " << mPreSeq
            << ", trying to switch to " << finalLedger->seq();
        app_.getOPs().consensusViewChange();
        app_.getOPs().getConsensus().consensus_->handleWrongLedger(finalLedger->hash());
        return;
    }

	// build new ledger
	auto ledgerInfo = finalLedger->getLedgerInfo();
    auto buildLCL = std::make_shared<Ledger>(*app_.getLedgerMaster().getLedgerBySeq(mPreSeq), ledgerInfo.closeTime);
	finalLedger->apply(*buildLCL);

    buildLCL->updateAmendments(app_);
    buildLCL->updateSkipList();

    // Write the final version of all modified SHAMap
    // nodes to the node store to preserve the new Ledger
    // Note,didn't save tx-shamap,so don't load tx-shamap when load ledger.
    auto timeStart = utcTime();
    int asf = buildLCL->stateMap().flushDirty(
        hotACCOUNT_NODE, buildLCL->info().seq);
    int tmf = buildLCL->txMap().flushDirty(
        hotTRANSACTION_NODE, buildLCL->info().seq);
    JLOG(journal_.debug()) << "Flushed " << asf << " accounts and " << tmf
        << " transaction nodes";
    JLOG(journal_.info()) << "flushDirty time used:" << utcTime() - timeStart << "ms";

    buildLCL->unshare();
    buildLCL->setAccepted(ledgerInfo.closeTime, ledgerInfo.closeTimeResolution, true, app_.config());

    // check hash
    if (ledgerInfo.accountHash != buildLCL->stateMap().getHash().as_uint256() ||
        ledgerInfo.txHash != buildLCL->txMap().getHash().as_uint256() ||
        ledgerInfo.hash != buildLCL->info().hash)
    {
        JLOG(journal_.warn()) << "Final ledger txs/accounts shamap root hash missmatch";
        return;
    }

    auto microLedger = submitMicroLedger(finalLedger->getMicroLedgerHash(mShardID), true);

    app_.getLedgerMaster().storeLedger(buildLCL);

    app_.getOPs().getConsensus().adaptor_.notify(protocol::neACCEPTED_LEDGER, RCLCxLedger{buildLCL}, true);

    app_.getLedgerMaster().setBuildingLedger(0);
	//save ledger
	app_.getLedgerMaster().accept(buildLCL);

    // Build new open ledger for newRound
    {
        auto lock = make_lock(app_.getMasterMutex(), std::defer_lock);
        auto sl = make_lock(app_.getLedgerMaster().peekMutex(), std::defer_lock);
        std::lock(lock, sl);

        auto const lastVal = app_.getLedgerMaster().getValidatedLedger();
        boost::optional<Rules> rules;
        rules.emplace(*lastVal, app_.config().features);

        app_.openLedger().accept(*rules, buildLCL);
    }

    app_.getLedgerMaster().setClosedLedger(buildLCL);

    if (microLedger)
    {
        app_.getTxPool().removeTxs(microLedger->txHashes(), buildLCL->info().seq, buildLCL->info().parentHash);
    }
    else
    {
        app_.getTxPool().removeTxs(buildLCL->txMap(), buildLCL->info().seq, buildLCL->info().parentHash);
    }

    app_.getLedgerMaster().processHeldTransactions();

	//begin next round consensus
	app_.getOPs().endConsensus();
}

void Node::onMessage(std::shared_ptr<protocol::TMCommitteeViewChange> const& m)
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

    if (committeeVC->preSeq() > mPreSeq)
    {
        JLOG(journal_.warn()) << "Got CommitteeViewChange, I'm on " << mPreSeq
            << ", trying to switch to " << committeeVC->preSeq();
        app_.getOPs().consensusViewChange();
        app_.getOPs().getConsensus().consensus_->handleWrongLedger(committeeVC->preHash());
    }
    else if (committeeVC->preSeq() == mPreSeq)
    {
        mCachedMLs.clear();

        app_.getOPs().getConsensus().onCommitteeViewChange();
    }
    else
    {
        JLOG(journal_.warn()) << "Committee view change: stale";
    }

    mShardManager.checkValidatorLists();

    return;
}


}
