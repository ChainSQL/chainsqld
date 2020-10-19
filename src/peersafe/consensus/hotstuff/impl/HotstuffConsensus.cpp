//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012, 2013 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================


#include <peersafe/consensus/hotstuff/HotstuffConsensus.h>


namespace ripple {


HotstuffConsensus::HotstuffConsensus(
    ripple::Adaptor& adaptor,
    clock_type const& clock,
    beast::Journal j)
    : ConsensusBase(clock, j)
    , adaptor_(*(HotstuffAdaptor*)(&adaptor))
{
    hotstuff_ = std::make_shared<hotstuff::Hotstuff>(
        adaptor_.getIOService(),
        j,
        adaptor_.valPublic(),
        this,
        adaptor_,
        this,
        this,
        adaptor_);
}

void HotstuffConsensus::startRound(
    NetClock::time_point const& now,
    typename Ledger_t::ID const& prevLedgerID,
    Ledger_t prevLedger,
    hash_set<NodeID> const& nowUntrusted,
    bool proposing)
{
    hotstuff_->start();
}

void HotstuffConsensus::timerEntry(NetClock::time_point const& now)
{
    (void)now;
}

bool HotstuffConsensus::peerConsensusMessage(
    std::shared_ptr<PeerImp>& peer,
    bool isTrusted,
    std::shared_ptr<protocol::TMConsensus> const& m)
{
    return true;
}

void HotstuffConsensus::gotTxSet(NetClock::time_point const& now, TxSet_t const& txSet)
{

}

Json::Value HotstuffConsensus::getJson(bool full) const
{
    Json::Value ret(Json::objectValue);

    return ret;
}

void HotstuffConsensus::extract(hotstuff::Command& cmd)
{

}

bool HotstuffConsensus::compute(const hotstuff::Block& block, LedgerInfo& ledger_info)
{
    return false;
}

bool HotstuffConsensus::verify(const LedgerInfo& ledger_info, const LedgerInfo& parent_ledger_info)
{
    return false;
}

int HotstuffConsensus::commit(const hotstuff::Block& block)
{
    return false;
}

const hotstuff::Author& HotstuffConsensus::Self() const
{
    return adaptor_.valPublic();
}

bool HotstuffConsensus::signature(const Slice& message, hotstuff::Signature& signature)
{
    return false;
}

const bool HotstuffConsensus::verifySignature(
    const hotstuff::Author& author,
    const hotstuff::Signature& signature,
    const Slice& message) const
{
    return false;
}

const bool HotstuffConsensus::verifyLedgerInfo(
    const hotstuff::BlockInfo& commit_info,
    const hotstuff::HashValue& consensus_data_hash,
    const std::map<hotstuff::Author, hotstuff::Signature>& signatures) const
{
    return false;
}

const bool HotstuffConsensus::checkVotingPower(const std::map<hotstuff::Author, hotstuff::Signature>& signatures) const
{
    return false;
}

// -------------------------------------------------------------------
// Private member functions

int HotstuffConsensus::handleProposal(
    const hotstuff::Block& proposal,
    const hotstuff::SyncInfo& sync_info)
{
    return hotstuff_->handleProposal(proposal, sync_info);
}

int HotstuffConsensus::handleVote(
    const ripple::hotstuff::Vote& vote,
    const ripple::hotstuff::SyncInfo& sync_info)
{
    return hotstuff_->handleVote(vote, sync_info);
}


} // namespace ripple