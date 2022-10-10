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


#ifndef PEERSAFE_CONSENSUS_HOTSTUFFADAPTOR_H_INCLUDE
#define PEERSAFE_CONSENSUS_HOTSTUFFADAPTOR_H_INCLUDE


#include <peersafe/protocol/STProposal.h>
#include <peersafe/protocol/STVote.h>
#include <peersafe/protocol/STEpochChange.h>
#include <peersafe/consensus/Adaptor.h>
#include <peersafe/consensus/hotstuff/HotstuffConsensusParams.h>
#include <peersafe/consensus/hotstuff/impl/Types.h>
#include <peersafe/consensus/hotstuff/impl/ProposerElection.h>
#include <peersafe/consensus/hotstuff/impl/NetWork.h>
#include <peersafe/consensus/hotstuff/impl/ExecuteBlock.h>


namespace ripple {


class HotstuffAdaptor final : public Adaptor, public hotstuff::ProposerElection
{
private:
    HotstuffConsensusParms parms_;

public:
    using Author = hotstuff::Author;
    using Round = hotstuff::Round;
    using Block = hotstuff::Block;
    using SyncInfo = hotstuff::SyncInfo;
    using Vote = hotstuff::Vote;

public:
    HotstuffAdaptor(HotstuffAdaptor&) = default;
    HotstuffAdaptor&
    operator=(HotstuffAdaptor&) = default;

    HotstuffAdaptor(
        Schema& app,
        std::unique_ptr<FeeVote>&& feeVote,
        LedgerMaster& ledgerMaster,
        InboundTransactions& inboundTransactions,
        ValidatorKeys const& validatorKeys,
        beast::Journal journal,
        LocalTxs& localTxs,
        ConsensusParms const& consensusParms);

    inline HotstuffConsensusParms const&
    parms() const
    {
        return parms_;
    }

    inline std::recursive_mutex&
    peekConsensusMutex()
    {
        return app_.getOPs().peekConsensusMutex();
    }

    inline boost::asio::io_service&
    getIOService() const
    {
        return app_.getIOService();
    }

    TrustChanges
    onConsensusReached(
        bool waitingConsensusReach,
        Ledger_t previousLedger,
        uint64_t newRound) override final;

    // Overwrite ProposerElection interfaces.
    Author
    GetValidProposer(Round round) const override final;

    std::shared_ptr<SHAMap>
    onExtractTransactions(RCLCxLedger const& prevLedger, ConsensusMode mode);

    void
    broadcast(STProposal const& proposal);
    void
    broadcast(STVote const& vote);
    void
    sendVote(PublicKey const& pubKey, STVote const& vote);
    void
    broadcast(STEpochChange const& epochChange);
    void
    acquireBlock(PublicKey const& pubKey, uint256 const& hash);
    void
    sendBLock(
        std::shared_ptr<PeerImp> peer,
        hotstuff::ExecutedBlock const& block);

    bool
    doAccept(typename Ledger_t::ID const& lgrId);

    void
    peerValidation(std::shared_ptr<PeerImp>& peer, STValidation::ref val);

private:
    void
    validate(std::shared_ptr<Ledger const> ledger);

    void
    handleNewValidation(STValidation::ref val, std::string const& source);
};


}

#endif
