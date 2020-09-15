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

#ifndef RIPPLE_CONSENSUS_HOTSTUFF_H
#define RIPPLE_CONSENSUS_HOTSTUFF_H

#include <vector>

#include <peersafe/consensus/hotstuff/impl/Block.h>
#include <peersafe/consensus/hotstuff/impl/HotstuffCore.h>
#include <ripple/basics/Log.h>

namespace ripple { namespace hotstuff {

class Pacemaker;

class Sender {
public:
    virtual ~Sender() {}

    virtual void proposal(const ReplicaID& id, const Block& block) = 0;
    virtual void vote(const ReplicaID& id, const PartialCert& cert) = 0;
    virtual void newView(const ReplicaID& id, const QuorumCert& qc) = 0;
protected:
    Sender() {}
};

struct Config {
    // self id
    ReplicaID id;
    // change a new leader per view_change
    int view_change;
    // schedule for electing a new leader
    std::vector<ReplicaID> leader_schedule;
    // generate a dummy block after timeout (seconds)
    int timeout;
};

class Hotstuff {
public:
    Hotstuff(
        ripple::JobQueue* jobQueue,
        const Config& config,
        const beast::Journal& journal,
        Sender* sender,
        Storage* storage,
        Executor* executor,
        Pacemaker* pacemaker);

    ~Hotstuff();

    ReplicaID id() const {
        return config_.id;
    }

    void propose();
    void nextSyncNewView(int height);

    void handlePropose(const Block& block);
    void handleVote(const PartialCert& cert);
    void handleNewView(const QuorumCert& qc);

private:
    friend class RoundRobinLeader;
     // operate hotstuffcore
    const Block leaf();
    void setLeaf(const Block& block);
    const int Height();  
    const Block& votedBlock();
    const QuorumCert HightQC();
    Block CreateLeaf(const Block& leaf, 
        const Command& cmd, 
        const QuorumCert& qc, 
        int height);

    const Config& config() const {
        return config_;
    }

    Config& config() {
        return config_;
    }

    void broadCast(const Block& block);

    Config config_;
    Signal::pointer signal_;
    HotstuffCore* hotstuff_core_;
    Sender* sender_;
    Pacemaker* pacemaker_;
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_H