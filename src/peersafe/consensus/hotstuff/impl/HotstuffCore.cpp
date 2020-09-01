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

#include <peersafe/consensus/hotstuff/impl/HotstuffCore.h>

namespace ripple { namespace hotstuff {

HotstuffCore::HotstuffCore(
    const ReplicaID& id,
    Storage* storage,
    Executor* executor)
: id_(id)
, vHeight_(1)
, genesis_()
, lock_()
, exec_()
, leaf_()
, hight_qc_()
, pendingQCs_()
, storage_(storage)
, executor_(executor) {
    
    genesis_.committed = true;
    genesis_.height = vHeight_;
    
    genesis_.hash = Block::blockHash(genesis_);
    storage_->addBlock(genesis_);

    lock_ = genesis_;
    exec_ = genesis_;
    leaf_ = genesis_;
    hight_qc_ = QuorumCert(genesis_.hash);
}

HotstuffCore::~HotstuffCore() {
}

Block HotstuffCore::CreatePropose() {
    Command cmd;
    storage_->command(5, cmd);
    Block block = CreateLeaf(leaf_, cmd, hight_qc_, leaf_.height + 1);
    return block;
}

bool HotstuffCore::OnReceiveProposal(const Block &block, PartialCert& cert) {
    bool safe = false;
    do {
        if (executor_->accept(block.cmd) == false)
            break;

        storage_->addBlock(block);

        if (block.height <= vHeight_)
            break;

        Block qcBlock;
        if (storage_->getBlock(block.justify.hash(), qcBlock) == false)
            break;

        if (qcBlock.height > lock_.height) {
            safe = true;
        }
        else {
            Block b = block;
            bool bOk = true;
            for (;;) {
                if (bOk && b.height > lock_.height + 1)
                    bOk = storage_->getBlock(b.parent, b);
                else
                    break;
            }

            if (bOk && b.parent == lock_.hash)
                safe = true;
        }

        if (safe == false)
            break;

        vHeight_ = block.height;

        PartialSig sig;
        if (executor_->signature(id_, Block::blockHash(block), sig) == false) {
            safe = false;
            break;
        }

        cert.partialSig = sig;
        cert.blockHash = block.hash;
        safe = true;
    } while (false);

    update(block);

    return safe;
}

void HotstuffCore::OnReceiveVote(const PartialCert& cert) {
    if(executor_->verifySignature(cert.partialSig, cert.blockHash) == false)
        return;

    auto qcs = pendingQCs_.find(cert.blockHash);
    if(qcs == pendingQCs_.end()) {
        Block expect_block;
        if(storage_->getBlock(cert.blockHash, expect_block) == false)
            return;
        
        if(expect_block.height <= leaf_.height)
            return;

        QuorumCert qc = QuorumCert(expect_block.hash);
        qc.addPartiSig(cert);
        qcs = pendingQCs_.emplace(expect_block.hash, qc).first;
    } else {
        qcs->second.addPartiSig(cert);
    }

    if(qcs->second.sizeOfSig() >= executor_->quorumSize()) {
        updateHighQC(qcs->second);
    }
}

Block HotstuffCore::CreateLeaf(const Block& leaf, 
    const Command& cmd, 
    const QuorumCert& qc, 
    int height) {

    Block block;

    block.parent = leaf.hash;
    block.cmd = cmd;
    block.height = height;
    block.justify = qc;

    block.hash = Block::blockHash(block);

    return block;
}

void HotstuffCore::update(const Block &block) {
    // b'' <- b.justify.node;
    // b' <- b''.justify.node;
    // b <- b'.justify.node;
    // block1 = b'', block2 = b', block3 = b
    Block block1, block2, block3;
    if(storage_->getBlock(block.justify.hash(), block1) == false)
        return;
    if(block1.committed)
        return;

    // pre-commit on block1
    updateHighQC(block.justify);

    if(storage_->getBlock(block1.justify.hash(), block2) == false)
        return;
    if(block2.committed)
        return;

    if(block2.height > lock_.height) {
        // commit on block2
        lock_ = block2;
    }

    if(storage_->getBlock(block2.justify.hash(), block3) == false)
        return;
    if(block3.committed)
        return;

    if(block1.parent == block2.hash && block2.parent == block3.hash) {
        commit(block3);
        // decided on block3
        exec_ = block3;
    }

    // free up space by deleting old data
}

void HotstuffCore::commit(Block &block) {
    if(exec_.height < block.height) {
        Block parent;
        if(storage_->getBlock(block.parent, parent)) {
            commit(parent);
        }
        block.committed = true;
        executor_->consented(block);
    }
}

bool HotstuffCore::updateHighQC(const QuorumCert &qc) {
    if (qc.sizeOfSig() < executor_->quorumSize()) {
        return false;
    }
    int numVerified = 0;
    auto sigs = qc.sigs();
    for(auto it = sigs.begin(); it != sigs.end(); it++) {
        if(executor_->verifySignature(it->second, qc.hash()))
            numVerified++;
    }

    if(numVerified < executor_->quorumSize()) {
        return false;
    }

    Block newBlock;
    if(storage_->getBlock(qc.hash(), newBlock) == false)
        return false;

    Block oldBlock;
    if(storage_->getBlock(hight_qc_.hash(), oldBlock) == false)
        return false;
    
    if(newBlock.height > oldBlock.height) {
        hight_qc_ = qc;
        leaf_ = newBlock;
        return true;
    }
    return false;
}

} // namespace hotstuff
} // namespace ripple