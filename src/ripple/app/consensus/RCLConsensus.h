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

#ifndef RIPPLE_APP_CONSENSUS_RCLCONSENSUS_H_INCLUDED
#define RIPPLE_APP_CONSENSUS_RCLCONSENSUS_H_INCLUDED

#include <ripple/app/main/Application.h>
#include <ripple/basics/chrono.h>
#include <ripple/beast/utility/Journal.h>
#include <mutex>
#include <peersafe/consensus/Adaptor.h>
#include <peersafe/consensus/ConsensusBase.h>
#include <peersafe/consensus/ConsensusParams.h>

namespace ripple {

class InboundTransactions;
class LocalTxs;
class LedgerMaster;
class ValidatorKeys;

/** Manages the generic consensus algorithm for use by the RCL.
 */
class RCLConsensus
{
private:
    // Since Consensus does not provide intrinsic thread-safety, this mutex
    // guards all calls to consensus_. adaptor_ uses atomics internally
    // to allow concurrent access of its data members that have getters.
    mutable std::recursive_mutex mutex_;
    using ScopedLockType = std::lock_guard<std::recursive_mutex>;

    ConsensusType type_ = ConsensusType::POP;
    std::shared_ptr<Adaptor> adaptor_;
    std::shared_ptr<ConsensusBase> consensus_;

    ConsensusParms parms_;

    beast::Journal j_;

public:
    //! Constructor
    RCLConsensus(
        Schema& app,
        std::unique_ptr<FeeVote>&& feeVote,
        LedgerMaster& ledgerMaster,
        LocalTxs& localTxs,
        InboundTransactions& inboundTransactions,
        ConsensusBase::clock_type const& clock,
        ValidatorKeys const& validatorKeys,
        beast::Journal journal);

    RCLConsensus(RCLConsensus const&) = delete;

    RCLConsensus&
    operator=(RCLConsensus const&) = delete;

    inline ConsensusType
    consensusType() const
    {
        return type_;
    }

    inline ConsensusPhase
    phase()
    {
        return consensus_->phase_;
    }

    inline ConsensusParms const&
    parms() const
    {
        return parms_;
    }

    inline std::recursive_mutex&
    peekMutex()
    {
        return mutex_;
    }

    inline void
    setGenesisLedgerIndex(LedgerIndex seq)
    {
        consensus_->GENESIS_LEDGER_INDEX = seq;
    }

    inline uint64_t
    getCurrentTurn() const
    {
        return consensus_->getCurrentTurn();
    }

    //! @see Consensus::startRound
    void
    startRound(
        NetClock::time_point const& now,
        RCLCxLedger::ID const& prevLgrId,
        RCLCxLedger const& prevLgr,
        hash_set<NodeID> const& nowUntrusted,
        hash_set<NodeID> const& nowTrusted);

    //! @see Consensus::timerEntry
    void
    timerEntry(NetClock::time_point const& now);

    bool
    waitingForInit()
    {
        return consensus_->waitingForInit();
    }

    bool
    peerConsensusMessage(
        std::shared_ptr<PeerImp>& peer,
        bool isTrusted,
        std::shared_ptr<protocol::TMConsensus> const& m);

    //! @see Consensus::gotTxSet
    void
    gotTxSet(NetClock::time_point const& now, RCLTxSet const& txSet);

    //! @see Consensus::getJson
    Json::Value
    getJson(bool full) const;

    // ----------------------------------------------------------------------
    // RPC server_status interfaces

    //! Whether we are validating consensus ledgers.
    inline bool
    validating() const
    {
        return adaptor_->validating();
    }

    //! Get the number of proposing peers that participated in the previous
    //! round.
    inline std::size_t
    prevProposers() const
    {
        return adaptor_->prevProposers();
    }

    /** Get duration of the previous round.

        The duration of the round is the establish phase, measured from closing
        the open ledger to accepting the consensus result.

        @return Last round duration in milliseconds
    */
    inline std::chrono::milliseconds
    prevRoundTime() const
    {
        return adaptor_->prevRoundTime();
    }

    inline ConsensusMode
    mode() const
    {
        return adaptor_->mode();
    }

    bool
    checkLedgerAccept(std::shared_ptr<Ledger const> const& ledger);

    // ----------------------------------------------------------------------
    // for Pop consensus
    void
    onDeleteUntrusted(hash_set<NodeID> const& nowUntrusted);

    // ----------------------------------------------------------------------
    // RPC ledger_accept interfaces
    void
    simulate(
        NetClock::time_point const& now,
        boost::optional<std::chrono::milliseconds> consensusDelay);

    // ----------------------------------------------------------------------
    static ConsensusType
    stringToConsensusType(std::string const& s);

    static std::string
    conMsgTypeToStr(ConsensusMessageType t);

    std::chrono::milliseconds 
    getConsensusTimeOut() const;
};

}  // namespace ripple

#endif
