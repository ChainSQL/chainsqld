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

#ifndef RIPPLE_CONSENSUS_HOTSTUFF_ROUNDSTATE_H
#define RIPPLE_CONSENSUS_HOTSTUFF_ROUNDSTATE_H

#include <atomic>
#include <map>

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/optional.hpp>

#include <peersafe/consensus/hotstuff/impl/Types.h>
#include <peersafe/consensus/hotstuff/impl/SyncInfo.h>
#include <peersafe/consensus/hotstuff/impl/PendingVotes.h>

namespace ripple {
namespace hotstuff {

struct NewRoundEvent {
	enum Reason {
		QCRead,
		Timeout,
	};
	Round round;
	Reason reason;
};

class ValidatorVerifier;

class RoundState {
public:
	RoundState(
		boost::asio::io_service* io_service,
		beast::Journal journal);
	~RoundState();
	
	boost::optional<NewRoundEvent> ProcessCertificates(const SyncInfo& sync_info);
	
	boost::asio::steady_timer& RoundTimeoutTimer() {
		return round_timeout_timer_;
	}

	boost::asio::steady_timer& GenerateProposalTimeoutTimer() {
		return generate_proposal_timeout_timer_;
	}

	Round current_round() const {
		return current_round_;
	}

	void recordVote(const Vote& vote) {
		send_vote_ = vote;
	}

	const boost::optional<Vote>& send_vote() const {
		return send_vote_;
	}

	int insertVote(
		const Vote& vote, 
		ValidatorVerifier* verifer, 
		QuorumCertificate& quorumCertResult,
		boost::optional<TimeoutCertificate>& timeoutCertResult);

	std::size_t cacheVote(const Vote& vote);
	std::size_t getAndRemoveCachedVotes(const HashValue& id, PendingVotes::Votes& votes);

	void reset();

	void increaseOffsetRound();
	Round getOffsetRound();
	void resetOffsetRound();
private:
	void CancelRoundTimeout();
	void CancelGenerateProposalimeout();

	beast::Journal journal_;
	std::atomic<Round> current_round_;
	boost::asio::steady_timer round_timeout_timer_;
	boost::asio::steady_timer generate_proposal_timeout_timer_;
	PendingVotes::pointer pending_votes_;
	boost::optional<Vote> send_vote_;
	std::atomic<Round> offset_round_;
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_ROUNDSTATE_H