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

#include <peersafe/app/shard/Committee.h>
#include <peersafe/app/shard/ShardManager.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/consensus/RCLConsensus.h>
#include <ripple/app/consensus/RCLValidations.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/basics/random.h>
#include <ripple/overlay/impl/TrafficCount.h>

#include <ripple/core/Config.h>
#include <ripple/core/ConfigSections.h>

#include <ripple/overlay/Peer.h>
#include <ripple/overlay/impl/PeerImp.h>

namespace ripple {

// Timeout interval in milliseconds
auto constexpr ML_ACQUIRE_TIMEOUT = 250ms;


Committee::Committee(ShardManager& m, Application& app, Config& cfg, beast::Journal journal)
    : NodeBase(m, app, cfg, journal)
    , mCachedMLs(std::chrono::seconds(60), stopwatch())
    , mTimer(app_.getIOService())
{
	mValidators = std::make_unique<ValidatorList>(
		app_.validatorManifests(), app_.publisherManifests(), app_.timeKeeper(),
		journal_, cfg_.VALIDATION_QUORUM);

	// Setup validators
	if (!mValidators->load(
		app_.getValidationPublicKey(),
        cfg_.COMMITTEE_VALIDATORS,
        cfg_.section(SECTION_VALIDATOR_LIST_KEYS).values(),
        mShardManager.myShardRole() == ShardManager::COMMITTEE))
	{
        Throw<std::runtime_error>("Committtee validators load failed");
	}
}

void Committee::addActive(std::shared_ptr<PeerImp> const& peer)
{
	std::lock_guard <decltype(mPeersMutex)> lock(mPeersMutex);

	mPeers.emplace_back(std::move(peer));
}

void Committee::eraseDeactivate()
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

bool Committee::isLeader(PublicKey const& pubkey, LedgerIndex curSeq, uint64 view)
{
    auto const& validators = mValidators->validators();
    assert(validators.size());
    int index = (view + curSeq) % validators.size();

    return pubkey == validators[index];
}

std::size_t Committee::quorum()
{
    return mValidators->quorum();
}

std::int32_t Committee::getPubkeyIndex(PublicKey const& pubkey)
{
    auto const& validators = mValidators->validators();

    for (std::int32_t idx = 0; idx < validators.size(); idx++)
    {
        if (validators[idx] == pubkey)
        {
            return idx;
        }
    }

    return -1;
}

void Committee::onViewChange(
    ViewChangeManager& vcManager,
    uint64 view,
    LedgerIndex preSeq,
    LedgerHash preHash)
{
    mCachedMLs.clear();

    std::shared_ptr<protocol::TMCommitteeViewChange> const& sm =
        vcManager.makeCommitteeViewChange(view, preSeq, preHash);

    auto const m = std::make_shared<Message>(
        *sm, protocol::mtCOMMITTEEVIEWCHANGE);

    mShardManager.node().distributeMessage(m);
    mShardManager.lookup().distributeMessage(m);

    mShardManager.checkValidatorLists();
}

void Committee::onConsensusStart(LedgerIndex seq, uint64 view, PublicKey const pubkey)
{
    {
        std::lock_guard<std::recursive_mutex> _(mMLBMutex);
        mValidMicroLedgers.clear();
    }

    mIsLeader = isLeader(pubkey, seq, view);
    mPreSeq = seq - 1;

    mSubmitCompleted = false;
    mFinalLedger.reset();

    mMicroLedger.reset();

    mAcquiring.reset();
    mAcquireMap.clear();

    commitMicroLedgerBuffer(seq);
}

void Committee::commitMicroLedgerBuffer(LedgerIndex seq)
{
    std::lock_guard<std::recursive_mutex> _(mMLBMutex);

    mCachedMLs.for_each(
        [this, seq](std::shared_ptr<MicroLedger> &microLedger) {
            if (microLedger->seq() == seq)
            {
                this->mValidMicroLedgers.emplace(microLedger->shardID(), microLedger);
            }
        });
}

std::pair<uint256, bool> Committee::microLedgerSetHash()
{
    using beast::hash_append;

    std::lock_guard<std::recursive_mutex> _(mMLBMutex);

    assert(microLedgersAllReady());

    bool emptyLedger = true;
    sha512_half_hasher microLedgerSetHash;

    for (auto it : mValidMicroLedgers)
    {
        // exclude committee microledger
        if (it.first)
        {
            if (!it.second->isEmptyLedger())
            {
                emptyLedger = false;
            }

            hash_append(microLedgerSetHash, it.second->ledgerHash());
        }
    }

    return std::make_pair(
        static_cast<typename sha512_half_hasher::result_type>(microLedgerSetHash),
        emptyLedger);
}

bool Committee::microLedgersAllReady()
{
    std::lock_guard<std::recursive_mutex> _(mMLBMutex);
    if (mValidMicroLedgers.count(0))
    {
        // exclude committee microledger
        return mValidMicroLedgers.size() - 1 == mShardManager.shardCount();
    }
    return mValidMicroLedgers.size() == mShardManager.shardCount();
}

uint32 Committee::firstMissingMicroLedger()
{
    std::lock_guard<std::recursive_mutex> _(mMLBMutex);

    for (uint32 shardID = 1; shardID <= mShardManager.shardCount(); shardID++)
    {
        if (mValidMicroLedgers.find(shardID) == mValidMicroLedgers.end())
        {
            return shardID;
        }
    }

    return std::numeric_limits<std::uint32_t>::max();
}

boost::optional<uint256> Committee::acquireMicroLedgerSet(uint256 setID)
{
    if (microLedgersAllReady() && microLedgerSetHash().first == setID)
    {
        return setID;
    }

    setAcquiring(setID);

    app_.getJobQueue().addJob(
        jtML_ACQUIRE, "Send->MicroLedgerAcquire",
        [this, setID](Job&)
    {
        trigger(setID);
    });

    return boost::none;
}

std::size_t Committee::selectPeers(
    std::set<std::shared_ptr<Peer>>& set,
    std::size_t limit,
    std::function<bool(std::shared_ptr<Peer> const&)> score)
{
    using item = std::pair<int, std::shared_ptr<PeerImp>>;
    std::vector<item> v;
    {
        std::lock_guard<std::recursive_mutex> _(mPeersMutex);
        v.reserve(mPeers.size());
    }

    for_each([&](std::shared_ptr<PeerImp>&& e)
    {
        auto const s = e->getScore(score(e));
        v.emplace_back(s, std::move(e));
    });

    std::sort(v.begin(), v.end(),
        [](item const& lhs, item const&rhs)
    {
        return lhs.first > rhs.first;
    });

    std::size_t accepted = 0;
    for (auto const& e : v)
    {
        if (set.insert(e.second).second && ++accepted >= limit)
            break;
    }
    return accepted;
}

void Committee::trigger(uint256 setID)
{
    if (microLedgersAllReady() && microLedgerSetHash().first == setID)
    {
        app_.getOPs().getConsensus().gotMicroLedgerSet(
            app_.timeKeeper().closeTime(),
            setID);
        return;
    }

    protocol::TMMicroLedgerAcquire m;
    m.set_ledgerindex(mPreSeq + 1);
    m.set_setid(setID.data(), setID.size());

    m.set_nodepubkey(app_.getValidationPublicKey().data(),
        app_.getValidationPublicKey().size());

    auto signingHash = sha512Half(
        m.ledgerindex(),
        setID);

    auto sign = signDigest(app_.getValidationPublicKey(),
        app_.getValidationSecretKey(),
        signingHash);

    m.set_signature(sign.data(), sign.size());

    if (hasAcquireMap())
    {
        m.set_shardid(firstMissingMicroLedger());
    }

    auto const sm = std::make_shared<Message>(m, protocol::mtMICROLEDGER_ACQUIRE);
    std::set<std::shared_ptr<Peer>> peers;
    selectPeers(peers, 1, ScoreHasTxSet(setID));
    for (auto const& p : peers)
    {
        JLOG(journal_.info()) << "send "
            << TrafficCount::getName(static_cast<TrafficCount::category>(sm->getCategory()))
            << " to committee[" << p->getShardRole() << ":" << p->getShardIndex()
            << ":" << p->getRemoteAddress() << "]";
        p->send(sm);
    }
}

auto Committee::canonicalMicroLedgers()
    ->std::vector<std::shared_ptr<MicroLedger const>> const
{
    std::vector<std::shared_ptr<MicroLedger const>> v;
    for (auto it : mValidMicroLedgers)
    {
        if (it.first)
        {
            // exclude committee microledger
            v.push_back(it.second);
        }
    }

    if (mValidMicroLedgers.count(0))
    {
        v.push_back(mValidMicroLedgers[0]);
    }

    return std::move(v);
}

MicroLedger const&
Committee::buildMicroLedger(OpenView const& view, std::shared_ptr<CanonicalTXSet const> txSet)
{
    auto microLedger = std::make_shared<MicroLedger>(
        app_.getOPs().getConsensus().getView(),
        0,
        mShardManager.shardCount(),
        view.info().seq,
        view,
        txSet);

    mMicroLedger = *microLedger;

    mValidMicroLedgers.emplace(0, microLedger);

    return *mMicroLedger;
}

void Committee::buildFinalLedger(OpenView const& view, std::shared_ptr<Ledger const> ledger)
{
    mFinalLedger.emplace(view, ledger, canonicalMicroLedgers());
}

bool Committee::checkAccept()
{
    if (!mFinalLedger)
    {
        JLOG(journal_.info()) << "checkAccept: finalledger hasn't built yet";
        return false;
    }

    if (mFinalLedger->seq() == app_.getLedgerMaster().getBuildingLedger())
    {
        JLOG(journal_.info()) << "checkAccept: finalledger " << mFinalLedger->hash() << " building";
        return false;
    }

    size_t signCount = app_.getValidations().numTrustedForLedger(mFinalLedger->hash());

    if (signCount >= mValidators->quorum())
    {
        JLOG(journal_.info())
            << "Advancing accepted ledger to " << mFinalLedger->seq()
            << " with >= " << mValidators->quorum() << " validations";

        if (submitFinalLedger() &&
            app_.getLedgerMaster().getClosedLedger()->seq() == mFinalLedger->seq())
        {
            app_.getTxPool().removeTxs(mMicroLedger->txHashes(), mFinalLedger->seq(), mFinalLedger->parentHash());
            app_.getPreTxPool().removeTxs(mMicroLedger->txHashes(), mFinalLedger->seq(), mFinalLedger->parentHash());

            app_.getOPs().endConsensus();
        }

        return true;
    }

    JLOG(journal_.info()) << "Only " << signCount << " validations";

    return false;
}

bool Committee::submitFinalLedger()
{
    if (!app_.getHashRouter().shouldRelay(mFinalLedger->ledgerHash()))
    {
        JLOG(journal_.info()) << "Repeat submit finalledger, suppressed";
        return false;
    }

    auto const& vals = app_.getValidations().getTrustedForLedger2(mFinalLedger->hash());
    for (auto const& it : vals)
    {
        mFinalLedger->addSignature(it.first, it.second->getFieldVL(sfFinalLedgerSign));
        mMicroLedger->addSignature(it.first, it.second->getFieldVL(sfMicroLedgerSign));
    }

    {
        protocol::TMFinalLedgerSubmit ms;

        mFinalLedger->compose(ms);

        auto const m = std::make_shared<Message>(
            ms, protocol::mtFINALLEDGER_SUBMIT);

        mShardManager.node().distributeMessage(m);
        mShardManager.lookup().distributeMessage(m);
    }

    {
        protocol::TMMicroLedgerSubmit ms;

        mMicroLedger->compose(ms, true);

        auto const m = std::make_shared<Message>(
            ms, protocol::mtMICROLEDGER_SUBMIT);

        mShardManager.lookup().distributeMessage(m);
    }

    return true;
}

Overlay::PeerSequence Committee::getActivePeers(uint32 /* unused */)
{
    Overlay::PeerSequence ret;

    std::lock_guard<std::recursive_mutex> lock(mPeersMutex);

    ret.reserve(mPeers.size());

    for (auto w : mPeers)
    {
        if (auto p = w.lock())
        {
            ret.emplace_back(std::move(p));
        }
    }

    return ret;
}

void Committee::sendMessage(std::shared_ptr<Message> const &m)
{
    std::lock_guard<std::recursive_mutex> lock(mPeersMutex);

    for (auto w : mPeers)
    {
        if (auto p = w.lock())
        {
            JLOG(journal_.info()) << "sendMessage "
                << TrafficCount::getName(static_cast<TrafficCount::category>(m->getCategory()))
                << " to committee[" << p->getShardRole() << ":" << p->getShardIndex()
                << ":" << p->getRemoteAddress() << "]";
            p->send(m);
        }
    }
}

void Committee::relay(
    boost::optional<std::set<HashRouter::PeerShortID>> toSkip,
    std::shared_ptr<Message> const &m)
{
    assert(toSkip);
    std::lock_guard<std::recursive_mutex> lock(mPeersMutex);

    for (auto w : mPeers)
    {
        if (auto p = w.lock())
        {
            if (toSkip->find(p->id()) == toSkip->end())
            {
                JLOG(journal_.info()) << "relay "
                    << TrafficCount::getName(static_cast<TrafficCount::category>(m->getCategory()))
                    << " to committee[" << p->getShardRole() << ":" << p->getShardIndex()
                    << ":" << p->getRemoteAddress() << "]";
                p->send(m);
            }
        }
    }
}

void Committee::distributeMessage(std::shared_ptr<Message> const &m, bool forceBroadcast)
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

    std::vector<std::shared_ptr<Peer>> committeePeers;
    {
        std::lock_guard<std::recursive_mutex> lock(mPeersMutex);
        for (auto w : mPeers)
        {
            if (auto p = w.lock())
            {
                committeePeers.emplace_back(std::move(p));
            }
        }
    }

    std::sort(committeePeers.begin(), committeePeers.end(),
        [](std::shared_ptr<Peer const> const& p1, std::shared_ptr<Peer const> const& p2) {
        return publicKeyComp(p1->getNodePublic(), p2->getNodePublic());
    });

    auto dstGroupMemberCounts = committeePeers.size() / groupCounts;
    if (committeePeers.size() % groupCounts > 0)
    {
        dstGroupMemberCounts++;
    }

    auto myDstPeersLo = myGroupIndex * dstGroupMemberCounts;
    auto myDstPeersHi = myDstPeersLo + dstGroupMemberCounts;
    if (myDstPeersHi > committeePeers.size())
    {
        myDstPeersHi = committeePeers.size();
    }
    JLOG(journal_.info()) << "myDstPeersLo:" << myDstPeersLo << " myDstPeersHi:" << myDstPeersHi;

    for (auto i = myDstPeersLo; i < myDstPeersHi; i++)
    {
        JLOG(journal_.info()) << "distributeMessage "
            << TrafficCount::getName(static_cast<TrafficCount::category>(m->getCategory()))
            << " to committee[" << committeePeers[i]->getShardRole() << ":" << committeePeers[i]->getShardIndex()
            << ":" << committeePeers[i]->getRemoteAddress() << "]";
        committeePeers[i]->send(m);
    }
}

void Committee::onMessage(std::shared_ptr<protocol::TMMicroLedgerSubmit> const& m)
{
    std::shared_ptr<MicroLedger> microLedger = std::make_shared<MicroLedger>(*m, false);

    if (!app_.getHashRouter().shouldRelay(microLedger->ledgerHash()))
    {
        JLOG(journal_.info()) << "MicroLeger: duplicate";
        return;
    }

    uint32 shardID = microLedger->shardID();
    LedgerIndex seq = microLedger->seq();
    LedgerIndex curSeq = mPreSeq + 1;
    if (seq < curSeq)
    {
        JLOG(journal_.info()) << "This microledger submission is too late";
        return;
    }

    if (microLedger->shardCount() != mShardManager.shardCount())
    {
        JLOG(journal_.info()) << "Shard count is " << mShardManager.shardCount() <<
            " now, But " << microLedger->shardCount() << " in shard " << shardID;
        return;
    }

    if (mShardManager.node().shardValidators().find(shardID) ==
        mShardManager.node().shardValidators().end()
        || !microLedger->checkValidity(mShardManager.node().shardValidators()[shardID], false))
    {
        JLOG(journal_.info()) << "Microledger signature verification failed";
        return;
    }

    JLOG(journal_.info()) << "Recved valid microledger"
        << "(contains " << microLedger->txCounts() << "txs)"
        << " seq:" << microLedger->seq()
        << " shardID:" << microLedger->shardID();

    if (seq == curSeq)
    {
        std::lock_guard<std::recursive_mutex> _(mMLBMutex);
        auto setID = mAcquiring;
        if (setID && mAcquireMap.size())
        {
            if (microLedger->ledgerHash() == mAcquireMap[shardID])
            {
                mValidMicroLedgers.emplace(shardID, microLedger);
                app_.getJobQueue().addJob(
                    jtML_ACQUIRE, "Send->MicroLedgerAcquire",
                    [this, setID](Job&)
                {
                    trigger(*setID);
                });
            }
            else
            {
                JLOG(journal_.info()) << "Discard this microledger, because finalLedger will don't contain it";
            }
            return;
        }
        else
        {
            if (mValidMicroLedgers.emplace(shardID, microLedger).second)
            {
                return;
            }
            else
            {
                JLOG(journal_.info()) << "Duplicate microledger received for shard " << shardID << ", buffer it";
            }
        }
    }

    if (seq > curSeq)
    {
        JLOG(journal_.info()) << "This microledger submission is advancing, buffer it";
    }

    // seq big than curSeq or duplicate microledger, buffer it.
    mCachedMLs.emplace(microLedger->ledgerHash(), microLedger);
}

void Committee::onMessage(std::shared_ptr<protocol::TMMicroLedgerAcquire> const& m, std::weak_ptr<PeerImp> weak)
{
    LedgerIndex seq = m->ledgerindex();
    if (seq != mPreSeq + 1)
    {
        JLOG(journal_.info()) << "Recv micro ledger acquire, acquired seq(" << seq <<
            "), but we are on " << mPreSeq + 1;
        return;
    }

    uint256 setID;
    memcpy(setID.begin(), m->setid().data(), 32);
    if (!microLedgersAllReady() || setID != microLedgerSetHash().first)
    {
        JLOG(journal_.info()) << "Recv micro ledger acquire, acquired setID missing too or mismatch";
        return;
    }

    PublicKey const publicKey(makeSlice(m->nodepubkey()));
    if (!mValidators->trusted(publicKey))
    {
        JLOG(journal_.info()) << "Recv untrusted peer micro ledger acquire";
        return;
    }

    auto signingHash = sha512Half(seq, setID);
    if (!verifyDigest(publicKey, signingHash, makeSlice(m->signature()), false))
    {
        JLOG(journal_.warn()) << "Recv micro ledger acquire fails sig check";
        return;
    }

    std::shared_ptr<Message> msg;

    if (m->has_shardid())
    {
        uint32 shardID = m->shardid();
        if (mValidMicroLedgers.find(shardID) == mValidMicroLedgers.end())
        {
            JLOG(journal_.info()) << "Recv micro ledger acquire, acquire shardID(" << shardID << "), missing too";
            return;
        }

        protocol::TMMicroLedgerSubmit ms;
        mValidMicroLedgers[shardID]->compose(ms, false);
        msg = std::make_shared<Message>(ms, protocol::mtMICROLEDGER_SUBMIT);
    }
    else
    {
        protocol::TMMicroLedgerInfos mi;

        for (auto const& it : mValidMicroLedgers)
        {
            protocol::MLInfo& mInfo = *mi.add_microledgers();
            mInfo.set_shardid(it.first);
            mInfo.set_mlhash(it.second->ledgerHash().data(), it.second->ledgerHash().size());
        }

        mi.set_nodepubkey(app_.getValidationPublicKey().data(),
            app_.getValidationPublicKey().size());
        auto sign = signDigest(app_.getValidationPublicKey(),
            app_.getValidationSecretKey(),
            setID);
        mi.set_signature(sign.data(), sign.size());
        msg = std::make_shared<Message>(mi, protocol::mtMICROLEDGER_INFOS);
    }

    if (auto p = weak.lock())
    {
        JLOG(journal_.info()) << "send "
            << TrafficCount::getName(static_cast<TrafficCount::category>(msg->getCategory()))
            << " to committee[" << p->getShardRole() << ":" << p->getShardIndex()
            << ":" << p->getRemoteAddress() << "]";
        p->send(msg);
    }
}

void Committee::onMessage(std::shared_ptr<protocol::TMMicroLedgerInfos> const& m)
{
    if (!getAcquiring())
    {
        JLOG(journal_.info()) << "Recv microledger_infos, but I'm not acquiring";
        return;
    }

    std::map<uint32, uint256> microLedgerInfos;
    for (int i = 0; i < m->microledgers().size(); i++)
    {
        protocol::MLInfo const& mbInfo = m->microledgers(i);
        ripple::LedgerHash mbHash;
        memcpy(mbHash.begin(), mbInfo.mlhash().data(), 32);
        microLedgerInfos[mbInfo.shardid()] = mbHash;
    }

    using beast::hash_append;
    uint256 setID = zero;
    if (microLedgerInfos.size() > 0)
    {
        sha512_half_hasher h;
        for (auto const& it : microLedgerInfos)
        {
            hash_append(h, it.second);
        }
        setID = static_cast<typename sha512_half_hasher::result_type>(h);
    }

    if (!app_.getHashRouter().shouldRelay(setID))
    {
        JLOG(journal_.info()) << "MicroLegerInfos: duplicate";
        return;
    }

    PublicKey const publicKey(makeSlice(m->nodepubkey()));
    if (!mValidators->trusted(publicKey))
    {
        JLOG(journal_.info()) << "Recv untrusted peer microledger_infos";
        return;
    }

    if (!verifyDigest(publicKey, setID, makeSlice(m->signature()), false))
    {
        JLOG(journal_.warn()) << "Recv microledger_infos fails sig check";
        return;
    }

    if (!getAcquiring() || getAcquiring().get() != setID)
    {
        JLOG(journal_.info()) << "Recv microledger_infos, mismatch";
        return;
    }

    {
        std::lock_guard<std::recursive_mutex> _(mMLBMutex);
        for (auto const& it : microLedgerInfos)
        {
            mAcquireMap[it.first] = it.second;
            if (mValidMicroLedgers.find(it.first) != mValidMicroLedgers.end() &&
                mValidMicroLedgers[it.first]->ledgerHash() != it.second)
            {
                // erase wrong
                mValidMicroLedgers.erase(it.first);
            }
            if (mValidMicroLedgers.find(it.first) == mValidMicroLedgers.end())
            {
                auto microLedger = mCachedMLs.fetch(it.second);
                if (microLedger && microLedger->shardID() == it.first)
                {
                    mValidMicroLedgers.emplace(it.first, microLedger);
                }
            }
        }
    }

    app_.getJobQueue().addJob(
        jtML_ACQUIRE, "Send->MicroLedgerAcquire",
        [this, setID](Job&)
    {
        trigger(setID);
    });
}

}
