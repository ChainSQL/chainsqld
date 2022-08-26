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

#include <peersafe/consensus/ConsensusBase.h>
#include <peersafe/consensus/LedgerTiming.h>
#include <peersafe/consensus/hotstuff/Hotstuff.h>
#include <peersafe/consensus/hotstuff/HotstuffAdaptor.h>
#include <peersafe/protocol/STEpochChange.h>
#include <peersafe/protocol/STProposal.h>
#include <peersafe/protocol/STVote.h>

#include <future>

namespace ripple {

class HotstuffConsensus final : public ConsensusBase,
                                public hotstuff::CommandManager,
                                public hotstuff::StateCompute,
                                public hotstuff::ValidatorVerifier,
                                public hotstuff::NetWork
{
public:
    HotstuffConsensus(
        Adaptor& adaptor,
        clock_type const& clock,
        beast::Journal j);
    ~HotstuffConsensus();

public:
    void
    startRound(
        NetClock::time_point const& now,
        typename Ledger_t::ID const& prevLedgerID,
        Ledger_t prevLedger,
        hash_set<NodeID> const& nowUntrusted,
        bool proposing) override final;

    /** Call periodically to drive consensus forward.

        @param now The network adjusted time
    */
    void
    timerEntry(NetClock::time_point const& now) override final;

    bool
    peerConsensusMessage(
        std::shared_ptr<PeerImp>& peer,
        bool isTrusted,
        std::shared_ptr<protocol::TMConsensus> const& m) override final;

    /** Process a transaction set acquired from the network

        @param now The network adjusted time
        @param txSet the transaction set
    */
    void
    gotTxSet(NetClock::time_point const& now, TxSet_t const& txSet)
        override final;

    /** Get the Json state of the consensus process.

        Called by the consensus_info RPC.

        @param full True if verbose response desired.
        @return     The Json state.
    */
    Json::Value
    getJson(bool full) const override final;

    bool
    waitingForInit() const override final;

    uint64_t
    getCurrentTurn() const override final;

    // Overwrite CommandManager extract interface.
    bool
    canExtract() override final;
    boost::optional<hotstuff::Command>
    extract(hotstuff::BlockData& blockData) override final;

    // Overwrite StateCompute interfaces.
    bool
    compute(const hotstuff::Block& block, hotstuff::StateComputeResult& result)
        override final;
    bool
    verify(
        const hotstuff::Block& block,
        const hotstuff::StateComputeResult& result) override final;
    int
    commit(const hotstuff::ExecutedBlock& block) override final;
    bool
    syncState(const hotstuff::BlockInfo& prevInfo) override final;
    bool
    syncBlock(
        const uint256& blockID,
        const hotstuff::Author& author,
        hotstuff::ExecutedBlock& executedBlock) override final;
    void
    asyncBlock(
        const uint256& blockID,
        const hotstuff::Author& author,
        hotstuff::StateCompute::AsyncCompletedHander asyncCompletedHandler)
        override final;

    // Overwrite ValidatorVerifier interfaces.
    const hotstuff::Author&
    Self() const override final;
    bool
    signature(const uint256& digest, hotstuff::Signature& signature)
        override final;
    bool
    verifySignature(
        const hotstuff::Author& author,
        const hotstuff::Signature& signature,
        const hotstuff::HashValue& digest) const override final;
    bool
    verifySignature(
        const hotstuff::Author& author,
        const hotstuff::Signature& signature,
        const hotstuff::Block& block) const override final;
    bool
    verifySignature(
        const hotstuff::Author& author,
        const hotstuff::Signature& signature,
        const hotstuff::Vote& vote) const override final;
    bool
    verifyLedgerInfo(
        const hotstuff::BlockInfo& commit_info,
        const hotstuff::HashValue& consensus_data_hash,
        const std::map<hotstuff::Author, hotstuff::Signature>& signatures)
        override final;
    bool
    checkVotingPower(const std::map<hotstuff::Author, hotstuff::Signature>&
                         signatures) const override final;

    // Overwrite NetWork interfaces.
    void
    broadcast(const hotstuff::Block& block, const hotstuff::SyncInfo& syncInfo)
        override final;
    void
    broadcast(const hotstuff::Vote& vote, const hotstuff::SyncInfo& syncInfo)
        override final;
    void
    sendVote(
        const hotstuff::Author& author,
        const hotstuff::Vote& vote,
        const hotstuff::SyncInfo& syncInfo) override final;
    void
    broadcast(
        const hotstuff::EpochChange& epochChange,
        const hotstuff::SyncInfo& syncInfo) override final;

    std::chrono::milliseconds 
    getConsensusTimeOut() const override final;
private:
    std::chrono::milliseconds
    timeSinceLastClose() const;

    void
    initAnnounce();

    void
    peerProposal(
        std::shared_ptr<PeerImp>& peer,
        bool isTrusted,
        std::shared_ptr<protocol::TMConsensus> const& m);

    void
    peerProposalInternal(STProposal::ref proposal);

    void
    peerVote(
        std::shared_ptr<PeerImp>& peer,
        bool isTrusted,
        std::shared_ptr<protocol::TMConsensus> const& m);

    void
    peerAcquireBlock(
        std::shared_ptr<PeerImp>& peer,
        bool isTrusted,
        std::shared_ptr<protocol::TMConsensus> const& m);

    void
    peerBlockData(
        std::shared_ptr<PeerImp>& peer,
        bool isTrusted,
        std::shared_ptr<protocol::TMConsensus> const& m);

    void
    peerEpochChange(
        std::shared_ptr<PeerImp>& peer,
        bool isTrusted,
        std::shared_ptr<protocol::TMConsensus> const& m);

    void
    peerInitAnnounce(
        std::shared_ptr<PeerImp>& peer,
        bool isTrusted,
        std::shared_ptr<protocol::TMConsensus> const& m);
    void
    peerInitAnnounceInternal(STInitAnnounce::ref viewChange);

    void
    peerValidation(
        std::shared_ptr<PeerImp>& peer,
        bool isTrusted,
        std::shared_ptr<protocol::TMConsensus> const& m);

    void
    startRoundInternal(
        NetClock::time_point const& now,
        RCLCxLedger::ID const& prevLgrId,
        RCLCxLedger const& prevLgr,
        ConsensusMode mode,
        bool recover);

    void
    checkCache();

    bool
    handleWrongLedger(typename Ledger_t::ID const& lgrId);

private:
    HotstuffAdaptor& adaptor_;
    std::shared_ptr<hotstuff::Hotstuff> hotstuff_;

    NetClock::duration closeResolution_ = ledgerDefaultTimeResolution;

    bool startup_ = false;
    NetClock::time_point startTime_;
    NetClock::time_point initAnnounceTime_;
    NetClock::time_point now_;
    NetClock::time_point consensusTime_;
    uint64_t openTime_;
    uint64_t consensusTimeMil_;

    uint64_t txQueuedCount_;

    hotstuff::Epoch epoch_ = 0;
    bool configChanged_ = false;
    TxSet_t::ID epochChangeHash_;

    hotstuff::Round newRound_ = 0;

    bool waitingConsensusReach_ = true;

    std::recursive_mutex lock_;
    hash_map<typename TxSet_t::ID, const TxSet_t> acquired_;
    std::map<typename TxSet_t::ID, std::map<PublicKey, STProposal::pointer>>
        curProposalCache_;
    std::map<std::uint32_t, std::map<PublicKey, STProposal::pointer>>
        nextProposalCache_;

    // For acquire hotstuff block asynchronize
    TaggedCache<
        uint256,
        std::vector<hotstuff::StateCompute::AsyncCompletedHander>>
        blockAcquiring_;
    NetClock::time_point sweepTime_;
    std::chrono::seconds const sweepInterval_{10};
};

}  // namespace ripple

#endif