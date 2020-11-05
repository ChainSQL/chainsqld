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

#ifndef RIPPLE_CONSENSUS_HOTSTUFF_PENDINGVOTES_H
#define RIPPLE_CONSENSUS_HOTSTUFF_PENDINGVOTES_H

#include <map>
#include <set>
#include <tuple>
#include <mutex>
#include <memory>

#include <peersafe/consensus/hotstuff/impl/Vote.h>
#include <peersafe/consensus/hotstuff/impl/QuorumCert.h>

namespace ripple {
namespace hotstuff {

class ValidatorVerifier;

class PendingVotes
{
public:
	using pointer = std::shared_ptr<PendingVotes>;

	static pointer New() {
		return pointer(new PendingVotes());
	}

	enum VoteReceptionResult {
		/// The vote has been added but QC has not been formed yet. Return the amount of voting power
		/// the given (proposal, execution) pair.
		VoteAdded,
		/// The very same vote message has been processed in past.
		DuplicateVote,
		/// The very same author has already voted for another proposal in this round (equivocation).
		EquivocateVote,
		/// This block has just been certified after adding the vote.
		NewQuorumCertificate,
		/// The vote completes a new TimeoutCertificate
		NewTimeoutCertificate,
		/// There might be some issues adding a vote
		ErrorAddingVote,
		/// The vote is not for the current round.
		UnexpectedRound,
	};

    ~PendingVotes();

	int insertVote(
		const Vote& vote,
		ValidatorVerifier* verifer,
		QuorumCertificate& quorumCert,
		boost::optional<TimeoutCertificate>& timeoutCert);

private:
    PendingVotes();

	std::mutex mutex_;
	std::map<Author, Vote> author_to_vote_;
	std::map<HashValue, LedgerInfoWithSignatures> li_digest_to_votes_;
	std::map<HashValue, std::set<Round>> maybe_shift_rounds_;
	boost::optional<TimeoutCertificate> maybe_partial_timeout_cert_;
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_PENDINGVOTES_H