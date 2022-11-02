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
#include <peersafe/consensus/RpcaPopAdaptor.h>
#include <peersafe/consensus/rpca/RpcaConsensusParams.h>


namespace ripple {

class RpcaAdaptor final : public RpcaPopAdaptor
{
private:
    /** Warn for transactions that haven't been included every so many ledgers.
     */
    constexpr static unsigned int censorshipWarnInternal = 15;

    RCLCensorshipDetector<TxID, LedgerIndex> censorshipDetector_;
    RpcaConsensusParms parms_;

public:
    RpcaAdaptor(RpcaAdaptor&) = default;
    RpcaAdaptor&
    operator=(RpcaAdaptor&) = default;

    RpcaAdaptor(
        Schema& app,
        std::unique_ptr<FeeVote>&& feeVote,
        LedgerMaster& ledgerMaster,
        InboundTransactions& inboundTransactions,
        ValidatorKeys const& validatorKeys,
        beast::Journal journal,
        LocalTxs& localTxs);

    /** Consensus simulation parameters */
    inline RpcaConsensusParms const&
    parms() const
    {
        return parms_;
    }

    inline std::pair<std::size_t, hash_set<NodeKey_t>>
    getQuorumKeys() const
    {
        return app_.validators().getQuorumKeys();
    }

    inline std::size_t
    laggards(Ledger_t::Seq const seq, hash_set<NodeKey_t>& trustedKeys) const
    {
        return app_.getValidations().laggards(seq, trustedKeys);
    }

    inline bool
    validator() const
    {
        return !valPublic_.empty();
    }

    /** Whether the open ledger has any transactions */
    inline bool
    hasOpenTransactions() const
    {
        return !app_.openLedger().empty();
    }

    /** Relay the given proposal to all peers

        @param peerPos The peer position to relay.
    */
    void
    share(RCLCxPeerPos const& peerPos);

    /** Relay disputed transacction to peers.

        Only relay if the provided transaction hasn't been shared recently.

        @param tx The disputed transaction to relay.
    */
    void
    share(RCLCxTx const& tx);

    /** Process the accepted ledger that was a result of simulation/force
        accept.

        @ref onAccept
    */
    void
    onForceAccept(
        Result const& result,
        RCLCxLedger const& prevLedger,
        NetClock::duration const& closeResolution,
        ConsensusCloseTimes const& rawCloseTimes,
        ConsensusMode const& mode,
        Json::Value&& consensusJson);

    /** Close the open ledger and return initial consensus position.

        @param ledger the ledger we are changing to
        @param closeTime When consensus closed the ledger
        @param mode Current consensus mode
        @return Tentative consensus result
    */
    Result
    onClose(
        RCLCxLedger const& ledger,
        NetClock::time_point const& closeTime,
        ConsensusMode mode);

    void
    updateOperatingMode(std::size_t const positions) const;

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

}  // namespace ripple

#endif
