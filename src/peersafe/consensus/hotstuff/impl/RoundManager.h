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

#ifndef RIPPLE_CONSENSUS_HOTSTUFF_ROUND_MANAGER_H
#define RIPPLE_CONSENSUS_HOTSTUFF_ROUND_MANAGER_H

#include <peersafe/consensus/hotstuff/impl/HotstuffCore.h>
#include <peersafe/consensus/hotstuff/impl/RoundState.h>
#include <peersafe/consensus/hotstuff/impl/ProposerElection.h>
#include <peersafe/consensus/hotstuff/impl/ProposalGenerator.h>
#include <peersafe/consensus/hotstuff/impl/NetWork.h>

namespace ripple {
namespace hotstuff {

class RoundManager {
public:
	RoundManager(
		BlockStorage* block_store,
		RoundState* round_state,
		HotstuffCore* hotstuff_core,
		ProposalGenerator* proposal_generator,
		ProposerElection* proposer_election,
		NetWork* network);
	~RoundManager();
	
	int start();
	int ProcessProposal(const Block& proposal, const SyncInfo& sync_info);
private:
	int ProcessNewRoundEvent(const NewRoundEvent& event);
	boost::optional<Block> GenerateProposal(const NewRoundEvent& event);

	void ProcessLocalTimeout(const boost::system::error_code& ec, Round round);

	BlockStorage* block_store_;
	RoundState* round_state_;
	HotstuffCore* hotstuff_core_;
	ProposalGenerator* proposal_generator_;
	ProposerElection* proposer_election_;
	NetWork* network_;
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_ROUND_MANAGER_H