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

#ifndef PEERSAFE_APP_SHARD_NODEBASE_H_INCLUDED
#define PEERSAFE_APP_SHARD_NODEBASE_H_INCLUDED

#include <ripple/protocol/Protocol.h>
#include <ripple/protocol/PublicKey.h>
#include <ripple/overlay/Overlay.h>
#include <ripple/app/misc/HashRouter.h>


namespace ripple {

class NodeBase {

public:

    NodeBase() {}
    ~NodeBase() {}

    virtual bool isLeader() = 0;

    virtual bool isLeader(PublicKey const& pubkey, LedgerIndex curSeq, uint64 view) = 0;

    virtual std::size_t quorum() = 0;

    virtual void onConsensusStart(LedgerIndex seq, uint64 view, PublicKey const pubkey) = 0;

    virtual std::unique_ptr<ValidatorList>& validatorsPtr() = 0;

    virtual Overlay::PeerSequence getActivePeers(uint32 shardID) = 0;

    virtual std::int32_t getPubkeyIndex(PublicKey const& pubkey) = 0;

    virtual void sendMessage(std::shared_ptr<Message> const &m) = 0;

    virtual void relay(
        boost::optional<std::set<HashRouter::PeerShortID>> toSkip,
        std::shared_ptr<Message> const &m) = 0;
};

}

#endif
