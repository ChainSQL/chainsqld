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

#include <peersafe/consensus/hotstuff/impl/Block.h>
#include <peersafe/consensus/hotstuff/impl/Crypto.h>

namespace ripple { namespace hotstuff {

class Storage {
public:
    virtual ~Storage() {}

    // for transactions
    virtual void command(std::size_t batch_size, Command& cmd) = 0;

    // for blocks
    virtual bool addBlock(const Block& block) = 0;
    virtual bool getBlock(const BlockHash& hash, Block& block) const = 0;
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

class HotstuffCore {
public: 
    HotstuffCore(
        const ReplicaID& id,
        Storage* storage,
        Executor* executor);
    ~HotstuffCore();

    Block CreatePropose();

    bool OnReceiveProposal(const Block& block, PartialCert& cert);
    void OnReceiveVote(const PartialCert& cert);

    const int& Height() const {
        return vHeight_;
    }

private:
    Block CreateLeaf(const Block& leaf, 
        const Command& cmd, 
        const QuorumCert& qc, 
        int height);
    void update(const Block& block);
    void commit(Block& block);
    bool updateHighQC(const QuorumCert& qc);

    const ReplicaID& id_;
    int vHeight_;
    Block genesis_;
    Block lock_;
    Block exec_;
    Block leaf_;
    QuorumCert hight_qc_;

    using PendingKey = BlockHash;
    using PendingValue = QuorumCert;
    std::map<PendingKey, PendingValue> pendingQCs_;

    Storage* storage_;
    Executor* executor_;
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_CORE_H