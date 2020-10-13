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


#include <peersafe/consensus/hotstuff/HotstuffAdaptor.h>


namespace ripple {


HotstuffAdaptor::HotstuffAdaptor(
    Application& app,
    std::unique_ptr<FeeVote>&& feeVote,
    LedgerMaster& ledgerMaster,
    InboundTransactions& inboundTransactions,
    ValidatorKeys const & validatorKeys,
    beast::Journal journal)
    : Adaptor(
        app,
        std::move(feeVote),
        ledgerMaster,
        inboundTransactions,
        validatorKeys,
        journal)
{
    if (app_.config().exists(SECTION_HCONSENSUS))
    {
    }
}

void HotstuffAdaptor::onAccept(
    Result const& result,
    RCLCxLedger const& prevLedger,
    NetClock::duration const& closeResolution,
    ConsensusCloseTimes const& rawCloseTimes,
    ConsensusMode const& mode,
    Json::Value&& consensusJson)
{
    return;
}

bool HotstuffAdaptor::checkLedgerAccept(std::shared_ptr<Ledger const> const& ledger)
{
    return true;
}

HotstuffAdaptor::Author HotstuffAdaptor::GetValidProposer(Round round) const
{
    auto const& validators = app_.validators().validators();
    assert(validators.size() > 0);

    return validators[round];
}

void HotstuffAdaptor::broadcast(const Block& proposal, const SyncInfo& sync_info)
{

}

void HotstuffAdaptor::sendVote(const Author& author, const Vote& vote, const SyncInfo& sync_info)
{

}

}