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

#ifndef RIPPLE_CONSENSUS_HOTSTUFF_NETWORK_H
#define RIPPLE_CONSENSUS_HOTSTUFF_NETWORK_H

#include <peersafe/consensus/hotstuff/impl/Block.h>
#include <peersafe/consensus/hotstuff/impl/Vote.h>
#include <peersafe/consensus/hotstuff/impl/SyncInfo.h>
#include <peersafe/consensus/hotstuff/impl/EpochChange.h>

namespace ripple {
namespace hotstuff {

class NetWork { 
public:
	// 广播给每一个节点，包括自身
    virtual void broadcast(const Block& proposal, const SyncInfo& sync_info) = 0;
    virtual void broadcast(const Vote& vote, const SyncInfo& sync_info) = 0;
	virtual void broadcast(const EpochChange& epoch_change) = 0;
	virtual void sendVote(const Author& author, const Vote& vote, const SyncInfo& sync_info) = 0;
    
    virtual ~NetWork() {}
protected:
    NetWork() {}
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_NETWORK_H