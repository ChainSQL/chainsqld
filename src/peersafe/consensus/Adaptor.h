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


#ifndef PEERSAFE_CONSENSUS_ADAPTOR_H_INCLUDE
#define PEERSAFE_CONSENSUS_ADAPTOR_H_INCLUDE


#include "ripple.pb.h"
#include <ripple/app/consensus/RCLValidations.h>
#include <ripple/app/consensus/RCLCxLedger.h>
#include <ripple/app/consensus/RCLCxPeerPos.h>
#include <ripple/app/consensus/RCLCxTx.h>
#include <ripple/app/ledger/InboundTransactions.h>
#include <ripple/app/ledger/OpenLedger.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/misc/FeeVote.h>
#include <ripple/app/misc/ValidatorList.h>
#include <peersafe/consensus/ConsensusTypes.h>
#include <peersafe/consensus/ViewChange.h>
#include <boost/optional.hpp>
#include <memory>


namespace ripple {


class ValidatorKeys;
class LocalTxs;


// Implements the Adaptor template interface required by Consensus.
class Adaptor
{
public:
    using Ledger_t          = RCLCxLedger;
    using NodeID_t          = NodeID;
    using NodeKey_t         = PublicKey;
    using TxSet_t           = RCLTxSet;
    using PeerPosition_t    = RCLCxPeerPos;
    using Result            = ConsensusResult<Adaptor>;

public:
    Application&                app_;
    beast::Journal              j_;
    std::unique_ptr<FeeVote>    feeVote_;
    LedgerMaster&               ledgerMaster_;
    LocalTxs&                   localTxs_;
    InboundTransactions&        inboundTransactions_;

    NodeID const    nodeID_;
    PublicKey const valPublic_;
    SecretKey const valSecret_;

    // Ledger we most recently needed to acquire
    LedgerHash      acquiringLedger_;

    // The timestamp of the last validation we used
    NetClock::time_point lastValidationTime_;

    // save next proposal
    std::map<std::uint32_t, std::map<PublicKey, RCLCxPeerPos>> proposalCache_;

    // These members are queried via public accesors and are atomic for
    // thread safety.
    std::atomic<bool>                       validating_{ false };
    std::atomic<std::size_t>                prevProposers_{ 0 };
    std::atomic<std::chrono::milliseconds>  prevRoundTime_{ std::chrono::milliseconds{0} };
    std::atomic<ConsensusMode>              mode_{ ConsensusMode::observing };

public:
    Adaptor(
        Application& app,
        std::unique_ptr<FeeVote>&& feeVote,
        LedgerMaster& ledgerMaster,
        LocalTxs& localTxs,
        InboundTransactions& inboundTransactions,
        ValidatorKeys const & validatorKeys,
        beast::Journal journal);

    virtual inline bool validating() const { return validating_; }
    virtual inline std::size_t prevProposers() const { return prevProposers_; }
    virtual inline std::chrono::milliseconds prevRoundTime() const { return prevRoundTime_; }
    virtual inline ConsensusMode mode() const { return mode_; }
    virtual inline NodeID_t const& nodeID() const { return nodeID_; }

    virtual inline bool haveValidated() const { return ledgerMaster_.haveValidated(); };
    virtual inline LedgerIndex getValidLedgerIndex() const { return ledgerMaster_.getValidLedgerIndex(); }

    virtual inline std::pair<std::size_t, hash_set<NodeKey_t>> getQuorumKeys() const
    {
        return app_.validators().getQuorumKeys();
    }

    virtual inline std::size_t laggards(Ledger_t::Seq const seq, hash_set<NodeKey_t >& trustedKeys) const
    {
        return app_.getValidations().laggards(seq, trustedKeys);
    }

    /** Number of proposers that have vallidated the given ledger

        @param h The hash of the ledger of interest
        @return the number of proposers that validated a ledger
    */
    virtual inline std::size_t proposersValidated(LedgerHash const& h) const
    {
        return app_.getValidations().numTrustedForLedger(h);
    }

    virtual inline bool validator() const
    {
        return !valPublic_.empty();
    }

    /** Whether the open ledger has any transactions */
    virtual inline bool hasOpenTransactions() const
    {
        return !app_.openLedger().empty();
    }

    /** Called before kicking off a new consensus round.

        @param prevLedger Ledger that will be prior ledger for next round
        @return Whether we enter the round proposing
    */
    virtual bool preStartRound(RCLCxLedger const & prevLedger);

    /** Notify peers of a consensus state change

        @param ne Event type for notification
        @param ledger The ledger at the time of the state change
        @param haveCorrectLCL Whether we believ we have the correct LCL.
    */
    virtual void notify(
        protocol::NodeEvent ne,
        RCLCxLedger const& ledger,
        bool haveCorrectLCL);

    /** Attempt to acquire a specific ledger.

        If not available, asynchronously acquires from the network.

        @param hash The ID/hash of the ledger acquire
        @return Optional ledger, will be seated if we locally had the ledger
    */
    virtual boost::optional<RCLCxLedger> acquireLedger(LedgerHash const& hash);

    /** Relay the given proposal to all peers

        @param peerPos The peer position to relay.
        */
    virtual void relay(RCLCxPeerPos const& peerPos);

    /** Relay disputed transacction to peers.

        Only relay if the provided transaction hasn't been shared recently.

        @param tx The disputed transaction to relay.
    */
    virtual void relay(RCLCxTx const& tx);

    /** Relay the given tx set to peers.

        @param set The TxSet to share.
    */
    virtual inline void relay(RCLTxSet const& set)
    {
        inboundTransactions_.giveSet(set.id(), set.map_, false);
    }

    /** Acquire the transaction set associated with a proposal.

        If the transaction set is not available locally, will attempt
        acquire it from the network.

        @param setId The transaction set ID associated with the proposal
        @return Optional set of transactions, seated if available.
    */
    virtual boost::optional<RCLTxSet> acquireTxSet(RCLTxSet::ID const& setId);

    /** Number of proposers that have validated a ledger descended from
        requested ledger.

        @param ledger The current working ledger
        @param h The hash of the preferred working ledger
        @return The number of validating peers that have validated a ledger
                descended from the preferred working ledger.
    */
    virtual std::size_t proposersFinished(RCLCxLedger const & ledger, LedgerHash const& h) const;

    /** Propose the given position to my peers.

        @param proposal Our proposed position
    */
    virtual void propose(RCLCxPeerPos::Proposal const& proposal);

    /** Get the ID of the previous ledger/last closed ledger(LCL) on the
        network

        @param ledgerID ID of previous ledger used by consensus
        @param ledger Previous ledger consensus has available
        @param mode Current consensus mode
        @return The id of the last closed network

        @note ledgerID may not match ledger.id() if we haven't acquired
                the ledger matching ledgerID from the network
        */
    virtual uint256 getPrevLedger(
        uint256 ledgerID,
        RCLCxLedger const& ledger,
        ConsensusMode mode);

    /** Notified of change in consensus mode

        @param before The prior consensus mode
        @param after The new consensus mode
    */
    virtual void onModeChange(ConsensusMode before, ConsensusMode after);

    virtual Result onCollectFinish(
        RCLCxLedger const& ledger,
        std::vector<uint256> const& transactions,
        NetClock::time_point const& closeTime,
        std::uint64_t const& view,
        ConsensusMode mode);

    virtual void onViewChanged(bool bWaitingInit, Ledger_t previousLedger);

    /** Process the accepted ledger.

        @param result The result of consensus
        @param prevLedger The closed ledger consensus worked from
        @param closeResolution The resolution used in agreeing on an
                                effective closeTime
        @param rawCloseTimes The unrounded closetimes of ourself and our
                                peers
        @param mode Our participating mode at the time consensus was
                    declared
        @param consensusJson Json representation of consensus state
    */
    virtual void onAccept(
        Result const& result,
        RCLCxLedger const& prevLedger,
        NetClock::duration const& closeResolution,
        ConsensusCloseTimes const& rawCloseTimes,
        ConsensusMode const& mode,
        Json::Value&& consensusJson);

    /** Process the accepted ledger that was a result of simulation/force
        accept.

        @ref onAccept
    */
    virtual void onForceAccept(
        Result const& result,
        RCLCxLedger const& prevLedger,
        NetClock::duration const& closeResolution,
        ConsensusCloseTimes const& rawCloseTimes,
        ConsensusMode const& mode,
        Json::Value&& consensusJson);

    virtual auto checkLedgerAccept(uint256 const& hash, std::uint32_t seq)
        -> std::pair<std::shared_ptr<Ledger const> const, bool>;
    virtual bool checkLedgerAccept(std::shared_ptr<Ledger const> const& ledger);

    /** Send view change message. */
    virtual void sendViewChange(ViewChange const& proposal);

    virtual void touchAcquringLedger(LedgerHash const& prevLedgerHash);

    /** Handle a new validation

        Also sets the trust status of a validation based on the validating node's
        public key and this node's current UNL.

        @param app Application object containing validations and ledgerMaster
        @param val The validation to add
        @param source Name associated with validation used in logging

        @return Whether the validation should be relayed
    */
    virtual bool handleNewValidation(STValidation::ref val, std::string const& source);

private:
    /** Accept a new ledger based on the given transactions.

        @ref onAccept
    */
    virtual void doAccept(
        Result const& result,
        RCLCxLedger const& prevLedger,
        NetClock::duration closeResolution,
        ConsensusCloseTimes const& rawCloseTimes,
        ConsensusMode const& mode,
        Json::Value&& consensusJson)
    {}

protected:
    /** Build the new last closed ledger.

        Accept the given the provided set of consensus transactions and
        build the last closed ledger. Since consensus just agrees on which
        transactions to apply, but not whether they make it into the closed
        ledger, this function also populates retriableTxs with those that
        can be retried in the next round.

        @param previousLedger Prior ledger building upon
        @param retriableTxs On entry, the set of transactions to apply to
                            the ledger; on return, the set of transactions
                            to retry in the next round.
        @param closeTime The time the ledger closed
        @param closeTimeCorrect Whether consensus agreed on close time
        @param closeResolution Resolution used to determine consensus close
                                time
        @param roundTime Duration of this consensus rorund
        @param failedTxs Populate with transactions that we could not
                            successfully apply.
        @return The newly built ledger
    */
    virtual RCLCxLedger buildLCL(
        RCLCxLedger const& previousLedger,
        CanonicalTXSet& retriableTxs,
        NetClock::time_point closeTime,
        bool closeTimeCorrect,
        NetClock::duration closeResolution,
        std::chrono::milliseconds roundTime,
        std::set<TxID>& failedTxs);

    /** Report that the consensus process built a particular ledger */
    virtual void consensusBuilt(
        std::shared_ptr<Ledger const> const& ledger,
        uint256 const& consensusHash,
        Json::Value consensus);

    /**
     * Determines how many validations are needed to fully validate a ledger
     *
     * @return Number of validations needed
     */
    virtual inline std::size_t getNeededValidations();

    /** Validate the given ledger and share with peers as necessary

        @param ledger The ledger to validate
        @param txns The consensus transaction set
        @param proposing Whether we were proposing transactions while
                         generating this ledger.  If we are not proposing,
                         a validation can still be sent to inform peers that
                         we know we aren't fully participating in consensus
                         but are still around and trying to catch up.
    */
    virtual void validate(RCLCxLedger const& ledger, RCLTxSet const& txns, bool proposing);
};


// Helper class to ensure adaptor is notified whenver the ConsensusMode
// changes
class MonitoredMode
{
    ConsensusMode mode_;

public:
    MonitoredMode(ConsensusMode m) : mode_{ m }
    {
    }

    ConsensusMode get() const
    {
        return mode_;
    }

    void set(ConsensusMode mode, Adaptor& a)
    {
        a.onModeChange(mode_, mode);
        mode_ = mode;
    }
};

}

#endif
