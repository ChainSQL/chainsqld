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

#ifndef PEERSAFE_APP_SHARD_COMMITTEE_H_INCLUDED
#define PEERSAFE_APP_SHARD_COMMITTEE_H_INCLUDED

#include "ripple.pb.h"
#include <peersafe/app/shard/NodeBase.h>
#include <ripple/basics/UnorderedContainers.h>
#include <ripple/beast/utility/Journal.h>
#include <ripple/overlay/impl/PeerImp.h>
#include <ripple/app/misc/ValidatorList.h>
#include <peersafe/app/shard/MicroLedger.h>
#include <peersafe/app/shard/FinalLedger.h>
#include <peersafe/app/consensus/ViewChangeManager.h>

#include <mutex>


namespace ripple {

class PeerImp;
class Peer;

class Committee : public NodeBase {

private:
    // Used if I am a Committee node
    bool                                                    mIsLeader;
    LedgerIndex                                             mPreSeq;
    std::map<uint32,
        std::shared_ptr<MicroLedger>>                       mValidMicroLedgers;     // This round. mapping shardID --> MicroLedger
    CachedMLs                                               mCachedMLs;
    boost::optional<uint256>                                mAcquiring;
    std::map<uint32, uint256>                               mAcquireMap;
    std::recursive_mutex                                    mMLBMutex;              // Micro ledger buffer mutex

    boost::asio::basic_waitable_timer<
        std::chrono::steady_clock>                          mTimer;

    boost::optional<MicroLedger>                            mMicroLedger;

    bool                                                    mSubmitCompleted;
    boost::optional<FinalLedger>                            mFinalLedger;

    // Hold all committee peers
	std::vector<std::weak_ptr <PeerImp>>				mPeers;
    std::recursive_mutex                                mPeersMutex;

    // Hold all committee validators
    std::unique_ptr <ValidatorList>                     mValidators;


public:

    Committee(ShardManager& m, Application& app, Config& cfg, beast::Journal journal);
    ~Committee() {}

    inline ValidatorList& validators()
    {
        return *mValidators;
    }

    inline std::unique_ptr<ValidatorList>& validatorsPtr() override
    {
        return mValidators;
    }

    inline bool isLeader() override
    {
        return mIsLeader;
    }

    bool isLeader(PublicKey const& pubkey, LedgerIndex curSeq, uint64 view) override; 

    std::size_t quorum() override;

    std::int32_t getPubkeyIndex(PublicKey const& pubkey) override;

	void addActive(std::shared_ptr<PeerImp> const& peer);

	void eraseDeactivate();

    inline uint256 getFinalLedgerHash()
    {
        assert(mFinalLedger);
        return mFinalLedger->ledgerHash();
    }

    inline uint256 getMicroLedgerHash()
    {
        assert(mMicroLedger);
        return mMicroLedger->ledgerHash();
    }

    inline void setAcquiring(uint256 hash)
    {
        std::lock_guard<std::recursive_mutex> _(mMLBMutex);
        mAcquiring.emplace(hash);
    }

    inline boost::optional<uint256> getAcquiring()
    {
        std::lock_guard<std::recursive_mutex> _(mMLBMutex);
        return mAcquiring;
    }

    inline bool hasAcquireMap()
    {
        std::lock_guard<std::recursive_mutex> _(mMLBMutex);
        return mAcquireMap.size();
    }

    bool microLedgersAllReady();

    std::vector<std::shared_ptr<MicroLedger const>> const canonicalMicroLedgers();

    uint32 firstMissingMicroLedger();

    std::pair<uint256, bool> microLedgerSetHash();

    void onViewChange(
        ViewChangeManager& vcManager,
        ViewChange::GenReason reason,
        uint64 view,
        LedgerIndex preSeq,
        LedgerHash preHash);

    void onConsensusStart(LedgerIndex seq, uint64 view, PublicKey const pubkey);

    void commitMicroLedgerBuffer(LedgerIndex seq);

    boost::optional<uint256> acquireMicroLedgerSet(uint256 setID);

    void trigger(uint256 setID);

    std::size_t selectPeers(
        std::set<std::shared_ptr<Peer>>& set,
        std::size_t limit,
        std::function<bool(std::shared_ptr<Peer> const&)> score);

    MicroLedger const&
    buildMicroLedger(OpenView const& view, std::shared_ptr<CanonicalTXSet const> txSet);

    void buildFinalLedger(OpenView const& view, std::shared_ptr<Ledger const> ledger);

    bool checkAccept();

    void submitFinalLedger();

    Overlay::PeerSequence getActivePeers(uint32);

    void sendMessage(std::shared_ptr<Message> const &m) override;
    void relay(
        boost::optional<std::set<HashRouter::PeerShortID>> toSkip,
        std::shared_ptr<Message> const &m) override;
    void distributeMessage(std::shared_ptr<Message> const &m, bool forceBroadcast = false);

    void onMessage(std::shared_ptr<protocol::TMMicroLedgerSubmit> const& m);
    void onMessage(std::shared_ptr<protocol::TMMicroLedgerAcquire> const& m, std::weak_ptr<PeerImp> weak);
    void onMessage(std::shared_ptr<protocol::TMMicroLedgerInfos> const& m);

    bool checkNetQuorum();

    template <class UnaryFunc>
    void
    for_each(UnaryFunc&& f)
    {
        std::lock_guard<std::recursive_mutex> lock(mPeersMutex);

        // Iterate over a copy of the peer list because peer
        // destruction can invalidate iterators.
        std::vector<std::weak_ptr<PeerImp>> wp;
        wp.reserve(mPeers.size());

        for (auto& x : mPeers)
            wp.push_back(x);

        for (auto& w : wp)
        {
            if (auto p = w.lock())
                f(std::move(p));
        }
    }
};

}

#endif
