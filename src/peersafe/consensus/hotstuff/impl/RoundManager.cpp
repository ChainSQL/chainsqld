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
}

int RoundManager::ProcessNewRoundEvent(const NewRoundEvent& new_round_event) {
	JLOG(journal_.info())
		<< "Process new round event: " << new_round_event.round;

	// setup round timeout
	boost::asio::steady_timer& roundTimeoutTimer = round_state_->RoundTimeoutTimer();
	roundTimeoutTimer.expires_from_now(std::chrono::seconds(config_.timeout));
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
		<< "ProcessLocalTimeout in round " << round;

	if (round != round_state_->current_round()) {
		JLOG(journal_.error())
			<< "Invalid round when processing local timeout."
			<< "Mismatch round: round in local timeout must be equal current round,"
			<< "but they wasn't. The round in local timeout is " << round
			<< " and current round is " << round_state_->current_round();
		return;
	}

	Vote timeout_vote;
	if (round_state_->send_vote()) {
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

	// setup round timeout
	boost::asio::steady_timer& roundTimeoutTimer = round_state_->RoundTimeoutTimer();
	roundTimeoutTimer.expires_from_now(std::chrono::seconds(config_.timeout));
	roundTimeoutTimer.async_wait(
		std::bind(&RoundManager::ProcessLocalTimeout, this, std::placeholders::_1, round));
}

bool RoundManager::CheckProposal(const Block& proposal, const SyncInfo& sync_info) {
	if (stop_)
		return false;

    JLOG(journal_.info())
        << "Check a proposal: " << proposal.block_data().round;

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

int RoundManager::ProcessProposal(const Block& proposal) {
	JLOG(journal_.info())
		<< "Process a proposal: " << proposal.block_data().round;

	if (proposer_election_->IsValidProposal(proposal) == false) {
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
	round_state_->recordVote(vote);
	network_->sendVote(next_leader, vote, block_store_->sync_info());
	return 0;
}

int RoundManager::ProcessVote(const Vote& vote, const SyncInfo& sync_info) {
	if (stop_)
		return 1;

    JLOG(journal_.info())
        << "Process a vote: " << vote.vote_data().proposed().round;

	if (EnsureRoundAndSyncUp(
		vote.vote_data().proposed().round,
		sync_info,
		vote.author()) == false) {
		JLOG(journal_.error())
			<< "Stale vote, current round "
			<< round_state_->current_round();
		return 1;
	}
	return ProcessVote(vote);
}

int RoundManager::ProcessVote(const Vote& vote) {
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
		Round next_round = vote.vote_data().proposed().round + 1;
		if (proposer_election_->IsValidProposer(proposal_generator_->author(), next_round) == false) {
			JLOG(journal_.warn())
				<< "Received a vote, but i am not a valid proposer for this round "
				<< next_round << ", ignore.";
			return 1;
		}
	}

	// TODO
	// Get QC from block storage

	// Add the vote and check whether it completes a new QC or a TC
	QuorumCertificate quorumCert;
	boost::optional<TimeoutCertificate> timeoutCert;
	int ret = round_state_->insertVote(
		vote,
		hotstuff_core_->epochState()->verifier,
		quorumCert, timeoutCert);
	if (ret == PendingVotes::VoteReceptionResult::NewQuorumCertificate &&
        block_store_->onQCAggregated(quorumCert)) {
		NewQCAggregated(quorumCert);
		return 0;
	}
	else if (ret == PendingVotes::VoteReceptionResult::NewTimeoutCertificate) {
		assert(timeoutCert);
		NewTCAggregated(timeoutCert.get());
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
			<< "The round for vote is " << vote.vote_data().proposed().round;
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
		JLOG(journal_.debug())
			<< "local sync info is stale than remote stale";

		if (const_cast<SyncInfo&>(sync_info).Verify(hotstuff_core_->epochState()->verifier) == false) {
			JLOG(journal_.error())
				<< "Verifing sync_info failed";
			return 1;
		}
		if (block_store_->state_compute()->syncState(
			sync_info.HighestQuorumCert().certified_block()) == false) {
			JLOG(journal_.error())
				<< "Sync compute state failed.";
			return 1;
		}
		block_store_->addCerts(sync_info, network_);
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
	if (block_store_->insertQuorumCert(quorumCert, network_) == 0) {
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

} // namespace hotstuff
} // namespace ripple