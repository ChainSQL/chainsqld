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

#include <peersafe/consensus/hotstuff/impl/PendingVotes.h>
#include <peersafe/consensus/hotstuff/impl/ValidatorVerifier.h>

#include <ripple/basics/Log.h>

namespace ripple {
namespace hotstuff {
    
PendingVotes::PendingVotes(const beast::Journal& journal)
: journal_(&journal)
, author_to_vote_()
, li_digest_to_votes_()
, maybe_partial_timeout_cert_() {
}

PendingVotes::~PendingVotes() {
}

int PendingVotes::insertVote(
	const Vote& vote,
	ValidatorVerifier* verifer,
	QuorumCertificate& quorumCert,
	boost::optional<TimeoutCertificate>& timeoutCert) {
	HashValue li_digest = const_cast<BlockInfo&>(vote.ledger_info().commit_info).id;

	// Has the author already voted for this round?
	auto previously_seen_vote = author_to_vote_.find(vote.author());
	if (previously_seen_vote != author_to_vote_.end()) {
		if (li_digest != const_cast<BlockInfo&>(previously_seen_vote->second.ledger_info().commit_info).id) {
			JLOG(journal_->warn())
				<< "An anutor " << vote.author()
				<< " voted a vote was Equivocated."
				<< "The round for vote is " 
				<< vote.vote_data().proposed().round;
			return VoteReceptionResult::EquivocateVote;
		}
		else {
			// we've already seen an equivalent vote before
			bool new_timeout_vote = vote.isTimeout() && (previously_seen_vote->second.isTimeout() == false);
			if (new_timeout_vote == false) {
				JLOG(journal_->warn())
					<< "An anutor " << vote.author()
					<< " voted a vote was duplicated."
					<< "The round for vote is "
					<< vote.vote_data().proposed().round;
				return VoteReceptionResult::DuplicateVote; // DuplicateVote
			}
		}
	}

	//if (verifer->verifySignature(vote.author(), vote.signature(), vote.ledger_info().consensus_data_hash) == false) {
	//	JLOG(journal_->error())
	//		<< "An anutor " << vote.author()
	//		<< " voted a vote mismatch signature."
	//		<< "The round for vote is "
	//		<< vote.vote_data().proposed().round;
	//	return VoteReceptionResult::ErrorAddingVote;
	//}

	// Store a new vote(or update in case it's a new timeout vote)
	author_to_vote_.emplace(std::make_pair(vote.author(), vote));

	auto it = li_digest_to_votes_.find(li_digest);
	if (it == li_digest_to_votes_.end()) {
		LedgerInfoWithSignatures ledger_info = LedgerInfoWithSignatures(vote.ledger_info());
		//ledger_info.ledger_info = vote.ledger_info();
		it = li_digest_to_votes_.emplace(std::make_pair(li_digest, ledger_info)).first;
	}
	it->second.addSignature(vote.author(), vote.signature());

	// check if we have enough signatures to create a QC
	if (verifer->checkVotingPower(it->second.signatures)) {
		quorumCert = QuorumCertificate(*journal_, vote.vote_data(), it->second);
		return VoteReceptionResult::NewQuorumCertificate;
	}
	
	// We couldn't form a QuorumCertificate, 
	// let's check if we can create a TimeoutCertificate
	if (vote.isTimeout()) {
		Timeout timoeut = vote.timeout();
		Signature signature = vote.timeout_signature().get();
		TimeoutCertificate partial_tc = maybe_partial_timeout_cert_.get_value_or(TimeoutCertificate(timoeut));
		partial_tc.addSignature(vote.author(), signature);
		if (verifer->checkVotingPower(partial_tc.signatures())) {
			timeoutCert = partial_tc;
			return VoteReceptionResult::NewTimeoutCertificate;
		}
	}

	return VoteReceptionResult::VoteAdded;
}

} // namespace hotstuff
} // namespace ripple