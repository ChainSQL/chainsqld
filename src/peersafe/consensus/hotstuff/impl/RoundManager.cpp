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

int
RoundManager::start(bool validating)
{
    JLOG(journal_.info()) << "start/restared. validating: " << validating
                          << " Self is "
                          << toBase58(
                                 TokenType::NodePublic,
                                 proposal_generator_->author());

	stop_ = false;
    validating_ = validating;
	//assert(stop_ == false);
	// open a new round
	boost::optional<NewRoundEvent> new_round_event = round_state_->ProcessCertificates(block_store_->sync_info());
	if (new_round_event) {
		return ProcessNewRoundEvent(new_round_event.get());
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

	JLOG(journal_.info())
            << "stopped. Self is "
            << toBase58(TokenType::NodePublic, proposal_generator_->author());
}

int RoundManager::ProcessNewRoundEvent(const NewRoundEvent& new_round_event) {
	JLOG(journal_.info())
		//<< "The author " << proposal_generator_->author()
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

	if (!IsValidProposer(proposal_generator_->author(), new_round_event.round)) {
		JLOG(journal_.info())
			<< "ProcessNewRoundEvent: New round is " << new_round_event.round
			<< " and I am a voter";
		return 1;
	}
    JLOG(journal_.info())
        << "ProcessNewRoundEvent: New round is " << new_round_event.round
        << " and I am the proposer";

	GenerateThenBroadCastProposalInTimer(new_round_event);

	return 0;
}

boost::optional<Block> RoundManager::GenerateProposal(const NewRoundEvent& event) {
	boost::optional<BlockData> proposal = proposal_generator_->Proposal(event.round);
	if (proposal) {
		return hotstuff_core_->SignProposal(proposal.get());
	}

	return boost::optional<Block>();
}

void RoundManager::GenerateThenBroadCastProposalInTimer(const NewRoundEvent& event) {
	if (proposal_generator_->canExtract()) {
		boost::optional<Block> proposal = GenerateProposal(event);
		if (proposal) {
			JLOG(journal_.info())
				//<< "The author " << proposal_generator_->author()
				<< "Broadcast a proposal whose round is " << proposal.get().block_data().round;

			network_->broadcast(proposal.get(), block_store_->sync_info());
		}
	}
	else {
        JLOG(journal_.debug()) << "delay to generate a proposal for round "
                                << event.round;
		boost::asio::steady_timer& timer = round_state_->GenerateProposalTimeoutTimer();
		timer.expires_from_now(std::chrono::milliseconds(config_.interval_extract));
		timer.async_wait(
			std::bind(&RoundManager::GenerateThenBroadCastProposal, this, std::placeholders::_1, event));
	}
}

void RoundManager::GenerateThenBroadCastProposal(const boost::system::error_code& ec, NewRoundEvent event) {
	if (ec)
		return;
	GenerateThenBroadCastProposalInTimer(event);
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
		//<< "An author " << proposal_generator_->author()
		<< "Processes localTimeout in round " << round;

	if (round != round_state_->current_round()) {
		JLOG(journal_.error())
			<< "Invalid round when processing local timeout."
			<< "Mismatch round: round in local timeout must be equal current round,"
			<< "but they wasn't. The round in local timeout is " << round
			<< " and current round is " << round_state_->current_round();
			//<< ". The author is " << proposal_generator_->author();
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
        << "Check a proposal: round: " << proposal.block_data().round
        << ", seq: " << proposal.block_data().getLedgerInfo().seq;

	HashValue hqc = sync_info.HighestQuorumCert().certified_block().id;

	if (hqc.isNonZero() && block_store_->existsBlock(hqc) == false) {

		if (preCheck(proposal.block_data().round, sync_info) == false)
			return false;

		block_store_->asyncExpectBlock(
			hqc, 
			proposal.block_data().author(),
			[this, hqc, proposal, sync_info](const StateCompute::AsyncBlockErrorCode ec, 
				const ExecutedBlock& executed_block, 
				boost::optional<Block>& checked_proposal) {

				if (ec != StateCompute::AsyncBlockErrorCode::ASYNC_SUCCESS) {
					JLOG(journal_.error())
						<< "Async block whose id is " << hqc << " was failure. ErrorCode is " << ec;
					return  StateCompute::AsyncBlockResult::UNKOWN_ERROR;
				}

				block_store_->addExecutedBlock(executed_block);
				checked_proposal = boost::none;
				if (preProcessProposal(proposal, sync_info) == 0) {
					checked_proposal = proposal;
					return StateCompute::AsyncBlockResult::PROPOSAL_SUCCESS;
				}
				return StateCompute::AsyncBlockResult::PROPOSAL_FAILURE;
			});
		return false;
	}

	return preProcessProposal(proposal, sync_info) == 0 ? true : false;
}

bool RoundManager::preCheck(const Round round, const SyncInfo& sync_info) {
	Round current_round = round_state_->current_round();
    if (round < current_round)
    {
        JLOG(journal_.error())
            << "preCheck: Invalid round."
            << "Round is " << round << " and local round is " << current_round
            << ". Self is "
            << toBase58(TokenType::NodePublic, proposal_generator_->author());
        return false;
    }

	SyncInfo local_sync_info = block_store_->sync_info();
	if (sync_info.hasNewerCertificate(local_sync_info)) {
		JLOG(journal_.debug())
			<< "preCheck: local sync info is stale than remote";

		if (const_cast<SyncInfo&>(sync_info).Verify(hotstuff_core_->epochState()->verifier) == false) {
			JLOG(journal_.error())
				<< "preCheck: Verifing sync_info failed";
			return false;
		}
	}
	return true;
}

int RoundManager::preProcessProposal(
	const Block& proposal, 
	const SyncInfo& sync_info) {

	if (EnsureRoundAndSyncUp(
		proposal.block_data().round,
		sync_info,
		proposal.block_data().author()) == false) {
		JLOG(journal_.warn())
			<< "Stale proposal, current round "
			<< round_state_->current_round();
		return 1;
	}

	const boost::optional<Signature>& signature = proposal.signature();
	if (signature) {
		if (hotstuff_core_->epochState()->verifier->verifySignature(
			proposal.block_data().author(), signature.get(), proposal) == false) {
			JLOG(journal_.error())
				<< "CheckProposal: using an author "
				<< proposal.block_data().author()
				<< "'s key for verifing signature failed";
			return 1;
		}
	}

	return 0;
}

int RoundManager::ProcessProposal(const Block& proposal) {
	JLOG(journal_.info())
		<< "Process a proposal: round: " << proposal.block_data().round
        << ", seq: " << proposal.block_data().getLedgerInfo().seq;

	if (IsValidProposal(proposal) == false) {
            JLOG(journal_.error())
                << "Proposer "
                << toBase58(
                       TokenType::NodePublic, proposal.block_data().author())
                << " for the proposal"
                << " is a invalid proposer for this round "
                << proposal.block_data().round;
		return 1;
	}

	Round proposal_round = proposal.block_data().round;
	Vote vote;
    if (ExecuteAndVote(proposal, vote) == false) {
        if (validating_)
        {
            JLOG(journal_.error())
                << "Execute and vote failed for the proposal";
        }
        return 1;
    }

	Author next_leader = proposer_election_->GetValidProposer(proposal_round + 1);
	JLOG(journal_.info())
		//<< "The author " << proposal_generator_->author()
		<< "Send the vote whose round is " << proposal_round
		//<< " and author is " << proposal.block_data().author()
		<< " to next leader " << toBase58(TokenType::NodePublic, next_leader);
	round_state_->recordVote(vote);
	network_->sendVote(next_leader, vote, block_store_->sync_info());

	return 0;
}

int RoundManager::ProcessVote(const Vote& vote, const SyncInfo& sync_info) {
	if (stop_)
		return 1;

	Round round = vote.vote_data().proposed().round;
	JLOG(journal_.info())
		<< "Process a vote: round: " << round
        << ", seq: " << vote.vote_data().proposed().ledger_info.seq;

	HashValue hqc = sync_info.HighestQuorumCert().certified_block().id;

	if (hqc.isNonZero() && block_store_->existsBlock(hqc) == false) {

		if (preCheck(vote.vote_data().proposed().round, sync_info) == false)
			return false;

		block_store_->asyncExpectBlock(hqc, vote.author(),
			[this, hqc, vote, sync_info](const StateCompute::AsyncBlockErrorCode ec,
				const ExecutedBlock& executed_block, boost::optional<Block>& proposal) {

					if (ec != StateCompute::AsyncBlockErrorCode::ASYNC_SUCCESS) {
						JLOG(journal_.error())
							<< "Async block whose id is " << hqc << " was failure. ErrorCode is " << ec;
						return  StateCompute::AsyncBlockResult::UNKOWN_ERROR;
					}

					block_store_->addExecutedBlock(executed_block);
					proposal = boost::none;
					if (preProcessVote(vote, sync_info) == 0)
						return StateCompute::AsyncBlockResult::VOTE_SUCCESS;

					return StateCompute::AsyncBlockResult::VOTE_FAILURE;
			});
		// asynchronously process
		return -1;
	}

	return preProcessVote(vote, sync_info);
}

int RoundManager::preProcessVote(const Vote& vote, const SyncInfo& sync_info) {

	if (EnsureRoundAndSyncUp(
		vote.vote_data().proposed().round,
		sync_info,
		vote.author()) == false) {
		JLOG(journal_.info())
			<< "Stale vote, current round "
			<< round_state_->current_round();
		return 1;
	}

	if (hotstuff_core_->epochState()->verifier->verifySignature(
		vote.author(), vote.signature(), vote) == false)
	{
		JLOG(journal_.error())
			<< "An anutor " << vote.author()
			<< " voted a vote mismatch signature."
			<< "The round for vote is "
			<< vote.vote_data().proposed().round;
		return 1;
	}

	HashValue proposed_id = vote.vote_data().proposed().id;
	if (ReceivedProposedBlock(proposed_id) == false) {
		AddVoteToCache(vote);
		return 0;
	}
	else {
		HandleCacheVotes(proposed_id);
	}

	return ProcessVote(vote);
}

int RoundManager::ProcessVote(const Vote& vote) {
	Round round = vote.vote_data().proposed().round;
	if (vote.isTimeout() == false) {
		if (IsValidProposer(vote) == false) {
			JLOG(journal_.warn())
				//<< "The author " << proposal_generator_->author()
				<< "Received a vote, but i am not a valid proposer for this round "
				<< (round + 1) << ", ignore.";
			return 1;
		}
	}

	// Add the vote and check whether it completes a new QC or a TC
	QuorumCertificate quorumCertResult;
	boost::optional<TimeoutCertificate> timeoutCertResult;
	int ret = round_state_->insertVote(
		vote,
		hotstuff_core_->epochState()->verifier,
		quorumCertResult, 
		timeoutCertResult);
	if (ret == PendingVotes::VoteReceptionResult::NewQuorumCertificate) {
		JLOG(journal_.info())
			//<< "The author " << proposal_generator_->author()
			<< "Collected enough votes for the round " << vote.vote_data().proposed().round
			<< ". A new round " << (round_state_->current_round() + 1)
			<< " will open.";
		NewQCAggregated(quorumCertResult, vote.author());
		return 0;
	}
	else if (ret == PendingVotes::VoteReceptionResult::NewTimeoutCertificate) {
		assert(timeoutCertResult);
		NewTCAggregated(timeoutCertResult.get());
		return 0;
	}
    else if (ret == PendingVotes::VoteReceptionResult::VoteAdded)
    {
        JLOG(journal_.info()) << "vote added";
        return 0;
    }
	else {
		JLOG(journal_.warn())
			//<< "The author " << proposal_generator_->author()
			<< "Inserted a vote failed, the round is " << round
			<< ", the author of vote is "
            << toBase58(TokenType::NodePublic, vote.author())
			<< ", Is timeout vote ? " << vote.isTimeout()
			<< ". Hash of ledger_info " << vote.ledger_info().consensus_data_hash
			<< ". Insert result is " << ret;
	}

	return 1;
}

bool RoundManager::CheckEpochChange(
	const ripple::hotstuff::EpochChange& epoch_change,
	const ripple::hotstuff::SyncInfo& sync_info) {

	if (epoch_change.author == proposal_generator_->author())
		return true;

	if (epoch_change.epoch != hotstuff_core_->epochState()->epoch) {
		JLOG(journal_.error())
			<< "Mismatch epoch in epoch_change in checkEpochChange.";
		assert(false);
		return false;
	}

	if (epoch_change.verify(hotstuff_core_->epochState()->verifier) == false) {
		JLOG(journal_.error())
			<< "verify epoch change failed in checkEpochChange.";
		assert(false);
		return false;
	}

	HashValue consensus_data_hash = epoch_change.ledger_info.ledger_info.consensus_data_hash;
	if (consensus_data_hash != sync_info.HighestCommitCert().ledger_info().ledger_info.consensus_data_hash
		&& consensus_data_hash != sync_info.HighestQuorumCert().ledger_info().ledger_info.consensus_data_hash) {
		JLOG(journal_.error())
			<< "Mismatch consensus hash in betwwen HQC and HCC in checkEpochChange.";
		assert(false);
		return false;
	}

	if (hotstuff_core_->epochState()->verifier->checkVotingPower(epoch_change.ledger_info.signatures) == false) {
		JLOG(journal_.error())
			<< "check vote power failed in checkEpochChange.";
		assert(false);
		return false;
	}

	if (EnsureRoundAndSyncUp(
		epoch_change.round + 1, 
		sync_info, 
		epoch_change.author) == false) {
		JLOG(journal_.error())
			<< "Stale epoch change, current round "
			<< round_state_->current_round()
			<< " in checkEpochChange";
		return false;
	}

	return true;
}

/// The function generates a VoteMsg for a given proposed_block:
/// * first execute the block and add it to the block store
/// * then verify the voting rules
bool RoundManager::ExecuteAndVote(const Block& proposal, Vote& vote) {
	ExecutedBlock executed_block = block_store_->executeAndAddBlock(proposal);
    if (!validating_)
    {
        JLOG(journal_.info())
            << "Don't vote for proposal, I'm not validating, round: "
            << proposal.block_data().round;
        return false;
    }
    if (hotstuff_core_->ConstructAndSignVote(executed_block, vote) == false)
    {
        JLOG(journal_.error())
            << "Construct and sign vote failed."
            << "The round for vote is " << proposal.block_data().round
            << ". Self is "
            << toBase58(TokenType::NodePublic, proposal_generator_->author());
        return false;
    }
	return true;
}

bool RoundManager::EnsureRoundAndSyncUp(
		Round round,
		const SyncInfo& sync_info,
		const Author& author) {
	Round current_round = round_state_->current_round();
    if (round < current_round)
    {
        JLOG(journal_.info())
            << "EnsureRoundAndSyncUp: Invalid round."
            << "Round is " << round << " and local round is " << current_round;
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
			<< "local sync info is stale than remote";

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

int RoundManager::ProcessCertificates() {
	SyncInfo sync_info = block_store_->sync_info();
	boost::optional<NewRoundEvent> new_round_event = round_state_->ProcessCertificates(sync_info);
	if (new_round_event) {
		ProcessNewRoundEvent(new_round_event.get());
	}
	return 0;
}

int RoundManager::NewQCAggregated(
	const QuorumCertificate& quorumCert, 
	const Author& author) {
	if (block_store_->insertQuorumCert(quorumCert, author, network_) == 0) {
		ProcessCertificates();
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
					<< "Execute and vote failed for the nil block whose round is "
					<< nil_block.get().block_data().round
					<< " in local timeout.";
				return;
			}
			JLOG(journal_.info())
				//<< "The author " << proposal_generator_->author()
				<< "Generates a nil block whose round is "
				<< nil_block.get().block_data().round
				<< " in local timeout.";
		}
		else {
			JLOG(journal_.error())
				<< "Generate NilBlock whose round is "
				<< nil_block.get().block_data().round 
				<< " failed when processing localtimeout";
			return;
		}
	}

	if (timeout_vote.isTimeout() == false) {
		Timeout timeout = timeout_vote.timeout();
		Signature signature;
		if (timeout.sign(hotstuff_core_->epochState()->verifier, signature))
			timeout_vote.addTimeoutSignature(signature);
	}

	JLOG(journal_.info())
		//<< "The author " << proposal_generator_->author()
		<< "Broadcasted a timeout vote whose round is "
		<< timeout_vote.vote_data().proposed().round;
	round_state_->recordVote(timeout_vote);
	// broadcast vote
	network_->broadcast(timeout_vote, block_store_->sync_info());
}

void RoundManager::NotUseNilBlockProcessLocalTimeout(const Round& round) {
	Vote timeout_vote;
	if (round_state_->send_vote()) {
		timeout_vote = round_state_->send_vote().get();

		if (timeout_vote.isTimeout() == false) {
			Timeout timeout = timeout_vote.timeout();
			Signature signature;
			if (timeout.sign(hotstuff_core_->epochState()->verifier, signature))
				timeout_vote.addTimeoutSignature(signature);
		}

		round_state_->recordVote(timeout_vote);
		// broadcast vote
		network_->broadcast(timeout_vote, block_store_->sync_info());

		JLOG(journal_.warn())
			<< "broadcast timout vote: "
			<< "epoch is " << timeout_vote.timeout().epoch
			<< ", round is " << timeout_vote.timeout().round;
	}
	else {

		round_state_->increaseOffsetRound();

		Author expect_next_author = proposer_election_->GetValidProposer(round + round_state_->getOffsetRound());
		JLOG(journal_.warn())
			<< "The next leader may be exception, "
			"so we should pick up a next leader for generating a proposal (round "
			<< round << ").";
		JLOG(journal_.warn())
			<< "We pick up a next leader is " << expect_next_author;
			//<< ", self is " << proposal_generator_->author();

		if (expect_next_author == proposal_generator_->author()) {
			NewRoundEvent round_event;
			round_event.reason = NewRoundEvent::Timeout;
			round_event.round = round;
			boost::optional<Block> proposal = GenerateProposal(round_event);
			if (proposal) {
				network_->broadcast(proposal.get(), block_store_->sync_info());
			}
		}
	}
}

bool RoundManager::IsValidProposer(const Vote& vote) {
	bool isValid = false;
	Round next_round = vote.vote_data().proposed().round;
	if (config_.disable_nil_block) {
		 next_round += round_state_->getOffsetRound();
	}
	else {
		next_round += 1;
	}
	isValid = IsValidProposer(proposal_generator_->author(), next_round);

	JLOG(journal_.debug())
		<< "Is valid proposer for the vote? " << isValid
		<< ". The Author of the vote is " << vote.author()
		<< ", The round of the vote is " << vote.vote_data().proposed().round
		//<< ". The self is " << proposal_generator_->author()
		<< " and the next round is " << next_round;

	return isValid;
}

bool RoundManager::IsValidProposer(const Author& author, const Round& round) {
	return proposer_election_->IsValidProposer(author, round);
}

bool RoundManager::IsValidProposal(const Block& proposal) {
	bool isValid = false;
	if (config_.disable_nil_block) {
		Round offsetRound = round_state_->getOffsetRound();
		Round propsal_round = proposal.block_data().round;
		//assert(offsetRound);
		Author expect_autor = proposer_election_->GetValidProposer(propsal_round + offsetRound);
		isValid = expect_autor == proposal.block_data().author();

		JLOG(journal_.info())
			<< "Is valid propsal? " << isValid
			<< ". The round of the propsal is " << propsal_round
			<< "and the author of the proposal is " << proposal.block_data().author()
			<< ". The offset round is " << offsetRound
			<< ". The expected author is " << expect_autor;
	}
	else {
		isValid = proposer_election_->IsValidProposal(proposal);
	}

	return isValid;
}

bool RoundManager::ReceivedProposedBlock(const HashValue& proposed_id) {
	ExecutedBlock block;
	return block_store_->safetyBlockOf(proposed_id, block);
}

void RoundManager::AddVoteToCache(const Vote& vote) {
	JLOG(journal_.warn())
		<< "Received a vote but it's proposal hasn't been received now, cache it."
		<< "Vote round is " << vote.vote_data().proposed().round
		<< " and author is " << toBase58(TokenType::NodePublic, vote.author());
	round_state_->cacheVote(vote);
}

void RoundManager::HandleCacheVotes(const HashValue& id) {
	PendingVotes::Votes votes;
	std::size_t size = round_state_->getAndRemoveCachedVotes(id, votes);
	for (std::size_t i = 0; i < size; i++) {
		ProcessVote(votes[i]);
	}
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