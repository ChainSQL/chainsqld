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


#ifndef PEERSAFE_CONSENSUS_RPCAADAPTOR_H_INCLUDE
#define PEERSAFE_CONSENSUS_RPCAADAPTOR_H_INCLUDE


#include <ripple/app/consensus/RCLCensorshipDetector.h>
#include <peersafe/consensus/Adaptor.h>
#include <peersafe/consensus/rpca/RpcaConsensusParams.h>


namespace ripple {


class RpcaAdaptor : public Adaptor
{
private:
    /** Warn for transactions that haven't been included every so many ledgers. */
    constexpr static unsigned int censorshipWarnInternal = 15;

    RCLCensorshipDetector<TxID, LedgerIndex>    censorshipDetector_;
    RpcaConsensusParms                          parms_;

public:
    RpcaAdaptor(RpcaAdaptor&) = default;
    RpcaAdaptor& operator=(RpcaAdaptor&) = default;

    RpcaAdaptor(
        Application& app,
        std::unique_ptr<FeeVote>&& feeVote,
        LedgerMaster& ledgerMaster,
        LocalTxs& localTxs,
        InboundTransactions& inboundTransactions,
        ValidatorKeys const & validatorKeys,
        beast::Journal journal);

    bool preStartRound(RCLCxLedger const & prevLgr) override final;
    boost::optional<RCLCxLedger> acquireLedger(LedgerHash const& hash) override final;

    /** Consensus simulation parameters */
    inline RpcaConsensusParms const& parms() const
    {
        return parms_;
    }

    /** Close the open ledger and return initial consensus position.

        @param ledger the ledger we are changing to
        @param closeTime When consensus closed the ledger
        @param mode Current consensus mode
        @return Tentative consensus result
    */
    Result onClose(
        RCLCxLedger const& ledger,
        NetClock::time_point const& closeTime,
        ConsensusMode mode);

private:
    void doAccept(
        Result const& result,
        RCLCxLedger const& prevLedger,
        NetClock::duration closeResolution,
        ConsensusCloseTimes const& rawCloseTimes,
        ConsensusMode const& mode,
        Json::Value&& consensusJson) override final;
};


}

#endif
