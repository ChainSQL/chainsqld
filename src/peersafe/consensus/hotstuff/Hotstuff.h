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

#include <peersafe/consensus/hotstuff/impl/RoundManager.h>

#include <ripple/basics/Log.h>
#include <ripple/core/JobQueue.h>

namespace ripple { namespace hotstuff {

//class Sender {
//public:
//    virtual ~Sender() {}
//
//    virtual void proposal(const ReplicaID& id, const Block& block) = 0;
//    //virtual void vote(const ReplicaID& id, const PartialCert& cert) = 0;
//    //virtual void newView(const ReplicaID& id, const QuorumCert& qc) = 0;
//protected:
//    Sender() {}
//};

//struct Config {
//    // self id
//    ReplicaID id;
//    // change a new leader per view_change
//    int view_change;
//    // schedule for electing a new leader
//    std::vector<ReplicaID> leader_schedule;
//    // generate a dummy block after timeout (seconds)
//    int timeout;
//    // commands batch size
//    int cmd_batch_size;
//
//    Config()
//    : id(0)
//    , view_change(1)
//    , leader_schedule()
//    , timeout(60)
//    , cmd_batch_size(100) {
//
//    }
//};

class Hotstuff {
public:
    Hotstuff(
        const beast::Journal& journal,
        BlockStorage* storage,
		EpochState* epoch_state,
		RoundState* round_state,
		ProposalGenerator* proposal_generator,
		ProposerElection* proposer_election,
		NetWork* network);

    ~Hotstuff();

	int start();
private:
	HotstuffCore hotstuff_core_;
	RoundManager* round_manager_;
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_H