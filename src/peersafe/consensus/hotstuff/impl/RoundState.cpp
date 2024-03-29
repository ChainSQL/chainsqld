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

#include <peersafe/consensus/hotstuff/impl/RoundState.h>
#include <peersafe/consensus/hotstuff/impl/ValidatorVerifier.h>

#include <ripple/basics/Log.h>

namespace ripple {
namespace hotstuff {

RoundState::RoundState(
	boost::asio::io_service* io_service,
	beast::Journal journal)
: journal_(journal)
, current_round_(0)
, round_timeout_timer_(*io_service)
, generate_proposal_timeout_timer_(*io_service)
, pending_votes_(nullptr)
, send_vote_()
, offset_round_(0) {

}

RoundState::~RoundState() {

}

boost::optional<NewRoundEvent> RoundState::ProcessCertificates(const SyncInfo& sync_info) {
	Round new_round = sync_info.HighestRound() + 1;

	JLOG(journal_.info())
		<< "Openning a new round which is " << new_round
		<< ", current round is " << current_round_;

	if (new_round > current_round_) {
		// reset timeout
		CancelRoundTimeout();
		CancelGenerateProposalimeout();

		current_round_ = new_round;
		pending_votes_ = PendingVotes::New();
		send_vote_ = boost::optional<Vote>();

		NewRoundEvent new_round_event;
		new_round_event.reason = NewRoundEvent::QCRead;
		new_round_event.round = current_round_;

		resetOffsetRound();

		return boost::optional<NewRoundEvent>(new_round_event);
	}

	JLOG(journal_.error())
		<< "Opened a new round failed, new round is "
		<< new_round 
		<< ", but current round is " << current_round_;
	return boost::optional<NewRoundEvent>();
}

void RoundState::CancelRoundTimeout() {
	for (;;) {
		if (round_timeout_timer_.cancel() == 0)
			break;
	}
}

void RoundState::CancelGenerateProposalimeout() {
	for (;;) {
		if (generate_proposal_timeout_timer_.cancel() == 0)
			break;
	}
}

int RoundState::insertVote(
	const Vote& vote, 
	ValidatorVerifier* verifer, 
	QuorumCertificate& quorumCertResult,
	boost::optional<TimeoutCertificate>& timeoutCertResult) {
	Round round = current_round();
	if (vote.vote_data().proposed().round != round) {
		JLOG(journal_.error())
			<< "insert vote failed. reason: expect round is "
			<< vote.vote_data().proposed().round
			<< " but current round is " << round;
		return 1;
	}
	return pending_votes_->insertVote(
		vote, 
		verifer, 
		quorumCertResult,
		timeoutCertResult);
}

std::size_t RoundState::cacheVote(const Vote& vote) {
	Round round = current_round();
	if (vote.vote_data().proposed().round != round) {
		JLOG(journal_.error())
			<< "cache a vote failed. reason: expecte round is "
			<< vote.vote_data().proposed().round
			<< " but current round is " << round;
		return 0;
	}
	return pending_votes_->cacheVote(vote);
}

std::size_t RoundState::getAndRemoveCachedVotes(const HashValue& id, PendingVotes::Votes& votes) {
	return pending_votes_->getAndRemoveCachedVotes(id, votes);
}

void RoundState::reset() {
	current_round_ = 0;
	pending_votes_ = PendingVotes::New();
	send_vote_ = boost::optional<Vote>();
	resetOffsetRound();
}

void RoundState::increaseOffsetRound() {
	offset_round_.fetch_add(1);
}

Round RoundState::getOffsetRound() {
	return offset_round_;
}

void RoundState::resetOffsetRound() {
	offset_round_.exchange(0);
}

} // namespace hotstuff
} // namespace ripple