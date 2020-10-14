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
	const beast::Journal& journal,
	BlockStorage* block_store,
	RoundState* round_state,
	HotstuffCore* hotstuff_core,
	ProposalGenerator* proposal_generator,
	ProposerElection* proposer_election,
	NetWork* network)
: journal_(journal)
, block_store_(block_store)
, round_state_(round_state)
, hotstuff_core_(hotstuff_core)
, proposal_generator_(proposal_generator)
, proposer_election_(proposer_election)
, network_(network)
, stop_(false) {

}

RoundManager::~RoundManager() {
	assert(stop_ == true);
}

int RoundManager::start() {
	// open a new round
	boost::optional<NewRoundEvent> new_round_event = round_state_->ProcessCertificates(block_store_->sync_info());
	if (new_round_event) {
		return ProcessNewRoundEvent(new_round_event.get());
	}
	return 1;
}

void RoundManager::stop() {
	stop_ = true;

	for (;;) {
		if (round_state_->RoundTimeoutTimer().cancel() == 0)
			break;
	}
}

int RoundManager::ProcessNewRoundEvent(const NewRoundEvent& new_round_event) {
	// setup round timeout
	boost::asio::steady_timer& roundTimeoutTimer = round_state_->RoundTimeoutTimer();
	roundTimeoutTimer.expires_from_now(std::chrono::seconds(7));
	roundTimeoutTimer.async_wait(
		std::bind(&RoundManager::ProcessLocalTimeout, this, std::placeholders::_1, new_round_event.round));

	if (!proposer_election_->IsValidProposer(proposal_generator_->author(), new_round_event.round)) {
		JLOG(journal_.error())
			<< "ProcessNewRoundEvent: invalidProposel."
			<< " New round is " << new_round_event.round
			<< " and proposal's author is " << proposal_generator_->author();
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
	if (stop_)
		return 1;

	if (EnsureRoundAndSyncUp(
		proposal.block_data().round,
		sync_info,
		proposal.block_data().author()) == false) {
		JLOG(journal_.error())
			<< "Stale proposal, current round " 
			<< round_state_->current_round();
		return 1;
	}
	return ProcessProposal(proposal);
}

int RoundManager::ProcessVote(const Vote& vote, const SyncInfo& sync_info) {
	if (stop_)
		return 1;

	if (EnsureRoundAndSyncUp(
		vote.vote_data().proposed().round,
		sync_info,
		vote.author()) == false) {
		return 1;
	}
	return ProcessVote(vote);
}

int RoundManager::ProcessProposal(const Block& proposal) {
	if (proposer_election_->IsValidProposal(proposal) == false) {
		JLOG(journal_.error())
			<< "Proposer " << proposal.block_data().author()
			<< " for the proposal"
			<< " is not a valid proposer for this round "
			<< proposal.block_data().round;
		return 1;
	}

	Round proposal_round = proposal.block_data().round;
	Vote vote;
	if (ExecuteAndVote(proposal, vote) == false) {
		JLOG(journal_.error())
			<< "Execute and vote failed for the proposal";
		return 1;
	}

	Author next_leader = proposer_election_->GetValidProposer(proposal_round + 1);
	round_state_->recordVote(vote);
	network_->sendVote(next_leader, vote, block_store_->sync_info());
	return 0;
}

int RoundManager::ProcessVote(const Vote& vote) {
	Round next_round = vote.vote_data().proposed().round + 1;
	if (proposer_election_->IsValidProposer(proposal_generator_->author(), next_round) == false) {
		JLOG(journal_.warn())
			<< "Received a vote, but i am not a valid proposer for this round "
			<< next_round << ", ignore.";
		return 1;
	}

	// TODO
	// Get QC from block storage

	// Add the vote and check whether it completes a new QC or a TC
	QuorumCertificate quorumCert;
	if (round_state_->insertVote(
		vote, 
		hotstuff_core_->epochState()->verifier,
		quorumCert) == PendingVotes::VoteReceptionResult::NewQuorumCertificate) {

		NewQCAggregated(quorumCert);
		return 0;
	}
	return 1;
}

/// The function generates a VoteMsg for a given proposed_block:
/// * first execute the block and add it to the block store
/// * then verify the voting rules
bool RoundManager::ExecuteAndVote(const Block& proposal, Vote& vote) {
	ExecutedBlock executed_block = block_store_->executeAndAddBlock(proposal);
	if (hotstuff_core_->ConstructAndSignVote(executed_block, vote) == false)
		return false;
	block_store_->saveVote(vote);
	return true;
}

bool RoundManager::EnsureRoundAndSyncUp(
		Round round,
		const SyncInfo& sync_info,
		const Author& author) {
	if (round < round_state_->current_round()) {
		JLOG(journal_.error())
			<< "EnsureRoundAndSyncUp: Invalid round."
			<< "Round is " << round
			<< " and local round is " << round_state_->current_round();
		return false;
	}

	if (SyncUp(sync_info, author) != 0)
		return false;

	if (round != round_state_->current_round()) {
		JLOG(journal_.error())
			<< "After sync, round " << round 
			<< " dosen't match local round " << round_state_->current_round();
		assert(round == round_state_->current_round());
		return false;
	}
	return true;
}

int RoundManager::SyncUp(
	const SyncInfo& sync_info,
	const Author& author) {
	SyncInfo local_sync_info = block_store_->sync_info();

	// TODO
	// if local_sync_info is newer than sync_info, 
	// then local send local_sync_info to remote peer


	if (sync_info.hasNewerCertificate(local_sync_info)) {
		JLOG(journal_.debug())
			<< "local sync info is stale than remote stale";

		if (const_cast<SyncInfo&>(sync_info).Verify(hotstuff_core_->epochState()->verifier) == false) {
			JLOG(journal_.error())
				<< "Verifing sync_info failed";
			return 1;
		}
		block_store_->addCerts(sync_info);
		// open a new round
		ProcessCertificates();
	}

	return 0;
}

int RoundManager::ProcessCertificates() {
	SyncInfo sync_info = block_store_->sync_info();
	boost::optional<NewRoundEvent> new_round_event = round_state_->ProcessCertificates(sync_info);
	if (new_round_event) {
		ProcessNewRoundEvent(new_round_event.get());
	}
	return 0;
}

int RoundManager::NewQCAggregated(const QuorumCertificate& quorumCert) {
	if (block_store_->insertQuorumCert(quorumCert) == 0) {
		ProcessCertificates();
	}
	return 0;
}

} // namespace hotstuff
} // namespace ripple