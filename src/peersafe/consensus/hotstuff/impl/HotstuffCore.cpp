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
    const beast::Journal& journal,
    const Signal::weak& signal,
    Storage* storage,
    Executor* executor)
: id_(id)
, mutex_()
, journal_(journal)
//, vHeight_(0)
, votedBlock_()
, genesis_()
, lock_()
, exec_()
, leaf_()
, hight_qc_()
, pendingPartialCerts_()
, pendingQCs_()
, signal_(signal)
, storage_(storage)
, executor_(executor) {

    genesis_.committed = true;
    genesis_.height = 0;
    genesis_.id = 0;
    
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
    block.id = id_;
    block.hash = Block::blockHash(block);
    storage_->addBlock(block);
    return block;
}

bool HotstuffCore::OnReceiveProposal(const Block &block, PartialCert& cert) {
    JLOG(journal_.debug()) 
        << "Receive a proposal that hash is " 
        << ripple::strHex(ripple::Slice(block.hash.data(), block.hash.size()))
        << " and height is " << block.height;

    const std::lock_guard<std::mutex> lock(mutex_);
    bool safe = false;
    do {
        if (executor_->accept(block.cmd) == false) {
            JLOG(journal_.error()) 
                << "checking cmd failed that the hash of the block is "
                << ripple::strHex(ripple::Slice(block.hash.data(), block.hash.size()))
                << " and height is " << block.height;
            break;
        }

        storage_->addBlock(block);

        if (block.height <= votedBlock_.height) {
            JLOG(journal_.error()) 
                << "Block's Height is less that vHeight,"
                << "the hash of the block is " 
                << ripple::strHex(ripple::Slice(block.hash.data(), block.hash.size()))
                << " and height is " << block.height;
            break;
        }

        Block qcBlock;
        if (storage_->getBlock(block.justify.hash(), qcBlock) == false) {
            JLOG(journal_.error()) 
                << "Missing a block that hash is "
                << ripple::strHex(ripple::Slice(block.hash.data(), block.hash.size()))
                << " and height is " << block.height;
            break;
        }

        if (qcBlock.height > lock_.height) {
            safe = true;
        }
        else {
            JLOG(journal_.warn()) 
                << "liveness condition failed."
                << "the hash of the block is "
                << ripple::strHex(ripple::Slice(block.hash.data(), block.hash.size()))
                << " and height is " << block.height;
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
            else
                JLOG(journal_.error()) 
                    << "safety condition failed."
                    << "the hash of the block is "
                    << ripple::strHex(ripple::Slice(block.hash.data(), block.hash.size()))
                    << " and height is " << block.height;
        }

        if (safe == false) {
            JLOG(journal_.error()) 
                << "the block that hash is "
                << ripple::strHex(ripple::Slice(block.hash.data(), block.hash.size()))
                << " and height is " << block.height
                << " was not safe.";
            break;
        }

        //vHeight_ = block.height;
        votedBlock_ = block;

        PartialSig sig;
        if (executor_->signature(id_, Block::blockHash(block), sig) == false) {
            safe = false;
            JLOG(journal_.error()) 
                << "the replica that id is " << id_ << " signed to fail in block "
                << ripple::strHex(ripple::Slice(block.hash.data(), block.hash.size()));
            break;
        }

        cert.partialSig = sig;
        cert.blockHash = block.hash;
        safe = true;

        Event evnet{Event::ReceiveProposal, QuorumCert(), block, id_};
        emitEvent(evnet);

        JLOG(journal_.debug())
            << "successfully generated a vote for a block that hash is "
            << ripple::strHex(ripple::Slice(block.hash.data(), block.hash.size()))
            << " and height is " << block.height;

    } while (false);

    update(block);

    return safe;
}

void HotstuffCore::OnReceiveVote(const PartialCert& cert) {
    JLOG(journal_.debug()) 
        << "Receive a vote for a block that hash is " 
        << ripple::strHex(ripple::Slice(cert.blockHash.data(), cert.blockHash.size()));

    Event event{Event::ReceiveVote, QuorumCert(), Block(), cert.partialSig.ID};
    emitEvent(event);

    const std::lock_guard<std::mutex> lock(mutex_);
    if(executor_->verifySignature(cert.partialSig, cert.blockHash) == false) {
        JLOG(journal_.error())
            << "A block that hash is "
            << ripple::strHex(ripple::Slice(cert.blockHash.data(), cert.blockHash.size()))
            << " verified to fail";
             
        return;
    }

    // 当前 leader 收到其他 replicas 发送的 vote 消息的时候，
    // 可能此 leader 还没收到 proposal 消息，也就意味着 cert.blockHash
    // 指向的 block 在本地还没有(storage_->getBlock 获取失败)
    Block expect_block;
    if(storage_->getBlock(cert.blockHash, expect_block) == false) {
        auto it = pendingPartialCerts_.find(cert.blockHash);
        if(it == pendingPartialCerts_.end()) {
            std::vector<PartialCert> partialCerts;
            it = pendingPartialCerts_.emplace(cert.blockHash, partialCerts).first;
        }
        it->second.push_back(cert);
    } else {
        auto it = pendingPartialCerts_.find(cert.blockHash);
        if(it != pendingPartialCerts_.end()) {
            const std::vector<PartialCert>& partialCerts = it->second;
            std::size_t size = partialCerts.size();
            for(std::size_t i = 0; i < size; i++) {
                handleVote(partialCerts[i]);
            }
            pendingPartialCerts_.erase(it);
        }
        handleVote(cert);
    }
}

void HotstuffCore::OnReceiveNewView(const QuorumCert &qc) {
    JLOG(journal_.debug()) 
        << "Receive a new view that hash of block is " 
        << ripple::strHex(ripple::Slice(qc.hash().data(), qc.hash().size()));

    {
        const std::lock_guard<std::mutex> lock(mutex_);
        updateHighQC(qc);
    }

    Event evnet{Event::ReceiveNewView, qc, Block(), id_};
    emitEvent(evnet);
}

Block HotstuffCore::CreateLeaf(const Block& leaf, 
    const Command& cmd, 
    const QuorumCert& qc, 
    int height) {

    Block block;

    block.parent = leaf.hash;
    block.cmd = cmd;
    block.height = height;
    if(qc.isZero() == false)
        block.justify = qc;
    return block;
}

void HotstuffCore::handleVote(const PartialCert& cert) {
    auto qcs = pendingQCs_.find(cert.blockHash);
    if(qcs == pendingQCs_.end()) {
        Block expect_block;
        if(storage_->getBlock(cert.blockHash, expect_block) == false) {
            JLOG(journal_.error())
                << "Missing a block that hash is "
                << ripple::strHex(ripple::Slice(cert.blockHash.data(), cert.blockHash.size()));
            return;
        }
        
        if(expect_block.height <= leaf_.height) {
            JLOG(journal_.error())
                << "The height of the expected block is less that height of leaf."
                << "The hash of the expected block is "
                << ripple::strHex(ripple::Slice(cert.blockHash.data(), cert.blockHash.size()))
                << " and height is " << expect_block.height;
            return;
        }

        QuorumCert qc = QuorumCert(expect_block.hash);
        qc.addPartiSig(cert);
        qcs = pendingQCs_.emplace(expect_block.hash, qc).first;
    } else {
        qcs->second.addPartiSig(cert);
    }

    if(qcs->second.sizeOfSig() >= executor_->quorumSize()) {
        updateHighQC(qcs->second);

        Event evnet{Event::QCFinish, qcs->second, Block(), id_};
        emitEvent(evnet);
    }
}

void HotstuffCore::update(const Block &block) {
    // b'' <- b.justify.node;
    // b' <- b''.justify.node;
    // b <- b'.justify.node;
    // block1 = b'', block2 = b', block3 = b

    JLOG(journal_.debug()) 
        << "update a block that hash is " 
        << ripple::strHex(ripple::Slice(block.hash.data(), block.hash.size()))
        << " and height is " << block.height;

    Block block1, block2, block3;
    if(storage_->getBlock(block.justify.hash(), block1) == false) {
        JLOG(journal_.debug()) 
            << "Missing a block that hash is " 
            << ripple::strHex(ripple::Slice(block.justify.hash().data(), block.justify.hash().size()))
            << " when updating a block.";
        return;
    }
    if(block1.committed)
        return;

    // pre-commit on block1
    updateHighQC(block.justify);

    if(storage_->getBlock(block1.justify.hash(), block2) == false) {
        JLOG(journal_.debug()) 
            << "Missing a block that hash is " 
            << ripple::strHex(ripple::Slice(block1.justify.hash().data(), block1.justify.hash().size()))
            << " when updating a block.";
        return;
    }
    if(block2.committed)
        return;

    if(block2.height > lock_.height) {
        // commit on block2
        lock_ = block2;
    }

    if(storage_->getBlock(block2.justify.hash(), block3) == false) {
        JLOG(journal_.debug()) 
            << "Missing a block that hash is " 
            << ripple::strHex(ripple::Slice(block2.justify.hash().data(), block2.justify.hash().size()))
            << " when updating a block.";
        return;
    }
    if(block3.committed)
        return;

    //if(block1.parent == block2.hash && block2.parent == block3.hash) {
    if(block1.justify.hash() == block2.hash && block2.justify.hash() == block3.hash) {
        commit(block3);
        // decided on block3
        exec_ = block3;
    }

    // free up space by deleting old data
}

void HotstuffCore::commit(Block &block) {
    //if(block.isDummy())
    //    return;
    if(exec_.height < block.height) {
        Block parent;
        if(storage_->getBlock(block.parent, parent)) {
            commit(parent);
        }

        JLOG(journal_.debug())
            << "committed a block that height is " << block.height << " and "
            << "the hash is "
            << ripple::strHex(ripple::Slice(block.hash.data(), block.hash.size()));

        block.committed = true;
        executor_->consented(block);
    } else {
        JLOG(journal_.error())
            << "The height of a executed block is more than "
            << "the height of the current block that hash is "
            << ripple::strHex(ripple::Slice(block.hash.data(), block.hash.size()))
            << " and height is " << block.height;
    }
}

bool HotstuffCore::updateHighQC(const QuorumCert &qc) {

    JLOG(journal_.debug()) 
        << "update high QC that hash is " 
        << ripple::strHex(ripple::Slice(qc.hash().data(), qc.hash().size()));

    if(qc.isZero())
        return false;

    if (qc.sizeOfSig() < executor_->quorumSize()) {
        JLOG(journal_.error()) 
            << "The size of collecting QC was insufficient, the hash of qc is "
            << ripple::strHex(ripple::Slice(qc.hash().data(), qc.hash().size()));
        return false;
    }

    int numVerified = 0;
    auto sigs = qc.sigs();
    for(auto it = sigs.begin(); it != sigs.end(); it++) {
        if(executor_->verifySignature(it->second, qc.hash()))
            numVerified++;
    }

    if(numVerified < executor_->quorumSize()) {
        JLOG(journal_.error()) 
            << "The size of verifing signature was insufficient, the hash of qc is "
            << ripple::strHex(ripple::Slice(qc.hash().data(), qc.hash().size()));
        return false;
    }

    Block newBlock;
    if(storage_->getBlock(qc.hash(), newBlock) == false) {
        JLOG(journal_.debug()) 
            << "Missing a block that hash is " 
            << ripple::strHex(ripple::Slice(qc.hash().data(), qc.hash().size()));
        return false;
    }

    Block oldBlock;
    if(storage_->getBlock(hight_qc_.hash(), oldBlock) == false) {
        JLOG(journal_.debug()) 
            << "Missing a block that hash is " 
            << ripple::strHex(ripple::Slice(hight_qc_.hash().data(), hight_qc_.hash().size()));
        return false;
    }
    
    if(newBlock.height > oldBlock.height) {

        JLOG(journal_.debug())
            << "updated high QC was successful."
            << "old high qc is " 
            << ripple::strHex(ripple::Slice(hight_qc_.hash().data(), hight_qc_.hash().size()))
            << ", new high qc is " 
            << ripple::strHex(ripple::Slice(qc.hash().data(), qc.hash().size()))
            << ". the heigh of old leaf is " << leaf_.height << " and "
            << "the hash of old leaf is "
            << ripple::strHex(ripple::Slice(leaf_.hash.data(), leaf_.hash.size()))
            << ", the height of new leaf is " << newBlock.height << " and " 
            << "the hash of new leaf is "
            << ripple::strHex(ripple::Slice(newBlock.hash.data(), newBlock.hash.size()));

        hight_qc_ = qc;
        leaf_ = newBlock;
        return true;
    }
    return false;
}

void HotstuffCore::emitEvent(const Event& event) {
    Signal::pointer signal = signal_.lock();
    if(signal) {
        signal->emitEvent(event);
    }
}

} // namespace hotstuff
} // namespace ripple