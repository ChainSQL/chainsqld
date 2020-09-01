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

#include <peersafe/consensus/hotstuff/Hotstuff.h>
#include <peersafe/consensus/hotstuff/Pacemaker.h>
#include <peersafe/consensus/hotstuff/impl/Block.h>

namespace ripple { namespace hotstuff {

Hotstuff::Hotstuff(
    ReplicaID id, 
    Sender* sender,
    Storage* storage,
    Executor* executor,
    Pacemaker* pacemaker)
: id_(id)
, hotstuff_core_(HotstuffCore(id, storage, executor))
, sender_(sender)
, pacemaker_(pacemaker) {
    pacemaker_->init(this);
}

Hotstuff::~Hotstuff() {
}

void Hotstuff::Propose() {
    Block block = hotstuff_core_.CreatePropose();
    // broadcast the block to all replicas
    broadCast(block);
    handlePropose(block);
}

void Hotstuff::handlePropose(const Block &block) {
    PartialCert cert;
    if(hotstuff_core_.OnReceiveProposal(block, cert) == false)
        return;

    int nextLeader = pacemaker_->GetLeader(block.height);
    if (id() == nextLeader) {
        hotstuff_core_.OnReceiveVote(cert);
    } else {
        // send votemsg to next leader
        sender_->vote(nextLeader, cert);
    }
}

void Hotstuff::handleVote(const PartialCert& cert) {
    hotstuff_core_.OnReceiveVote(cert);
}

void Hotstuff::broadCast(const Block& block) {
    for(int i = 0; i < 10; i++) {
        sender_->proposal(i, block);
    }
}

} // namespace hotstuff
} // namespace ripple