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

#ifndef PEERSAFE_POP_CONSENSUS_H_INCLUDED
#define PEERSAFE_POP_CONSENSUS_H_INCLUDED


#include <peersafe/app/util/Common.h>
#include <peersafe/consensus/ConsensusBase.h>
#include <peersafe/consensus/pop/PopAdaptor.h>
#include <peersafe/consensus/pop/ViewChangeManager.h>
#include <boost/logic/tribool.hpp>
#include <sstream>


namespace ripple {


class PublicKey;


class PopConsensus : public ConsensusBase
{
private:
    PopAdaptor& adaptor_;

    //-------------------------------------------------------------------------
    // Network time measurements of consensus progress

    // The current network adjusted time.  This is the network time the
    // ledger would close if it closed now
    NetClock::time_point now_;
    NetClock::time_point closeTime_;
    NetClock::time_point openTime_;
    std::chrono::steady_clock::time_point proposalTime_;
    uint64_t openTimeMilli_;
    uint64_t consensusTime_;

    uint64_t lastTxSetSize_;

    boost::optional<NetClock::time_point> startTime_;
    NetClock::time_point initAnnounceTime_;

    NetClock::duration closeResolution_ = ledgerDefaultTimeResolution;

    // Transaction Sets, indexed by hash of transaction tree
    hash_map<typename TxSet_t::ID, const TxSet_t> acquired_;

    // Tx set peers has voted(including self)
    hash_map<typename TxSet_t::ID, std::set<PublicKey>> txSetVoted_;
    hash_map<typename TxSet_t::ID, std::set<PublicKey>> txSetCached_;

    // save next proposal
    std::map<std::uint32_t, std::map<PublicKey, RCLCxPeerPos>> proposalCache_;

    boost::optional<Result> result_;

    // current setID proposed by leader.
    boost::optional<typename TxSet_t::ID> setID_;

    ConsensusCloseTimes rawCloseTimes_;

    // Transaction hashes that have packaged in packaging block.
    std::vector<uint256> transactions_;

    bool waitingConsensusReach_ = true;
    bool extraTimeOut_ = false;

    // Count for timeout that didn't reach consensus
    unsigned timeOutCount_;
    std::atomic_bool leaderFailed_ = {false};

    uint64_t view_ = 0;
    uint64_t toView_ = 0;
    ViewChangeManager viewChangeManager_;

    std::recursive_mutex lock_;

     Ledger_t::ID initAcquireLedgerID_ = beast::zero;
public:
    /** Constructor.

        @param clock The clock used to internally sample consensus progress
        @param adaptor The instance of the adaptor class
        @param j The journal to log debug output
    */
    PopConsensus(Adaptor& adaptor, clock_type const& clock, beast::Journal j);

    /** Kick-off the next round of consensus.

        Called by the client code to start each round of consensus.

        @param now The network adjusted time
        @param prevLedgerID the ID of the last ledger
        @param prevLedger The last ledger
        @param proposing Whether we want to send proposals to peers this round.

        @note @b prevLedgerID is not required to the ID of @b prevLedger
        since the ID may be known locally before the contents of the ledger
        arrive
    */
    void
    startRound(
        NetClock::time_point const& now,
        typename Ledger_t::ID const& prevLedgerID,
        Ledger_t prevLedger,
        hash_set<NodeID_t> const& nowUntrusted,
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

    void
    onDeleteUntrusted(hash_set<NodeID> const& nowUntrusted) override final;

    std::chrono::milliseconds 
    getConsensusTimeOut() const override final;
private:
    inline uint64_t
    timeSinceOpen() const
    {
        return utcTime() - openTimeMilli_;
    }
    inline uint64_t
    timeSinceConsensus() const
    {
        return utcTime() - consensusTime_;
    }

    std::chrono::milliseconds
    timeSinceLastClose();

    void
    initAnnounce();
    void
    initAnnounceToPeer(PublicKey const& pubKey);

    void
    startRoundInternal(
        NetClock::time_point const& now,
        typename Ledger_t::ID const& prevLedgerID,
        Ledger_t const& prevLedger,
        ConsensusMode mode);

    // Change our view of the previous ledger
    void
    handleWrongLedger(typename Ledger_t::ID const& lgrId);

    /** Check if our previous ledger matches the network's.

        If the previous ledger differs, we are no longer in sync with
        the network and need to bow out/switch modes.
    */
    void
    checkLedger();

    /** Check if we have proposal cached for current ledger when consensus
        started.

        If we have the cache,just need to fetch the corresponding tx-set.
    */
    void
    checkCache();

    /** Handle tx-collecting phase.

        In the tx-collecting phase, the ledger is open as we wait for new
        transactions.  After enough time has elapsed, we will close the ledger,
        switch to the establish phase and start the consensus process.
    */
    void
    phaseCollecting();

    /** Is final condition reached for proposing.
        We should check:
        1. Is maxBlockTime reached.
        2. Is tx-count reached max and max >=5000 and minBlockTime/2
           reached.(There will be a time to reach tx-set consensus)
        3. If there are txs but not reach max-count,is the minBlockTime reached.
    */
    bool
    finalCondReached(int64_t sinceOpen, int64_t sinceLastClose);

    /** Handle voting phase.

        In the voting phase, the ledger has closed and we work with peers
        to reach consensus. Update our position only on the timer, and in this
        phase.

        If we have consensus, move to the accepted phase.
    */
    void
    checkVoting();

    bool
    haveConsensus();

    void
    checkTimeout();

    void
    launchViewChange();

    void
    leaveConsensus();

    /** A peer has proposed a new position, adjust our tracking.

         @param now The network adjusted time
         @param newProposal The new proposal from a peer
         @return Whether we should do delayed relay of this proposal.
     */
    bool
    peerProposal(
        std::shared_ptr<PeerImp>& peer,
        bool isTrusted,
        std::shared_ptr<protocol::TMConsensus> const& m);

    /** Handle a replayed or a new peer proposal. */
    bool
    peerProposalInternal(
        NetClock::time_point const& now,
        PeerPosition_t const& newProposal);

    void
    checkSaveNextProposal(PeerPosition_t const& newPeerPos);

    bool
    peerViewChange(
        std::shared_ptr<PeerImp>& peer,
        bool isTrusted,
        std::shared_ptr<protocol::TMConsensus> const& m);
    bool
    peerViewChangeInternal(STViewChange::ref viewChange);
    void
    checkChangeView(uint64_t toView);
    void
    onViewChange(uint64_t toView);

    bool
    peerValidation(
        std::shared_ptr<PeerImp>& peer,
        bool isTrusted,
        std::shared_ptr<protocol::TMConsensus> const& m);

    bool
    peerInitAnnounce(
        std::shared_ptr<PeerImp>& peer,
        bool isTrusted,
        std::shared_ptr<protocol::TMConsensus> const& m);
    bool
    peerInitAnnounceInternal(STInitAnnounce::ref viewChange);

    bool
    peerAcquirValidationSet(
        std::shared_ptr<PeerImp>& peer,
        bool isTrusted,
        std::shared_ptr<protocol::TMConsensus> const& m);

    bool
    peerValidationSetData(
        std::shared_ptr<PeerImp>& peer,
        bool isTrusted,
        std::shared_ptr<protocol::TMConsensus> const& m);
};


}
#endif
