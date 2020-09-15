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
    ripple::JobQueue* jobQueue,
    const Config& config,
    const beast::Journal& journal,
    Sender* sender,
    Storage* storage,
    Executor* executor,
    Pacemaker* pacemaker)
: config_(config)
, signal_(std::make_shared<Signal>(jobQueue))
, hotstuff_core_(new HotstuffCore(config_.id, journal, signal_, storage, executor))
, sender_(sender)
, pacemaker_(pacemaker) {
    pacemaker_->init(this, signal_.get());
}

Hotstuff::~Hotstuff() {
    delete hotstuff_core_;
}

void Hotstuff::propose() {
    Block block = hotstuff_core_->CreatePropose(config_.cmd_batch_size);
    // broadcast the block to all replicas
    broadCast(block);
    handlePropose(block);
}

void Hotstuff::nextSyncNewView(int height) {
    const QuorumCert& qc = hotstuff_core_->HightQC();
    ReplicaID nextLeader = pacemaker_->GetLeader(height);
    if(nextLeader == id()) {
        hotstuff_core_->OnReceiveNewView(qc);
    } else {
        sender_->newView(nextLeader, qc);
    }
}

void Hotstuff::handlePropose(const Block &block) {
    PartialCert cert;
    if(hotstuff_core_->OnReceiveProposal(block, cert) == false)
        return;

    // elect a new leader 
    int nextLeader = pacemaker_->GetLeader(block.height);
    if (id() == nextLeader) {
        hotstuff_core_->OnReceiveVote(cert);
    } else {
        // send votemsg to next leader
        sender_->vote(nextLeader, cert);
    }
}

void Hotstuff::handleVote(const PartialCert& cert) {
    hotstuff_core_->OnReceiveVote(cert);
}

void Hotstuff::handleNewView(const QuorumCert &qc) {
    hotstuff_core_->OnReceiveNewView(qc);
}

const Block Hotstuff::leaf() {
    return hotstuff_core_->leaf();
}

void Hotstuff::setLeaf(const Block &block) {
    return hotstuff_core_->setLeaf(block);
}

const int Hotstuff::Height() {
    return hotstuff_core_->Height();
}

const Block& Hotstuff::votedBlock() {
    return hotstuff_core_->votedBlock();
}

const QuorumCert Hotstuff::HightQC() {
    return hotstuff_core_->HightQC();
}

Block Hotstuff::CreateLeaf(const Block &leaf,
                           const Command &cmd,
                           const QuorumCert &qc,
                           int height) {
    return hotstuff_core_->CreateLeaf(leaf, cmd, qc, height);
}

void Hotstuff::broadCast(const Block& block) {
    std::size_t size = config_.leader_schedule.size();
    for(std::size_t i = 0; i < size; i++) {
        const ReplicaID& replicaID = config_.leader_schedule[i];
        if(replicaID != id())
            sender_->proposal(replicaID, block);
    }
}

} // namespace hotstuff
} // namespace ripple