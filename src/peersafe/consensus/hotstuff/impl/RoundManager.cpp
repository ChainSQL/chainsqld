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

#include <peersafe/consensus/hotstuff/impl/RoundManager.h>

namespace ripple {
namespace hotstuff {

RoundManager::RoundManager(
	BlockStorage* block_store,
	RoundState* round_state,
	HotstuffCore* hotstuff_core,
	ProposalGenerator* proposal_generator,
	ProposerElection* proposer_election,
	NetWork* network)
: block_store_(block_store)
, round_state_(round_state)
, hotstuff_core_(hotstuff_core)
, proposal_generator_(proposal_generator)
, proposer_election_(proposer_election)
, network_(network) {

}

RoundManager::~RoundManager() {

}

int RoundManager::start() {
	// open a new round
	boost::optional<NewRoundEvent> new_round_event = round_state_->ProcessCertificates(block_store_->sync_info());
	if (new_round_event) {
		int ret = ProcessNewRoundEvent(new_round_event.get());
		// setup round timeout
		boost::asio::steady_timer& roundTimeoutTimer = round_state_->RoundTimeoutTimer();
		roundTimeoutTimer.expires_from_now(std::chrono::seconds(7));
		roundTimeoutTimer.async_wait(
			std::bind(&RoundManager::ProcessLocalTimeout, this, std::placeholders::_1, new_round_event->round));

		return ret;
	}
	return 1;
}

int RoundManager::ProcessNewRoundEvent(const NewRoundEvent& new_round_event) {
	if (!proposer_election_->IsValidProposer(proposal_generator_->author(), new_round_event.round)) {
		return 1;
	}
	boost::optional<Block> proposal = GenerateProposal(new_round_event);
	if (proposal) {
		network_->broadcast(proposal.get(), block_store_->sync_info());
	}
	return 0;
}

boost::optional<Block> RoundManager::GenerateProposal(const NewRoundEvent& event) {
	boost::optional<BlockData> proposal = proposal_generator_->Proposal(event.round);
	if (proposal) {
		return hotstuff_core_->SignProposal(proposal.get());
	}

	return boost::optional<Block>();
}

void RoundManager::ProcessLocalTimeout(const boost::system::error_code& ec, Round round) {
	if (ec) {
		return;
	}
}

int RoundManager::ProcessProposal(const Block& proposal, const SyncInfo& sync_info) {
	return 0;
}

} // namespace hotstuff
} // namespace ripple