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

#include <mutex>


namespace ripple {

class Application;
class Config;
class ShardManager;
class PeerImp;
class Peer;

class Committee : public NodeBase {

private:

	ShardManager&											mShardManager;

	Application&											app_;
	beast::Journal											journal_;
	Config&													cfg_;

    // Used if I am a Committee node
    bool                                                    mIsLeader;
    std::map<uint32,
        std::shared_ptr<MicroLedger>>                       mValidMicroLedgers;     // This round. mapping shardID --> MicroLedger
    std::unordered_map<
        LedgerIndex,
        std::unordered_map<uint256,
        std::shared_ptr<MicroLedger>>>                      mMicroLedgerBuffer;     // seq --> (hash, MicroLedger)
    std::recursive_mutex                                    mMLBMutex;              // Micro ledger buffer mutex

    boost::asio::basic_waitable_timer<
        std::chrono::steady_clock>                          mTimer;

    boost::optional<FinalLedger>                            mFinalLedger;
    std::map<LedgerIndex,
        std::vector<std::tuple<uint256, PublicKey, Blob>>>  mSignatureBuffer;
    std::recursive_mutex                                    mSignsMutex;

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

	void eraseDeactivate(Peer::id_t id);

    inline uint256 getFinalLedgerHash()
    {
        assert(mFinalLedger);
        return mFinalLedger->ledgerHash();
    }

    bool microLedgersAllReady();

    std::vector<std::shared_ptr<MicroLedger const>> const& canonicalMicroLedgers();

    uint32 firstMissingMicroLedger();

    uint256 microLedgerSetHash();

    void onConsensusStart(LedgerIndex seq, uint64 view, PublicKey const pubkey);

    void commitMicroLedgerBuffer(LedgerIndex seq);

    boost::optional<uint256> acquireMicroLedgerSet();

    void setTimer(uint32 repeats);

    void buildFinalLedger(OpenView const& view, std::shared_ptr<Ledger const> ledger);

    void commitSignatureBuffer();

    void recvValidation(PublicKey& pubKey, STValidation& val);

    bool checkAccept();

    void submitFinalLedger();

    Overlay::PeerSequence getActivePeers(uint32);

    void sendMessage(std::shared_ptr<Message> const &m);

    void onMessage(protocol::TMMicroLedgerSubmit const& m);
    void onMessage(protocol::TMMicroLedgerAcquire const& m, std::weak_ptr<PeerImp> weak);

};

}

#endif
