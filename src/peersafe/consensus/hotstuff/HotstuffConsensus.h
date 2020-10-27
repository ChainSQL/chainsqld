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


#ifndef PEERSAFE_HOTSTUFF_CONSENSUS_H_INCLUDED
#define PEERSAFE_HOTSTUFF_CONSENSUS_H_INCLUDED


#include <peersafe/protocol/STProposal.h>
#include <peersafe/protocol/STVote.h>
#include <peersafe/consensus/LedgerTiming.h>
#include <peersafe/consensus/ConsensusBase.h>
#include <peersafe/consensus/hotstuff/HotstuffAdaptor.h>
#include <peersafe/consensus/hotstuff/Hotstuff.h>


namespace ripple {


class HotstuffConsensus final
    : public ripple::ConsensusBase
    , public hotstuff::CommandManager
    , public hotstuff::StateCompute
    , public hotstuff::ValidatorVerifier
    , public hotstuff::NetWork
{
public:
    HotstuffConsensus(ripple::Adaptor& adaptor, clock_type const& clock, beast::Journal j);

public:

    void startRound(
        NetClock::time_point const& now,
        typename Ledger_t::ID const& prevLedgerID,
        Ledger_t prevLedger,
        hash_set<NodeID> const& nowUntrusted,
        bool proposing) override final;

    /** Call periodically to drive consensus forward.

        @param now The network adjusted time
    */
    void timerEntry(NetClock::time_point const& now) override final;

    bool peerConsensusMessage(
        std::shared_ptr<PeerImp>& peer,
        bool isTrusted,
        std::shared_ptr<protocol::TMConsensus> const& m) override final;

    /** Process a transaction set acquired from the network

        @param now The network adjusted time
        @param txSet the transaction set
    */
    void gotTxSet(NetClock::time_point const& now, TxSet_t const& txSet) override final;

    /** Get the Json state of the consensus process.

        Called by the consensus_info RPC.

        @param full True if verbose response desired.
        @return     The Json state.
    */
    Json::Value getJson(bool full) const override final;

    // Overwrite CommandManager extract interface.
    boost::optional<hotstuff::Command> extract(hotstuff::BlockData &blockData) override final;

    // Overwrite StateCompute interfaces.
    bool compute(const hotstuff::Block& block, LedgerInfo& result) override final;
    bool verify(const LedgerInfo& info, const LedgerInfo& prevInfo) override final;
    int commit(const hotstuff::Block& block) override final;

    // Overwrite ValidatorVerifier interfaces.
    const hotstuff::Author& Self() const override final;
    bool signature(const uint256& digest, hotstuff::Signature& signature) override final;
    const bool verifySignature(
        const hotstuff::Author& author,
        const hotstuff::Signature& signature,
        const uint256& digest) const override final;
    const bool verifyLedgerInfo(
        const hotstuff::BlockInfo& commit_info,
        const hotstuff::HashValue& consensus_data_hash,
        const std::map<hotstuff::Author, hotstuff::Signature>& signatures) override final;
    const bool checkVotingPower(const std::map<hotstuff::Author, hotstuff::Signature>& signatures) const override final;

    // Overwrite NetWork interfaces.
    void broadcast(const hotstuff::Block& block, const hotstuff::SyncInfo& syncInfo) override final;
    void broadcast(const hotstuff::Vote& vote, const hotstuff::SyncInfo& syncInfo) override final;
    void sendVote(const hotstuff::Author& author, const hotstuff::Vote& vote, const hotstuff::SyncInfo& syncInfo) override final;

private:
    void peerProposal(
        std::shared_ptr<PeerImp>& peer,
        bool isTrusted,
        std::shared_ptr<protocol::TMConsensus> const& m);

    void peerProposalInternal(STProposal::ref proposal);

    void peerVote(
        std::shared_ptr<PeerImp>& peer,
        bool isTrusted,
        std::shared_ptr<protocol::TMConsensus> const& m);

    void newRound(
        RCLCxLedger::ID const& prevLgrId,
        RCLCxLedger const& prevLgr,
        ConsensusMode mode);

private:
    HotstuffAdaptor& adaptor_;
    std::shared_ptr<hotstuff::Hotstuff> hotstuff_;

    NetClock::duration closeResolution_ = ledgerDefaultTimeResolution;

    std::recursive_mutex lock_;
    hash_map<typename TxSet_t::ID, const TxSet_t> acquired_;
    std::map<typename TxSet_t::ID, std::map<PublicKey, STProposal::pointer>> proposalCache_;
};


}

#endif 