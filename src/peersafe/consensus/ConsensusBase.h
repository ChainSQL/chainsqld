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

#ifndef PEERSAFE_CONSENSUS_BASE_H_INCLUDED
#define PEERSAFE_CONSENSUS_BASE_H_INCLUDED


#include <ripple/basics/chrono.h>
#include <ripple/json/json_value.h>
#include <ripple/beast/utility/Journal.h>
#include <peersafe/consensus/Adaptor.h>
#include <peersafe/consensus/ConsensusTypes.h>


namespace ripple {

enum ConsensusType : std::uint8_t {
    RPCA = 0,
    POP = 1,
    HOTSTUFF = 2,

    UNKNOWN,
};

enum ConsensusMessageType {
    mtPROPOSESET = 0,
    mtVALIDATION = 1,
    mtVIEWCHANGE = 2,
    mtPROPOSAL = 3,
    mtVOTE = 4,
    mtACQUIREBLOCK = 5,  // acquire hotstuff block
    mtBLOCKDATA = 6,     // provide hotstuff block
    mtINITANNOUNCE = 7,
    mtEPOCHCHANGE = 8,
    mtACQUIRVALIDATIONSET = 9,
    mtVALIDATIONSETDATA = 10

};

class PeerImp;

class ConsensusBase
{
public:
    using TxSet_t = typename Adaptor::TxSet_t;
    using Tx_t = typename TxSet_t::Tx;
    using Ledger_t = typename Adaptor::Ledger_t;
    using NodeID_t = typename Adaptor::NodeID_t;
    using PeerPosition_t = typename Adaptor::PeerPosition_t;
    using Result = typename Adaptor::Result;
    using Proposal_t = STProposeSet;
    //! Clock type for measuring time within the consensus code
    using clock_type = beast::abstract_clock<std::chrono::steady_clock>;
    using ScopedLockType = std::lock_guard<std::recursive_mutex>;

    Ledger_t::Seq GENESIS_LEDGER_INDEX = 1;

public:
    clock_type const& clock_;
    // Journal for debugging
    beast::Journal j_;

    ConsensusPhase phase_{ConsensusPhase::accepted};
    MonitoredMode mode_{ConsensusMode::observing};

    // Last validated ledger ID provided to consensus
    typename Ledger_t::ID prevLedgerID_;
    Ledger_t::Seq prevLedgerSeq_;

    // Last validated ledger seen by consensus
    Ledger_t previousLedger_;

public:
    ConsensusBase(clock_type const& clock, beast::Journal journal)
        : clock_(clock), j_(journal)
    {
    }

    virtual ~ConsensusBase() = default;

    virtual void
    startRound(
        NetClock::time_point const& now,
        typename Ledger_t::ID const& prevLedgerID,
        Ledger_t prevLedger,
        hash_set<NodeID> const& nowUntrusted,
        bool proposing) = 0;

    virtual void
    timerEntry(NetClock::time_point const& now) = 0;

    virtual bool
    peerConsensusMessage(
        std::shared_ptr<PeerImp>& peer,
        bool isTrusted,
        std::shared_ptr<protocol::TMConsensus> const& m) = 0;

    virtual void
    gotTxSet(NetClock::time_point const& now, TxSet_t const& txSet) = 0;

    virtual Json::Value
    getJson(bool full) const = 0;

    virtual bool
    waitingForInit() const = 0;

    virtual uint64_t
    getCurrentTurn() const { return 0; }

    // -----------------------------------------------------------------------

    // Pop sepcific, for child schema deleted validators
    virtual void
    onDeleteUntrusted(hash_set<NodeID> const& nowUntrusted)
    {
    }

    // Rpca specific
    virtual void
    simulate(
        NetClock::time_point const& now,
        boost::optional<std::chrono::milliseconds> consensusDelay)
    {
    }
};

}  // namespace ripple

#endif