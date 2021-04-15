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

#ifndef PEERSAFE_PROTOCOL_STPROPOSESET_H_INCLUDED
#define PEERSAFE_PROTOCOL_STPROPOSESET_H_INCLUDED


#include <ripple/protocol/UintTypes.h>
#include <ripple/protocol/STObject.h>
#include <ripple/protocol/PublicKey.h>


namespace ripple {

class RCLTxSet;
class Schema;
class STProposeSet final : public STObject, public CountedObject<STProposeSet>
{
public:
    //< Sequence value when a peer initially joins consensus
    static std::uint32_t const seqJoin = 0;

    //< Sequence number when  a peer wants to bow out and leave consensus
    static std::uint32_t const seqLeave = 0xffffffff;

    static char const* getCountedObjectName()
    {
        return "STProposeSet";
    }

    using pointer = std::shared_ptr<STProposeSet>;
    using ref = const std::shared_ptr<STProposeSet>&;

    STProposeSet(
        SerialIter& sit,
        NetClock::time_point now,
        NodeID const& nodeid,
        PublicKey const& publicKey);

    STProposeSet(
        std::uint32_t proposeSeq,
        uint256 const& position,
        uint256 const& preLedgerHash,
        NetClock::time_point closetime,
        NetClock::time_point now,
        NodeID const& nodeid,
        PublicKey const& publicKey);

    STProposeSet(
        std::uint32_t proposeSeq,
        uint256 const& position,
        uint256 const& preLedgerHash,
        NetClock::time_point closetime,
        NetClock::time_point now,
        NodeID const& nodeid,
        PublicKey const& publicKey,
        std::uint32_t ledgerSeq,
        std::uint64_t view,
        RCLTxSet const& set);

    STBase*
    copy(std::size_t n, void* buf) const override
    {
        return emplace(n, buf, *this);
    }

    STBase*
    move(std::size_t n, void* buf) override
    {
        return emplace(n, buf, std::move(*this));
    }

    std::uint32_t proposeSeq() const { return proposeSeq_; }
    uint256 const& position() const { return position_; }
    uint256 const& prevLedger() const { return previousLedger_; }
    NetClock::time_point const& closeTime() const { return closeTime_; }
    NetClock::time_point const& seenTime() const { return time_; }
    NodeID const& nodeID() const { return nodeID_; }
    PublicKey const& getSignerPublic() const { return signerPublic_; }
    std::uint32_t const& curLedgerSeq() const { return curLedgerSeq_; }
    std::uint64_t const& view() const { return view_; }

    bool isInitial() const { return proposeSeq_ == seqJoin; }
    bool isBowOut() const { return proposeSeq_ == seqLeave; }
    bool isStale(NetClock::time_point cutoff) const { return time_ <= cutoff; }

    void changePosition(
        uint256 const& newPosition,
        NetClock::time_point newCloseTime,
        NetClock::time_point now);

    void bowOut(NetClock::time_point now);

    Blob getSerialized() const;

    Json::Value getJson() const;

    std::shared_ptr<RCLTxSet>
    getTxSet(Schema& app) const;

private:
    //! The sequence number of these positions taken by this node
    std::uint32_t proposeSeq_;

    //! Unique identifier of the position this proposal is taking
    uint256 position_;

    //! Unique identifier of prior ledger this proposal is based on
    uint256 previousLedger_;

    //! The ledger close time this position is taking
    NetClock::time_point closeTime_;

    // !The time this position was last updated
    NetClock::time_point time_;

    //! The identifier of the node taking this position
    NodeID nodeID_;

    PublicKey signerPublic_;

    std::uint32_t curLedgerSeq_ = 0;
    std::uint64_t view_ = 0;

private:
    static SOTemplate const& getFormat();
};

} // ripple

#endif
