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

#ifndef PEERSAFE_APP_SHARD_NODE_H_INCLUDED
#define PEERSAFE_APP_SHARD_NODE_H_INCLUDED

#include "ripple.pb.h"
#include <ripple/beast/utility/Journal.h>
#include <ripple/overlay/impl/PeerImp.h>
#include <ripple/app/misc/ValidatorList.h>
#include <ripple/protocol/STValidation.h>
#include <peersafe/app/shard/MicroLedger.h>
#include <ripple/app/consensus/RCLCxTx.h>
#include <ripple/app/consensus/RCLCxLedger.h>

#include <vector>
#include <mutex>

namespace ripple {

class Application;
class Config;
class ShardManager;


class Node {

public:
    enum {
        InvalidShardID   = -1,
        CommitteeShardID = 0,
    };

private:

    // These field used if I'm a shard node.
    int                                                 mShardID        = InvalidShardID;
    bool                                                mIsLeader       = false;
    boost::optional<MicroLedger>                        mMicroLedger;
    std::map<uint256,
        std::vector<std::pair<PublicKey, Blob>>>        mSignatureBuffer;
    std::recursive_mutex                                mSignsMutex;


	typedef hash_map<Peer::id_t, std::weak_ptr<PeerImp>>		HashMapOfPeers;
    typedef std::map<std::uint32_t, HashMapOfPeers >			MapOfShardPeers;
    typedef std::map<int, std::unique_ptr <ValidatorList>>      MapOfShardValidators;

    // Common field
    // Hold all shard peers
    MapOfShardPeers                                     mMapOfShardPeers;
    std::mutex                                          mPeersMutex;

    // Hold all shard validators
    MapOfShardValidators                                mMapOfShardValidators;

    ShardManager&                                       mShardManager;

    Application&                                        app_;
    beast::Journal                                      journal_;
    Config&                                             cfg_;

public:

    Node(ShardManager& m, Application& app, Config& cfg, beast::Journal journal);
    ~Node() {}

    inline uint32 shardID()
    { 
        return mShardID;
    }

    inline MapOfShardPeers& shardPeers()
    {
        return mMapOfShardPeers;
    }

    inline MapOfShardValidators& shardValidators()
    {
        return mMapOfShardValidators;
    }

    inline bool isLeader()
    {
        return mIsLeader;
    }

	void addActive(std::shared_ptr<PeerImp> const& peer);

	void eraseDeactivate(Peer::id_t id);

    void onConsensusStart(LedgerIndex seq, uint64 view, PublicKey const pubkey);

    void doAccept(RCLTxSet const& set, RCLCxLedger const& previousLedger, NetClock::time_point closeTime);

    void validate(MicroLedger &microLedger);

    void commitSignatureBuffer();

    void sendValidation(protocol::TMValidation& m);

    void recvValidation(PublicKey& pubKey, STValidation& val);

    void checkAccept();

    void submitMicroLedger(bool withTxMeta);

    void onMessage(protocol::TMFinalLedgerSubmit const& m);


	void sendTransaction(unsigned int shardIndex,protocol::TMTransaction& m);


};

}

#endif
