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

	const Round current_round() const {
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
		QuorumCertificate& quorumCert,
		boost::optional<TimeoutCertificate>& timeoutCert);

	void reset();

	void shiftRoundToNextLeader() {
		shift_round_to_next_leader_++;
	}

	Round getShiftRoundToNextLeader() {
		return shift_round_to_next_leader_;
	}

private:
	void CancelRoundTimeout();

	beast::Journal journal_;
	std::atomic<Round> current_round_;
	boost::asio::steady_timer round_timeout_timer_;
	PendingVotes::pointer pending_votes_;
	boost::optional<Vote> send_vote_;
	// 加入当前 current_round 的 leader 没有出块或是异常
	// 下一个 next_round = current_round + 1 的 leader 可以接管此轮次 current_round
	std::atomic<Round> shift_round_to_next_leader_;
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_ROUNDSTATE_H