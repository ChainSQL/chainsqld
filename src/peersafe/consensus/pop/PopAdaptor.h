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


#ifndef PEERSAFE_CONSENSUS_POPADAPTOR_H_INCLUDE
#define PEERSAFE_CONSENSUS_POPADAPTOR_H_INCLUDE


#include <peersafe/protocol/STViewChange.h>
#include <peersafe/consensus/RpcaPopAdaptor.h>
#include <peersafe/consensus/ConsensusParams.h>
#include <peersafe/consensus/pop/PopConsensusParams.h>


namespace ripple {


class PopAdaptor final : public RpcaPopAdaptor
{
private:
    PopConsensusParms parms_;

public:
    PopAdaptor(PopAdaptor&) = default;
    PopAdaptor&
    operator=(PopAdaptor&) = default;

    PopAdaptor(
        Schema& app,
        std::unique_ptr<FeeVote>&& feeVote,
        LedgerMaster& ledgerMaster,
        InboundTransactions& inboundTransactions,
        ValidatorKeys const& validatorKeys,
        beast::Journal journal,
        LocalTxs& localTxs,
        ConsensusParms const& consensusParms);

    inline PopConsensusParms const&
    parms() const
    {
        return parms_;
    }

    inline void
    flushValidations() const
    {
        app_.getValidations().flush();
    }

    inline bool
    isLeader(PublicKey const& publicKey, LedgerIndex curSeq, std::uint64_t view)
    {
        return publicKey == app_.validators().getLeaderPubKey(curSeq + view);
    }

    inline bool
    isLeader(LedgerIndex curSeq, std::uint64_t view)
    {
        return isLeader(valPublic_, curSeq, view);
    }

    Result
    onCollectFinish(
        RCLCxLedger const& ledger,
        std::vector<uint256> const& transactions,
        NetClock::time_point const& closeTime,
        std::uint64_t const& view,
        ConsensusMode mode);

    void
    launchViewChange(STViewChange const& viewChange);
    void
    onViewChanged(bool waitingConsensusReach, Ledger_t previousLedger, uint64_t newView);

private:
    void
    doAccept(
        Result const& result,
        RCLCxLedger const& prevLedger,
        NetClock::duration closeResolution,
        ConsensusCloseTimes const& rawCloseTimes,
        ConsensusMode const& mode,
        Json::Value&& consensusJson) override final;
};


}

#endif
