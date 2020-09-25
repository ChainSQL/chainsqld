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
#include <ripple/core/TimeKeeper.h>
#include <ripple/protocol/digest.h>
#include <ripple/app/consensus/RCLCxLedger.h>
#include <ripple/app/consensus/RCLCxPeerPos.h>
#include <ripple/app/consensus/RCLCxTx.h>
#include <ripple/app/ledger/InboundTransactions.h>
#include <ripple/app/ledger/OpenLedger.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/misc/FeeVote.h>
#include <ripple/app/misc/ValidatorList.h>
#include <peersafe/consensus/ConsensusTypes.h>
#include <boost/optional.hpp>
#include <memory>


namespace ripple {


class ValidatorKeys;


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
    InboundTransactions&        inboundTransactions_;

    NodeID const    nodeID_;
    PublicKey const valPublic_;
    SecretKey const valSecret_;

    // Ledger we most recently needed to acquire
    LedgerHash              acquiringLedger_;

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

    virtual inline NetClock::time_point closeTime() const { return app_.timeKeeper().closeTime(); }

    virtual inline PublicKey getMasterKey(PublicKey pk) const { return app_.validatorManifests().getMasterKey(pk); }

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

    /** Relay the given tx set to peers.

        @param set The TxSet to share.
    */
    virtual inline void relay(RCLTxSet const& set)
    {
        inboundTransactions_.giveSet(set.id(), set.map_, false);
    }

    virtual void signAndSendMessage(protocol::TMConsensus &consensus);

    /** Acquire the transaction set associated with a proposal.

        If the transaction set is not available locally, will attempt
        acquire it from the network.

        @param setId The transaction set ID associated with the proposal
        @return Optional set of transactions, seated if available.
    */
    virtual boost::optional<RCLTxSet> acquireTxSet(RCLTxSet::ID const& setId);

    /** Attempt to acquire a specific ledger.

        If not available, asynchronously acquires from the network.

        @param hash The ID/hash of the ledger acquire
        @return Optional ledger, will be seated if we locally had the ledger
    */
    virtual boost::optional<RCLCxLedger> acquireLedger(LedgerHash const& hash);

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
        Json::Value&& consensusJson) = 0;

    virtual bool checkLedgerAccept(std::shared_ptr<Ledger const> const& ledger) = 0;

    /** Notified of change in consensus mode

        @param before The prior consensus mode
        @param after The new consensus mode
    */
    virtual void onModeChange(ConsensusMode before, ConsensusMode after);
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


inline uint256 consensusMessageUniqueId(protocol::TMConsensus const& m)
{
    return sha512Half(
        makeSlice(m.msg()),
        PublicKey{ makeSlice(m.signerpubkey()) });
}

}

#endif
