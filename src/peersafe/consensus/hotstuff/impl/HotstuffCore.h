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

#ifndef RIPPLE_CONSENSUS_HOTSTUFF_CORE_H
#define RIPPLE_CONSENSUS_HOTSTUFF_CORE_H

#include <mutex>
#include <memory>

#include <boost/signals2.hpp>

#include <peersafe/consensus/hotstuff/impl/Block.h>
#include <peersafe/consensus/hotstuff/impl/Crypto.h>

#include <ripple/basics/Log.h>
#include <ripple/core/JobQueue.h>

namespace ripple { namespace hotstuff {

class Storage {
public:
    virtual ~Storage() {}

    // for transactions
    virtual void command(std::size_t batch_size, Command& cmd) = 0;

    // for blocks
    virtual bool addBlock(const Block& block) = 0;

    // 通过 block hash 获取 block，如果本地没有函数返回 false
    virtual bool blockOf(const BlockHash& hash, Block& block) const = 0;
    // 通过 block hash 获取 block, 如果本地没有则需要从网络同步
    virtual bool expectBlock(const BlockHash& hash, Block& block) = 0;
protected:
    Storage() {}
};

class Executor {
public:
    virtual ~Executor() {}

    virtual bool accept(const Command& cmd) = 0;
    virtual void consented(const Block& block) = 0;

    virtual int quorumSize() = 0; 

    virtual bool signature(const ReplicaID& id, const BlockHash& hash, PartialSig& sig) = 0;
    virtual bool verifySignature(const PartialSig& sig, const BlockHash& hash) = 0;
protected:
    Executor() {}
};

struct Event
{
    enum Type {
        QCFinish,
        ReceiveProposal,
        ReceiveVote,
        ReceiveNewView,
        Commit,
    };
    
    Type type;
    QuorumCert QC;
    Block block;
    ReplicaID replica;
};

class Signal : public std::enable_shared_from_this<Signal> {
    using OnEmit = boost::signals2::signal<void(const Event& event)>;
public:
    using pointer = std::shared_ptr<Signal>;
    using weak = std::weak_ptr<Signal>;
    using OnEmitSlotType = OnEmit::slot_type;

    Signal(ripple::JobQueue* jobQueue)
    : onEmit_()
    , jobQueue_(jobQueue) {
    }

    ~Signal() {
    }

    void emitEvent(const Event& event) {
        std::weak_ptr<Signal> This = shared_from_this();
        jobQueue_->addJob(
            jtPROPOSAL_t, 
            "emitHotstuffEvent",
            [This, event](Job&) {
                auto p = This.lock();
                if(p)
                    p->onEmit_(event);
        });
    }

    boost::signals2::connection doOnEmitEvent(const OnEmitSlotType& slot) {
        return onEmit_.connect(slot);
    }

private:
    OnEmit onEmit_;
    ripple::JobQueue* jobQueue_;
};

class HotstuffCore {
public: 
    HotstuffCore(
        const ReplicaID& id,
        const beast::Journal& journal,
        const Signal::weak& signal,
        Storage* storage,
        Executor* executor);
    ~HotstuffCore();

    Block CreatePropose(int batch_size);

    bool OnReceiveProposal(const Block& block, PartialCert& cert);
    void OnReceiveVote(const PartialCert& cert);
    void OnReceiveNewView(const QuorumCert& qc);

    void reset();

    const Block leaf() {
        const std::lock_guard<std::mutex> lock(mutex_);
        return leaf_;
    }

    void setLeaf(const Block& block) {
        {
            const std::lock_guard<std::mutex> lock(mutex_);
            leaf_ = block;
        }
        storage_->addBlock(block);
    }

    const int Height() {
        const std::lock_guard<std::mutex> lock(mutex_);
        return leaf_.height;
    }

    const Block& votedBlock() {
        const std::lock_guard<std::mutex> lock(mutex_);
        return votedBlock_;
    }
    
    const QuorumCert HightQC() {
        const std::lock_guard<std::mutex> lock(mutex_);
        return hight_qc_;
    }

    Block CreateLeaf(const Block& leaf, 
        const Command& cmd, 
        const QuorumCert& qc, 
        int height);
private:
    void handleVote(const PartialCert& cert);
    void update(const Block& block);
    void commit(Block& block);
    bool updateHighQC(const QuorumCert& qc);
    void emitEvent(const Event& event);

    const ReplicaID& id_;
    std::mutex mutex_;
    beast::Journal journal_;
    //int vHeight_;
    Block votedBlock_;
    Block genesis_;
    Block lock_;
    Block exec_;
    Block leaf_;
    QuorumCert hight_qc_;

    //using PendingKey = BlockHash;
    //using PendingValue = QuorumCert;
    std::map<BlockHash, std::vector<PartialCert>> pendingPartialCerts_;
    std::map<BlockHash, QuorumCert> pendingQCs_;

    Signal::weak signal_;
    Storage* storage_;
    Executor* executor_;
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_CORE_H