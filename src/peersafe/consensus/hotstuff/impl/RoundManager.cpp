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
	beast::Journal journal,
	const Config& config,
	BlockStorage* block_store,
	RoundState* round_state,
	HotstuffCore* hotstuff_core,
	ProposalGenerator* proposal_generator,
	ProposerElection* proposer_election,
	NetWork* network)
: journal_(journal)
, config_(config)
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
	stop_ = false;
	//assert(stop_ == false);
	// open a new round
	boost::optional<NewRoundEvent> new_round_event = round_state_->ProcessCertificates(block_store_->sync_info());
	if (new_round_event) {
		return ProcessNewRoundEvent(new_round_event.get(), 0);
	}
	return 1;
}

void RoundManager::stop() {
	if (stop_ == true)
		return;

	stop_ = true;

	for (;;) {
		if (round_state_->RoundTimeoutTimer().cancel() == 0)
			break;
	}
}

int RoundManager::ProcessNewRoundEvent(const NewRoundEvent& new_round_event, const Round& shift) {
	JLOG(journal_.info())
		<< "Process new round event: " << new_round_event.round;

	// setup round timeout
	boost::asio::steady_timer& roundTimeoutTimer = round_state_->RoundTimeoutTimer();
	roundTimeoutTimer.expires_from_now(std::chrono::seconds(config_.timeout));
	roundTimeoutTimer.async_wait(
		std::bind(&RoundManager::ProcessLocalTimeout, this, std::placeholders::_1, new_round_event.round));

    // open a new ledger round
    if (!block_store_->state_compute()->syncState(block_store_->sync_info().HighestQuorumCert().certified_block()))
    {
        JLOG(journal_.warn()) << "Sync ledger round failed";
        return 1;
    }

	if (!IsValidProposer(proposal_generator_->author(), new_round_event.round + shift)) {
		JLOG(journal_.error())
			<< "ProcessNewRoundEvent: invalidProposel."
			<< " New round is " << new_round_event.round
			<< " and proposal's author is " << proposal_generator_->author();
		return 1;
	}
    JLOG(journal_.info())
        << "ProcessNewRoundEvent: New round is " << new_round_event.round
        << " and I am the proposer";
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

/// The replica broadcasts a "timeout vote message", which includes the round signature, which
/// can be aggregated to a TimeoutCertificate.
/// The timeout vote message can be one of the following three options:
/// 1) In case a validator has previously voted in this round, it repeats the same vote and sign
/// a timeout.
/// 2) Otherwise vote for a NIL block and sign a timeout.
void RoundManager::ProcessLocalTimeout(const boost::system::error_code& ec, Round round) {
	if (ec)
		return;

	JLOG(journal_.info())
		<< "An author " << proposal_generator_->author()
		<< " processes localTimeout in round " << round
		<< ", shift " << round_state_->getShiftRoundToNextLeader();

	if (round != round_state_->current_round()) {
		JLOG(journal_.error())
			<< "Invalid round when processing local timeout."
			<< "Mismatch round: round in local timeout must be equal current round,"
			<< "but they wasn't. The round in local timeout is " << round
			<< " and current round is " << round_state_->current_round()
			<< ". The author is " << proposal_generator_->author();
		return;
	}
	
	if (config_.disable_nil_block) {
		NotUseNilBlockProcessLocalTimeout(round);
	}
	else {
		UseNilBlockProcessLocalTimeout(round);
	}

	//// setup round timeout
	boost::asio::steady_timer& roundTimeoutTimer = round_state_->RoundTimeoutTimer();
	roundTimeoutTimer.expires_from_now(std::chrono::seconds(config_.timeout));
	roundTimeoutTimer.async_wait(
		std::bind(&RoundManager::ProcessLocalTimeout, this, std::placeholders::_1, round));
}

bool RoundManager::CheckProposal(const Block& proposal, const SyncInfo& sync_info) {
	if (stop_)
		return false;

    JLOG(journal_.info())
        << "Check a proposal: " << proposal.block_data().round
        << ", seq: " << proposal.block_data().getLedgerInfo().seq;

	if (EnsureRoundAndSyncUp(
		proposal.block_data().round,
		sync_info,
		proposal.block_data().author()) == false) {
		JLOG(journal_.error())
			<< "Stale proposal, current round " 
			<< round_state_->current_round();
		return false;
	}


	const boost::optional<Signature>& signature = proposal.signature();
	if (signature) {
		if (hotstuff_core_->epochState()->verifier->verifySignature(
			proposal.block_data().author(), signature.get(), proposal.id()) == false) {
			JLOG(journal_.error())
				<< "CheckProposal: using an author "
				<< proposal.block_data().author()
				<< "'s key for verifing signature failed";
			return false;
		}
	}

	return true;
}

int RoundManager::ProcessProposal(const Block& proposal, const Round& shift /*= 0*/) {
	JLOG(journal_.info())
		<< "Process a proposal: round: " << proposal.block_data().round
        << ", seq: " << proposal.block_data().getLedgerInfo().seq;

	if (IsValidProposal(proposal, shift) == false) {
		JLOG(journal_.error())
			<< "Proposer " << "" << proposal.block_data().author()
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
	JLOG(journal_.info())
		<< "Send the vote whose round is " << proposal_round
		<< " to next leader " << next_leader;
	round_state_->recordVote(vote);
	network_->sendVote(next_leader, vote, block_store_->sync_info());
	return 0;
}

int RoundManager::ProcessVote(const Vote& vote, const SyncInfo& sync_info, const Round& shift /*= 0*/) {
	if (stop_)
		return 1;

	Round round = vote.vote_data().proposed().round;

	JLOG(journal_.info())
		<< "Process a vote: round: " << round
        << ", seq: " << vote.vote_data().proposed().ledger_info.seq;

	if (EnsureRoundAndSyncUp(
		round,
		sync_info,
		vote.author()) == false) {
		JLOG(journal_.error())
			<< "Stale vote, current round "
			<< round_state_->current_round();
		return 1;
	}
	return ProcessVote(vote, shift);
}

int RoundManager::ProcessVote(const Vote& vote, const Round& shift /*= 0*/) {
	if (hotstuff_core_->epochState()->verifier->verifySignature(
		vote.author(), vote.signature(), vote.ledger_info().consensus_data_hash) == false)
	{
		JLOG(journal_.error())
			<< "An anutor " << vote.author()
			<< " voted a vote mismatch signature."
			<< "The round for vote is "
			<< vote.vote_data().proposed().round;
		return 1;
	}

	if (vote.isTimeout() == false) {
		assert(shift == 0);
		Round next_round = vote.vote_data().proposed().round + 1;
		if (IsValidProposer(proposal_generator_->author(), next_round) == false) {
			JLOG(journal_.warn())
				<< "Received a vote, but i am not a valid proposer for this round "
				<< next_round << ", ignore.";
			return 1;
		}
	}

	// TODO
	// Get QC from block storage

	// Add the vote and check whether it completes a new QC or a TC
	PendingVotes::QuorumCertificateResult quorumCertResult;
	boost::optional<PendingVotes::TimeoutCertificateResult> timeoutCertResult;
	int ret = round_state_->insertVote(
		vote,
		shift,
		hotstuff_core_->epochState()->verifier,
		quorumCertResult, 
		timeoutCertResult);
	if (ret == PendingVotes::VoteReceptionResult::NewQuorumCertificate) {
		JLOG(journal_.info())
			<< "Collected enough votes for the round " << vote.vote_data().proposed().round
			<< ", shift " << std::get<0>(quorumCertResult)
			<< ". A new round " << (round_state_->current_round() + 1)
			<< " will open.";
		NewQCAggregated(
			std::get<1>(quorumCertResult),
			vote.author(),
			std::get<0>(quorumCertResult));
		return 0;
	}
	else if (ret == PendingVotes::VoteReceptionResult::NewTimeoutCertificate) {
		assert(timeoutCertResult);
		NewTCAggregated(std::get<1>(timeoutCertResult.get()));
		return 0;
	}
    else if (ret == PendingVotes::VoteReceptionResult::VoteAdded)
    {
        JLOG(journal_.info()) << "vote added";
        return 0;
    }
	return 1;
}

/// The function generates a VoteMsg for a given proposed_block:
/// * first execute the block and add it to the block store
/// * then verify the voting rules
bool RoundManager::ExecuteAndVote(const Block& proposal, Vote& vote) {
	ExecutedBlock executed_block = block_store_->executeAndAddBlock(proposal);
	if (hotstuff_core_->ConstructAndSignVote(executed_block, vote) == false) {
		JLOG(journal_.error())
			<< "Construct and sign vote failed."
			<< "The round for vote is " << proposal.block_data().round;
		return false;
	}
	return true;
}

bool RoundManager::EnsureRoundAndSyncUp(
		Round round,
		const SyncInfo& sync_info,
		const Author& author) {
	Round current_round = round_state_->current_round();
	if (round < current_round) {
		JLOG(journal_.error())
			<< "EnsureRoundAndSyncUp: Invalid round."
			<< "Round is " << round
			<< " and local round is " << current_round;
		return false;
	}

	if (SyncUp(sync_info, author) != 0)
		return false;

	current_round = round_state_->current_round();
	if (round != current_round) {
		JLOG(journal_.error())
			<< "After sync, round " << round 
			<< " dosen't match local round " << current_round;
		assert(round == current_round);
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
		JLOG(journal_.info())
			<< "local sync info is stale than remote stale";

		if (const_cast<SyncInfo&>(sync_info).Verify(hotstuff_core_->epochState()->verifier) == false) {
			JLOG(journal_.error())
				<< "Verifing sync_info failed";
			return 1;
		}

		block_store_->addCerts(sync_info, author, network_);

		// open a new round
		ProcessCertificates();
	}

	return 0;
}

int RoundManager::ProcessCertificates(const Round& shift /*= 0*/) {
	SyncInfo sync_info = block_store_->sync_info();
	boost::optional<NewRoundEvent> new_round_event = round_state_->ProcessCertificates(sync_info);
	if (new_round_event) {
		ProcessNewRoundEvent(new_round_event.get(), shift);
	}
	return 0;
}

int RoundManager::NewQCAggregated(
	const QuorumCertificate& quorumCert, 
	const Author& author, 
	const Round& shift) {
	if (block_store_->insertQuorumCert(quorumCert, author, network_) == 0) {
		ProcessCertificates(shift);
	}
	return 0;
}

int RoundManager::NewTCAggregated(const TimeoutCertificate& timeoutCert) {
	if (block_store_->insertTimeoutCert(timeoutCert) == 0) {
		ProcessCertificates();
	}
	return 0;
}

void RoundManager::UseNilBlockProcessLocalTimeout(const Round& round) {
	Vote timeout_vote;
	if (round_state_->send_vote()
		&& round_state_->send_vote()->vote_data().parent().round > 0) {
		timeout_vote = round_state_->send_vote().get();
	}
	else {
		// generates a dummy block
		// Didn't vote in this round yet, generate a backup vote
		boost::optional<Block> nil_block = proposal_generator_->GenerateNilBlock(round);
		if (nil_block) {
			assert(nil_block->block_data().block_type == BlockData::NilBlock);
			if (ExecuteAndVote(nil_block.get(), timeout_vote) == false) {
				JLOG(journal_.error())
					<< "Execute and vote failed for the proposal";
				return;
			}
		}
		else {
			JLOG(journal_.error())
				<< "Generate NilBlock failed when processing localtimeout";
			return;
		}
	}

	if (timeout_vote.isTimeout() == false) {
		Timeout timeout = timeout_vote.timeout();
		Signature signature;
		if (timeout.sign(hotstuff_core_->epochState()->verifier, signature))
			timeout_vote.addTimeoutSignature(signature);
	}
	round_state_->recordVote(timeout_vote);
	// broadcast vote
	network_->broadcast(timeout_vote, block_store_->sync_info());
}

void RoundManager::NotUseNilBlockProcessLocalTimeout(const Round& round) {
	round_state_->shiftRoundToNextLeader();
	Round shift_round = round_state_->getShiftRoundToNextLeader();

	if (round_state_->send_vote()) {
		Vote timeout_vote = round_state_->send_vote().get();
		if (timeout_vote.isTimeout() == false) {
			Timeout timeout = timeout_vote.timeout();
			Signature signature;
			if (timeout.sign(hotstuff_core_->epochState()->verifier, signature))
				timeout_vote.addTimeoutSignature(signature);
		}
		round_state_->recordVote(timeout_vote);

		Author next_leader = proposer_election_->GetValidProposer(round + 1 + shift_round);
		JLOG(journal_.info())
			<< "Switch send current vote whoes round is " << round
			<< " to next leader " << next_leader;
		round_state_->recordVote(timeout_vote);
		if (next_leader == proposal_generator_->author()) {
			ProcessVote(timeout_vote, shift_round);
		}
		else {
			network_->sendVote(
				next_leader, 
				timeout_vote, 
				block_store_->sync_info(),
				shift_round);
		}
	}
	else {
		Round next_round = round + shift_round;
		if (!IsValidProposer(proposal_generator_->author(), next_round)) {
			return;
		}

		boost::optional<BlockData> proposal = proposal_generator_->Proposal(round);
		if (!proposal) {
			return;
		}
		Block block = hotstuff_core_->SignProposal(proposal.get());
		network_->broadcast(block, block_store_->sync_info(), shift_round);
	}
}

bool RoundManager::IsValidProposer(const Author& author, const Round& round) {
	return proposer_election_->IsValidProposer(author, round);
}

bool RoundManager::IsValidProposal(const Block& proposal, const Round& shift /*= 0*/) {
	Block block = proposal;
	block.block_data().round = block.block_data().round + shift;
	return proposer_election_->IsValidProposal(block);
}

void RoundManager::HandleSyncBlockResult(
	const HashValue& hash,
	const ExecutedBlock& block) {

	Round proposal_round = block.block.block_data().round;
	Vote vote;
	if (hotstuff_core_->ConstructAndSignVote(block, vote) == false) {
		JLOG(journal_.error())
			<< "Construct and sign vote failed in HandleSyncBlockResult."
			<< "The round for vote is " << proposal_round;
		return;
	}
	
	Author next_leader = proposer_election_->GetValidProposer(proposal_round + 1);
	round_state_->recordVote(vote);
	network_->sendVote(next_leader, vote, block_store_->sync_info());
}

// Get an expected block
bool RoundManager::expectBlock(
	const HashValue& block_id,
	ExecutedBlock& executed_block) {
	return block_store_->safetyBlockOf(block_id, executed_block);
}

bool RoundManager::unsafetyExpectBlock(
	const HashValue& block_id,
	ExecutedBlock& executed_block) {
	return block_store_->blockOf(block_id, executed_block);
}

} // namespace hotstuff
} // namespace ripple