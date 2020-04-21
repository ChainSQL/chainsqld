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

#include <vector>

namespace ripple {

class Application;
class Config;
class ShardManager;


class Node {

private:

    // Used if I am a shard node
    uint32                                              mShardID;

    typedef std::map<uint32, std::vector<std::weak_ptr <PeerImp>>> MapOfShardPeers;
    typedef std::map<uint32, std::unique_ptr <ValidatorList>> MapOfShardValidators;

    // Hold all shard peers
    MapOfShardPeers                                     mMapOfShardPeers;

    // Hold all shard validators
    MapOfShardValidators                                mMapOfShardValidators;

    ShardManager&                                       mShardManager;

    Application&										app_;
    beast::Journal										journal_;
    Config&												cfg_;


public:

    Node(ShardManager& m, Application& app, Config& cfg, beast::Journal journal);
    ~Node() {}

    inline int32_t ShardID()
    { 
        return mShardID;
    }

    inline MapOfShardPeers& ShardPeers()
    {
        return mMapOfShardPeers;
    }

    inline MapOfShardValidators& ShardValidators()
    {
        return mMapOfShardValidators;
    }

    void onMessage(protocol::TMFinalLedgerSubmit const& m);
};

}

#endif
