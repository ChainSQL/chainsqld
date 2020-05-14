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
    : mShardManager(m)
    , app_(app)
    , journal_(journal)
    , cfg_(cfg)
    , mTimer(app_.getIOService())
{
    // TODO


	mValidators = std::make_unique<ValidatorList>(
		app_.validatorManifests(), app_.publisherManifests(), app_.timeKeeper(),
		journal_, cfg_.VALIDATION_QUORUM);



	std::vector<std::string> & committeeValidators = cfg_.COMMITTEE_VALIDATORS;
	std::vector<std::string>  publisherKeys;
	// Setup trusted validators
	if (!mValidators->load(
		app_.getValidationPublicKey(),
		committeeValidators,
		publisherKeys,
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

void Committee::onConsensusStart(LedgerIndex seq, uint64 view, PublicKey const pubkey)
{
    //mMicroLedger.reset();

    auto const& validators = mValidators->validators();
    assert(validators.size() > 0);
    int index = (view + seq) % validators.size();

    mIsLeader = (pubkey == validators[index]);

    mFinalLedger.reset();

    {
        std::lock_guard<std::recursive_mutex> lock(mSignsMutex);
        mSignatureBuffer.erase(seq - 1);
    }

    mValidators->onConsensusStart(
        app_.getValidations().getCurrentPublicKeys());

    commitMicroLedgerBuffer(seq);
}

void Committee::commitMicroLedgerBuffer(LedgerIndex seq)
{
    std::lock_guard<std::recursive_mutex> _(mMLBMutex);

    mValidMicroLedgers.clear();

    mMicroLedgerBuffer.erase(seq - 1);

    auto& microLedgersAtSeq = mMicroLedgerBuffer[seq];
    for (auto it : microLedgersAtSeq)
    {
        mValidMicroLedgers.emplace(it.second->shardID(), it.second);
    }
}

uint256 Committee::microLedgerSetHash()
{
    using beast::hash_append;

    std::lock_guard<std::recursive_mutex> _(mMLBMutex);

    assert(mValidMicroLedgers.size() == mShardManager.shardCount());

    bool emptyLedger = true;
    sha512_half_hasher microLedgerSetHash;

    for (auto it : mValidMicroLedgers)
    {
        if (!it.second->isEmptyLedger())
        {
            emptyLedger = false;
        }
        hash_append(microLedgerSetHash, it.second->ledgerHash());
    }

    if (emptyLedger)
    {
        return zero;
    }

    return static_cast<typename sha512_half_hasher::result_type>(microLedgerSetHash);
}

inline bool Committee::microLedgersAllReady()
{
    std::lock_guard<std::recursive_mutex> _(mMLBMutex);
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

boost::optional<uint256> Committee::acquireMicroLedgerSet()
{
    if (microLedgersAllReady())
    {
        return microLedgerSetHash();
    }

    // Start timer. TODO: Adjust repeat counts.
    setTimer(12);

    return boost::none;
}

void Committee::setTimer(uint32 repeats)
{
    bool shouldAcquire = !microLedgersAllReady();

    app_.getJobQueue().addJob(
        jtML_ACQUIRE, "Send->MicroLedgerAcquire",
        [&](Job&)
    {
        if (shouldAcquire)
        {
            protocol::TMMicroLedgerAcquire m;
            m.set_ledgerindex(app_.getLedgerMaster().getValidLedgerIndex() + 1);
            m.set_shardid(firstMissingMicroLedger());

            m.set_nodepubkey(app_.getValidationPublicKey().data(),
                app_.getValidationPublicKey().size());

            auto signingHash = sha512Half(
                m.ledgerindex(),
                m.shardid());

            auto sign = signDigest(app_.getValidationPublicKey(),
                app_.getValidationSecretKey(),
                signingHash);

            m.set_signature(sign.data(), sign.size());

            auto const sm = std::make_shared<Message>(
                m, protocol::mtMICROLEDGER_ACQUIRE);
            {
                std::lock_guard<std::recursive_mutex> _(mPeersMutex);
                if (auto p = mPeers[rand_int((size_t)0, mPeers.size())].lock())
                {
                    p->send(sm);
                }
            }
        }
        else
        {
            app_.getOPs().getConsensus().gotMicroLedgerSet(
                app_.timeKeeper().closeTime(),
                microLedgerSetHash());
        }
    });

    if (--repeats > 0 && shouldAcquire)
    {
        mTimer.expires_from_now(ML_ACQUIRE_TIMEOUT);
        mTimer.async_wait(
            [&](boost::system::error_code const& ec)
        {
            if (ec == boost::asio::error::operation_aborted)
                return;

            setTimer(repeats);
        });
    }
}

auto Committee::canonicalMicroLedgers()
    ->std::vector<std::shared_ptr<MicroLedger const>> const
{
    std::vector<std::shared_ptr<MicroLedger const>> v;
    for (auto it : mValidMicroLedgers)
    {
        v.push_back(it.second);
    }

    return std::move(v);
}

void Committee::buildFinalLedger(OpenView const& view, std::shared_ptr<Ledger const> ledger)
{
    mFinalLedger.emplace(view, ledger, canonicalMicroLedgers());

    commitSignatureBuffer();
}

void Committee::commitSignatureBuffer()
{
    assert(mFinalLedger);

    std::lock_guard<std::recursive_mutex> lock_(mSignsMutex);

    auto iter = mSignatureBuffer.find(mFinalLedger->seq());
    if (iter != mSignatureBuffer.end())
    {
        for (auto it = iter->second.begin(); it != iter->second.end(); ++it)
        {
            if (std::get<0>(*it) == mFinalLedger->ledgerHash())
            {
                mFinalLedger->addSignature(std::get<1>(*it), std::get<2>(*it));
            }
        }
    }
}

void Committee::recvValidation(PublicKey& pubKey, STValidation& val)
{
    LedgerIndex seq = val.getFieldU32(sfLedgerSequence);
    uint256 finalLedgerHash = val.getFieldH256(sfFinalLedgerHash);

    if (seq <= app_.getLedgerMaster().getValidLedgerIndex())
    {
        JLOG(journal_.warn()) << "Validation for ledger seq(seq) from "
            << toBase58(TokenType::TOKEN_NODE_PUBLIC, pubKey) << " is stale";
        return;
    }

    if (mFinalLedger)
    {
        if (mFinalLedger->seq() == seq &&
            mFinalLedger->ledgerHash() == finalLedgerHash)
        {
            std::lock_guard<std::recursive_mutex> lock(mSignsMutex);

            mFinalLedger->addSignature(pubKey, val.getFieldVL(sfFinalLedgerSign));
            return;
        }
    }

    // Buffer it
    {
        std::lock_guard<std::recursive_mutex> lock(mSignsMutex);

        if (mSignatureBuffer.find(seq) != mSignatureBuffer.end())
        {
            mSignatureBuffer[seq].push_back(std::make_tuple(finalLedgerHash, pubKey, val.getFieldVL(sfFinalLedgerSign)));
        }
        else
        {
            std::vector<std::tuple<uint256, PublicKey, Blob>> v;
            v.push_back(std::make_tuple(finalLedgerHash, pubKey, val.getFieldVL(sfFinalLedgerSign)));
            mSignatureBuffer.emplace(seq, std::move(v));
        }
    }
}

bool Committee::checkAccept()
{
    size_t signCount = 0;

    if (mFinalLedger)
    {
        if (mFinalLedger->seq() == app_.getLedgerMaster().getBuildingLedger())
        {
            return false;
        }

        std::lock_guard<std::recursive_mutex> lock(mSignsMutex);
        signCount = mFinalLedger->signatures().size();
    }

    if (signCount >= mValidators->quorum())
    {
        JLOG(journal_.info())
            << "Advancing accepted ledger to " << mFinalLedger->seq()
            << " with >= " << mValidators->quorum() << " validations";

        submitFinalLedger();

        if (app_.getLedgerMaster().getClosedLedger()->seq() == mFinalLedger->seq())
        {
            app_.getOPs().endConsensus();
        }

        return true;
    }

    JLOG(journal_.info()) << "Only " << signCount << " validations";

    return false;
}

void Committee::submitFinalLedger()
{
    protocol::TMFinalLedgerSubmit ms;

    if (!app_.getHashRouter().shouldRelay(mFinalLedger->ledgerHash()))
    {
        JLOG(journal_.info()) << "Repeat submit finalledger, suppressed";
        return;
    }

    mFinalLedger->compose(ms);

    auto const m = std::make_shared<Message>(
        ms, protocol::mtFINALLEDGER_SUBMIT);

    mShardManager.node().sendMessageToAllShard(m);
    mShardManager.lookup().sendMessage(m);
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
    std::lock_guard<std::recursive_mutex> lock(mPeersMutex);

    for (auto w : mPeers)
    {
        if (auto p = w.lock())
        {
            if (!toSkip || toSkip.get().find(p->id()) == toSkip.get().end())
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

void Committee::onMessage(std::shared_ptr<protocol::TMMicroLedgerSubmit> const& m)
{
    std::shared_ptr<MicroLedger> microLedger = std::make_shared<MicroLedger>(*m, false);

    uint32 shardID = microLedger->shardID();

    {
        std::lock_guard<std::recursive_mutex> _(mMLBMutex);

        if (mValidMicroLedgers.find(shardID) != mValidMicroLedgers.end())
        {
            JLOG(journal_.info()) << "Duplicate microledger received for shard " << shardID;
            return;
        }
    }

    LedgerIndex seq = microLedger->seq();
    LedgerIndex curSeq = app_.getLedgerMaster().getValidLedgerIndex() + 1;
    if (seq < curSeq)
    {
        JLOG(journal_.info()) << "This microledger submission is too late";
        return;
    }

    if (mShardManager.node().shardValidators().find(shardID) ==
        mShardManager.node().shardValidators().end()
        || !microLedger->checkValidity(mShardManager.node().shardValidators()[shardID], false))
    {
        JLOG(journal_.info()) << "Microledger signature verification failed";
        return;
    }

    if (seq == curSeq)
    {
        std::lock_guard<std::recursive_mutex> _(mMLBMutex);

        JLOG(journal_.info()) << "Recved valid microledger seq:" << microLedger->seq()
            << " shardID:" << microLedger->shardID();

        mValidMicroLedgers.emplace(shardID, microLedger);
        return;
    }

    JLOG(journal_.info()) << "This microledger submission is advancing, buffer it";

    // seq big than curSeq, buffer it.
    {
        std::lock_guard<std::recursive_mutex> _(mMLBMutex);

        // ledgerHash used to unique
        uint256 ledgerHash = microLedger->ledgerHash();

        if (mMicroLedgerBuffer.find(seq) != mMicroLedgerBuffer.end())
        {
            mMicroLedgerBuffer[seq].emplace(ledgerHash, microLedger);
        }
        else
        {
            std::unordered_map<uint256, std::shared_ptr<MicroLedger>> map;
            map.emplace(ledgerHash, microLedger);
            mMicroLedgerBuffer.emplace(seq, std::move(map));
        }
    }
}

void Committee::onMessage(std::shared_ptr<protocol::TMMicroLedgerAcquire> const& m, std::weak_ptr<PeerImp> weak)
{
    LedgerIndex seq = m->ledgerindex();
    if (seq != app_.getLedgerMaster().getValidLedgerIndex() + 1)
    {
        JLOG(journal_.info()) << "Recv micro ledger acquire, acquired seq(seq), but we are on "
            << app_.getLedgerMaster().getValidLedgerIndex() + 1;
        return;
    }

    PublicKey const publicKey(makeSlice(m->nodepubkey()));
    if (!mValidators->trusted(publicKey))
    {
        JLOG(journal_.info()) << "Recv untrusted peer micro ledger acquire";
        return;
    }

    uint32 shardID = m->shardid();
    if (mValidMicroLedgers.find(shardID) == mValidMicroLedgers.end())
    {
        JLOG(journal_.info()) << "Recv micro ledger acquire, acquire shardID(shardID), missing too";
        return;
    }

    auto signingHash = sha512Half(seq, shardID);
    if (!verifyDigest(publicKey, signingHash, makeSlice(m->signature()), false))
    {
        JLOG(journal_.warn()) << "Recv micro ledger acquire fails sig check";
        return;
    }

    protocol::TMMicroLedgerSubmit ms;

    mValidMicroLedgers.find(shardID)->second->compose(ms, false);

    if (auto peer = weak.lock())
    {
        peer->send(std::make_shared<Message>(ms, protocol::mtMICROLEDGER_SUBMIT));
    }
}

}
