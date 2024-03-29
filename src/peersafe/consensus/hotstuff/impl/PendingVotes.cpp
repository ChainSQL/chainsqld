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

PendingVotes::PendingVotes()
    : mutex_()
    , author_to_vote_()
    , li_digest_to_votes_()
    , quorum_certificate_record_()
    , maybe_partial_timeout_cert_()
    , cache_votes_mutex_()
    , cache_votes_()
{
}

PendingVotes::~PendingVotes()
{
}

int
PendingVotes::insertVote(
    const Vote& vote,
    ValidatorVerifier* verifer,
    QuorumCertificate& quorumCert,
    boost::optional<TimeoutCertificate>& timeoutCert)
{
    std::lock_guard<std::mutex> lock(mutex_);

    HashValue li_digest = vote.ledger_info().consensus_data_hash;

    if (quorum_certificate_record_.find(li_digest) !=
        quorum_certificate_record_.end())
    {
        JLOG(debugLog().info())
            << "The round for vote is " << vote.vote_data().proposed().round
            << " QC has already reached";
        return VoteReceptionResult::QCHasAlreadyProcessed;
    }

    // Has the author already voted for this round?
    auto previously_seen_vote = author_to_vote_.find(vote.author());
    if (previously_seen_vote != author_to_vote_.end())
    {
        if (vote.vote_data().tc() != previously_seen_vote->second.vote_data().tc())
        {
            // Erase the previous vote
            author_to_vote_.erase(vote.author());
        }
        else if (li_digest !=
            previously_seen_vote->second.ledger_info().consensus_data_hash)
        {
            JLOG(debugLog().warn()) << "An anutor " << vote.author()
                                    << " voted a vote was Equivocated."
                                    << "The round for vote is "
                                    << vote.vote_data().proposed().round;
            return VoteReceptionResult::EquivocateVote;
        }
        else if (previously_seen_vote->second.isTimeout() || !vote.isTimeout())
        {
            // we've already seen an equivalent vote before
            JLOG(debugLog().warn()) << "An anutor " << vote.author()
                                    << " voted a vote was duplicated."
                                    << "The round for vote is "
                                    << vote.vote_data().proposed().round;
            return VoteReceptionResult::DuplicateVote;  // DuplicateVote
        }
        else
        {
            // Erase the previous vote
            author_to_vote_.erase(vote.author());
        }
    }

    // Store a new vote(or update in case it's a new timeout vote)
    author_to_vote_.emplace(std::make_pair(vote.author(), vote));

    auto it = li_digest_to_votes_.find(li_digest);
    if (it == li_digest_to_votes_.end())
    {
        LedgerInfoWithSignatures ledger_info =
            LedgerInfoWithSignatures(vote.ledger_info());
        // ledger_info.ledger_info = vote.ledger_info();
        it = li_digest_to_votes_.emplace(std::make_pair(li_digest, ledger_info))
                 .first;
    }
    it->second.addSignature(vote.author(), vote.signature());

    // check if we have enough signatures to create a QC
    if (verifer->checkVotingPower(it->second.signatures))
    {
        quorumCert = QuorumCertificate(vote.vote_data(), it->second);
        quorum_certificate_record_.emplace(li_digest, quorumCert);
        return VoteReceptionResult::NewQuorumCertificate;
    }

    // We couldn't form a QuorumCertificate,
    // let's check if we can create a TimeoutCertificate
    //if (vote.isTimeout())
    //{
    //    Timeout timoeut = vote.timeout();
    //    Signature signature = vote.timeout_signature().get();
    //    TimeoutCertificate partial_tc =
    //        maybe_partial_timeout_cert_.get_value_or(
    //            TimeoutCertificate(timoeut));
    //    partial_tc.addSignature(vote.author(), signature);
    //    if (verifer->checkVotingPower(partial_tc.signatures()))
    //    {
    //        timeoutCert = partial_tc;
    //        return VoteReceptionResult::NewTimeoutCertificate;
    //    }
    //}

    return VoteReceptionResult::VoteAdded;
}

std::size_t
PendingVotes::cacheVote(const Vote& vote)
{
    std::lock_guard<std::mutex> lock(cache_votes_mutex_);
    HashValue block_id = vote.vote_data().proposed().id;
    auto it = cache_votes_.find(block_id);
    if (it == cache_votes_.end())
    {
        it = cache_votes_.emplace(block_id, Votes()).first;
    }

    it->second.push_back(vote);
    return it->second.size();
}

std::size_t
PendingVotes::getAndRemoveCachedVotes(
    const HashValue& id,
    PendingVotes::Votes& votes)
{
    votes.clear();
    std::lock_guard<std::mutex> lock(cache_votes_mutex_);
    auto it = cache_votes_.find(id);
    if (it != cache_votes_.end())
    {
        votes.swap(it->second);
    }
    return votes.size();
}

}  // namespace hotstuff
}  // namespace ripple
