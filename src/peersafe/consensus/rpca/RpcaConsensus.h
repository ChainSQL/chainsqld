//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012-2017 Ripple Labs Inc.

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

#ifndef PEERSAFE_RPCA_CONSENSUS_H_INCLUDED
#define PEERSAFE_RPCA_CONSENSUS_H_INCLUDED


#include <peersafe/consensus/ConsensusBase.h>
#include <peersafe/consensus/LedgerTiming.h>
#include <peersafe/consensus/rpca/RpcaAdaptor.h>
#include <peersafe/consensus/rpca/RpcaConsensusParams.h>
#include <peersafe/consensus/rpca/DisputedTx.h>
#include <boost/logic/tribool.hpp>
#include <sstream>

namespace ripple {

/** How many of the participants must agree to reach a given threshold?

    Note that the number may not precisely yield the requested percentage.
    For example, with with size = 5 and percent = 70, we return 3, but
    3 out of 5 works out to 60%. There are no security implications to
    this.

    @param participants The number of participants (i.e. validators)
    @param percent The percent that we want to reach

    @return the number of participants which must agree
*/
inline int
participantsNeeded(int participants, int percent)
{
    int result = ((participants * percent) + (percent / 2)) / 100;

    return (result == 0) ? 1 : result;
}

/** Determines whether the current ledger should close at this time.

    This function should be called when a ledger is open and there is no close
    in progress, or when a transaction is received and no close is in progress.

    @param anyTransactions indicates whether any transactions have been received
    @param prevProposers proposers in the last closing
    @param proposersClosed proposers who have currently closed this ledger
    @param proposersValidated proposers who have validated the last closed
                              ledger
    @param prevRoundTime time for the previous ledger to reach consensus
    @param timeSincePrevClose  time since the previous ledger's (possibly
   rounded) close time
    @param openTime     duration this ledger has been open
    @param idleInterval the network's desired idle interval
    @param parms        Consensus constant parameters
    @param j            journal for logging
*/
bool
shouldCloseLedger(
    bool anyTransactions,
    std::size_t prevProposers,
    std::size_t proposersClosed,
    std::size_t proposersValidated,
    std::chrono::milliseconds prevRoundTime,
    std::chrono::milliseconds timeSincePrevClose,
    std::chrono::milliseconds openTime,
    std::chrono::milliseconds idleInterval,
    RpcaConsensusParms const& parms,
    beast::Journal j);

/** Determine whether the network reached consensus and whether we joined.

    @param prevProposers proposers in the last closing (not including us)
    @param currentProposers proposers in this closing so far (not including us)
    @param currentAgree proposers who agree with us
    @param currentFinished proposers who have validated a ledger after this one
    @param previousAgreeTime how long, in milliseconds, it took to agree on the
                             last ledger
    @param currentAgreeTime how long, in milliseconds, we've been trying to
                            agree
    @param parms            Consensus constant parameters
    @param proposing        whether we should count ourselves
    @param j                journal for logging
*/
ConsensusState
checkConsensus(
    std::size_t prevProposers,
    std::size_t currentProposers,
    std::size_t currentAgree,
    std::size_t currentFinished,
    std::chrono::milliseconds previousAgreeTime,
    std::chrono::milliseconds currentAgreeTime,
    RpcaConsensusParms const& parms,
    bool proposing,
    beast::Journal j);

/** Generic implementation of consensus algorithm.

  Achieves consensus on the next ledger.

  Two things need consensus:

    1.  The set of transactions included in the ledger.
    2.  The close time for the ledger.

  The basic flow:

    1. A call to `startRound` places the node in the `Open` phase.  In this
       phase, the node is waiting for transactions to include in its open
       ledger.
    2. Successive calls to `timerEntry` check if the node can close the ledger.
       Once the node `Close`s the open ledger, it transitions to the
       `Establish` phase.  In this phase, the node shares/receives peer
       proposals on which transactions should be accepted in the closed ledger.
    3. During a subsequent call to `timerEntry`, the node determines it has
       reached consensus with its peers on which transactions to include. It
       transitions to the `Accept` phase. In this phase, the node works on
       applying the transactions to the prior ledger to generate a new closed
       ledger. Once the new ledger is completed, the node shares the validated
       ledger with the network, does some book-keeping, then makes a call to
       `startRound` to start the cycle again.

  This class uses a generic interface to allow adapting Consensus for specific
  applications. The Adaptor template implements a set of helper functions that
  plug the consensus algorithm into a specific application.  It also identifies
  the types that play important roles in Consensus (transactions, ledgers, ...).
  The code stubs below outline the interface and type requirements.  The traits
  types must be copy constructible and assignable.

  @warning The generic implementation is not thread safe and the public methods
  are not intended to be run concurrently.  When in a concurrent environment,
  the application is responsible for ensuring thread-safety.  Simply locking
  whenever touching the Consensus instance is one option.

  @tparam Adaptor Defines types and provides helper functions needed to adapt
                  Consensus to the larger application.
*/
class RpcaConsensus : public ConsensusBase
{
private:
    RpcaAdaptor& adaptor_;

    bool firstRound_ = true;

    // The current network adjusted time.  This is the network time the
    // ledger would close if it closed now
    NetClock::time_point now_;
    NetClock::time_point prevCloseTime_;

    bool haveCloseTimeConsensus_ = false;

    // How long the consensus convergence has taken, expressed as
    // a percentage of the time that we expected it to take.
    int convergePercent_{0};

    // How long has this round been open
    ConsensusTimer openTime_;

    NetClock::duration closeResolution_ = ledgerDefaultTimeResolution;

    // Time it took for the last consensus round to converge
    std::chrono::milliseconds prevRoundTime_;

    // Transaction Sets, indexed by hash of transaction tree
    hash_map<typename TxSet_t::ID, const TxSet_t> acquired_;

    boost::optional<Result> result_;
    ConsensusCloseTimes rawCloseTimes_;

    //-------------------------------------------------------------------------
    // Peer related consensus data

    // Peer proposed positions for the current round
    hash_map<NodeID_t, PeerPosition_t> currPeerPositions_;

    // Recently received peer positions, available when transitioning between
    // ledgers or rounds
    hash_map<NodeID_t, std::deque<PeerPosition_t>> recentPeerPositions_;

    // The number of proposers who participated in the last consensus round
    std::size_t prevProposers_ = 0;

    // nodes that have bowed out of this consensus process
    hash_set<NodeID_t> deadNodes_;

public:
    RpcaConsensus(RpcaConsensus&&) noexcept = default;

    /** Constructor.

        @param clock The clock used to internally sample consensus progress
        @param adaptor The instance of the adaptor class
        @param j The journal to log debug output
    */
    RpcaConsensus(Adaptor& adaptor, clock_type const& clock, beast::Journal j);

    /** Kick-off the next round of consensus.

        Called by the client code to start each round of consensus.

        @param now The network adjusted time
        @param prevLedgerID the ID of the last ledger
        @param prevLedger The last ledger
        @param nowUntrusted ID of nodes that are newly untrusted this round
        @param proposing Whether we want to send proposals to peers this round.

        @note @b prevLedgerID is not required to the ID of @b prevLedger since
        the ID may be known locally before the contents of the ledger arrive
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

    /** Simulate the consensus process without any network traffic.

       The end result, is that consensus begins and completes as if everyone
       had agreed with whatever we propose.

       This function is only called from the rpc "ledger_accept" path with the
       server in standalone mode and SHOULD NOT be used during the normal
       consensus process.

       Simulate will call onForceAccept since clients are manually driving
       consensus to the accept phase.

       @param now The current network adjusted time.
       @param consensusDelay Duration to delay between closing and accepting the
                             ledger. Uses 100ms if unspecified.
    */
    void
    simulate(
        NetClock::time_point const& now,
        boost::optional<std::chrono::milliseconds> consensusDelay)
        override final;

    /** Get the Json state of the consensus process.

        Called by the consensus_info RPC.

        @param full True if verbose response desired.
        @return     The Json state.
    */
    Json::Value
    getJson(bool full) const override final;

    bool
    waitingForInit() const override final
    {
        return true;
    }

    std::chrono::milliseconds 
    getConsensusTimeOut() const override final;

private:
    void
    startRoundInternal(
        NetClock::time_point const& now,
        typename Ledger_t::ID const& prevLedgerID,
        Ledger_t const& prevLedger,
        ConsensusMode mode);

    /** If we radically changed our consensus context for some reason,
        we need to replay recent proposals so that they're not lost.
    */
    void
    playbackProposals();

    /** Check if our previous ledger matches the network's.

        If the previous ledger differs, we are no longer in sync with
        the network and need to bow out/switch modes.
    */
    void
    checkLedger();

    // Change our view of the previous ledger
    void
    handleWrongLedger(typename Ledger_t::ID const& lgrId);

    // Revoke our outstanding proposal, if any, and cease proposing
    // until this round ends.
    void
    leaveConsensus();

    /** Handle pre-close phase.

        In the pre-close phase, the ledger is open as we wait for new
        transactions.  After enough time has elapsed, we will close the ledger,
        switch to the establish phase and start the consensus process.
    */
    void
    phaseOpen();

    // Close the open ledger and establish initial position.
    void
    closeLedger();

    // Create disputes between our position and the provided one.
    void
    createDisputes(TxSet_t const& o);

    /** Handle establish phase.

        In the establish phase, the ledger has closed and we work with peers
        to reach consensus. Update our position only on the timer, and in this
        phase.

        If we have consensus, move to the accepted phase.
    */
    void
    phaseEstablish();

    // Adjust our positions to try to agree with other validators.
    void
    updateOurPositions();

    // The rounded or effective close time estimate from a proposer
    NetClock::time_point
    asCloseTime(NetClock::time_point raw) const;

    /** Evaluate whether pausing increases likelihood of validation.
     *
     *  As a validator that has previously synced to the network, if our most
     *  recent locally-validated ledger did not also achieve
     *  full validation, then consider pausing for awhile based on
     *  the state of other validators.
     *
     *  Pausing may be beneficial in this situation if at least one validator
     *  is known to be on a sequence number earlier than ours. The minimum
     *  number of validators on the same sequence number does not guarantee
     *  consensus, and waiting for all validators may be too time-consuming.
     *  Therefore, a variable threshold is enacted based on the number
     *  of ledgers past network validation that we are on. For the first phase,
     *  the threshold is also the minimum required for quorum. For the last,
     *  no online validators can have a lower sequence number. For intermediate
     *  phases, the threshold is linear between the minimum required for
     *  quorum and 100%. For example, with 3 total phases and a quorum of
     *  80%, the 2nd phase would be 90%. Once the final phase is reached,
     *  if consensus still fails to occur, the cycle is begun again at phase 1.
     *
     * @return Whether to pause to wait for lagging proposers.
     */
    bool
    shouldPause() const;

    bool
    haveConsensus();

    // Update our disputes given that this node has adopted a new position.
    // Will call createDisputes as needed.
    void
    updateDisputes(NodeID_t const& node, TxSet_t const& other);

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

    bool
    peerValidation(
        std::shared_ptr<PeerImp>& peer,
        bool isTrusted,
        std::shared_ptr<protocol::TMConsensus> const& m);
};

}  // namespace ripple

#endif
