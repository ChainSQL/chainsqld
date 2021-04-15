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


#include <ripple/protocol/jss.h>
#include <peersafe/protocol/STProposeSet.h>
#include <ripple/app/consensus/RCLCxTx.h>
#include <peersafe/schema/Schema.h>


namespace ripple {


STProposeSet::STProposeSet(
    SerialIter& sit,
    NetClock::time_point now,
    NodeID const& nodeid,
    PublicKey const& publicKey)
    : STObject(getFormat(), sit, sfProposeSet)
    , time_(now)
    , nodeID_(nodeid)
    , signerPublic_(publicKey)

{
    using tp = NetClock::time_point;
    using d = tp::duration;

    assert(nodeID_.isNonZero());

    proposeSeq_ = getFieldU32(sfSequence);
    position_ = getFieldH256(sfConsensusHash);
    previousLedger_ = getFieldH256(sfParentHash);
    closeTime_ = tp(d(getFieldU32(sfCloseTime)));

    if (isFieldPresent(sfLedgerSequence))
    {
        curLedgerSeq_ = getFieldU32(sfLedgerSequence);
    }

    if (isFieldPresent(sfView))
    {
        view_ = getFieldU64(sfView);
    }
}

STProposeSet::STProposeSet(
    std::uint32_t proposeSeq,
    uint256 const& position,
    uint256 const& preLedgerHash,
    NetClock::time_point closetime,
    NetClock::time_point now,
    NodeID const& nodeid,
    PublicKey const& publicKey)
    : STObject(getFormat(), sfProposeSet)
    , proposeSeq_(proposeSeq)
    , position_(position)
    , previousLedger_(preLedgerHash)
    , closeTime_(closetime)
    , time_(now)
    , nodeID_(nodeid)
    , signerPublic_(publicKey)
{
    // This is our own public key and it should always be valid.
    if (!publicKeyType(publicKey))
        LogicError("Invalid proposeset public key");

    setFieldU32(sfSequence, proposeSeq);
    setFieldH256(sfConsensusHash, position);
    setFieldH256(sfParentHash, preLedgerHash);
    setFieldU32(sfCloseTime, closetime.time_since_epoch().count());
}

STProposeSet::STProposeSet(
    std::uint32_t proposeSeq,
    uint256 const& position,
    uint256 const& preLedgerHash,
    NetClock::time_point closetime,
    NetClock::time_point now,
    NodeID const& nodeid,
    PublicKey const& publicKey,
    std::uint32_t ledgerSeq,
    std::uint64_t view,
    RCLTxSet const& set)
    : STObject(getFormat(), sfProposeSet)
    , proposeSeq_(proposeSeq)
    , position_(position)
    , previousLedger_(preLedgerHash)
    , closeTime_(closetime)
    , time_(now)
    , nodeID_(nodeid)
    , signerPublic_(publicKey)
    , curLedgerSeq_(ledgerSeq)
    , view_(view)
{
    // This is our own public key and it should always be valid.
    if (!publicKeyType(publicKey))
        LogicError("Invalid proposeset public key");

    setFieldU32(sfSequence, proposeSeq);
    setFieldH256(sfConsensusHash, position);
    setFieldH256(sfParentHash, preLedgerHash);
    setFieldU32(sfCloseTime, closetime.time_since_epoch().count());

    setFieldU32(sfLedgerSequence, ledgerSeq);
    setFieldU64(sfView, view);

    if (set.map_ != nullptr)
    {
        STArray txs;
        auto& txMap = *set.map_;
        for (auto const& item : txMap)
        {
            STObject obj(sfNewFields);
            obj.setFieldH256(sfTransactionHash, item.key());
            obj.setFieldVL(sfRaw, item.slice());
            txs.push_back(obj);
        }
        setFieldArray(sfTransactions, txs);
    }
}

void STProposeSet::changePosition(
    uint256 const& newPosition,
    NetClock::time_point newCloseTime,
    NetClock::time_point now)
{
    setFieldH256(sfConsensusHash, newPosition);
    setFieldU32(sfCloseTime, newCloseTime.time_since_epoch().count());

    position_ = newPosition;
    closeTime_ = newCloseTime;
    time_ = now;

    if (proposeSeq_ != seqLeave)
    {
        ++proposeSeq_;
        setFieldU32(sfSequence, proposeSeq_);
    }
}

std::shared_ptr<RCLTxSet>
STProposeSet::getTxSet(Schema& app) const
{
    if (isFieldPresent(sfTransactions))
    {
        auto initialSet = std::make_shared<SHAMap>(
            SHAMapType::TRANSACTION, app.getNodeFamily());
        initialSet->setUnbacked();

        auto& txs = getFieldArray(sfTransactions);
        for (auto& obj : txs)
        {
            uint256 txID = obj.getFieldH256(sfTransactionHash);
            auto raw = obj.getFieldVL(sfRaw);
            Serializer s(raw.data(), raw.size());
            initialSet->addItem(
                SHAMapItem(txID, std::move(s)), true, false);
        }
        return std::make_shared<RCLTxSet>(initialSet);
    }
    else
    {
        return nullptr;
    }
}

void STProposeSet::bowOut(NetClock::time_point now)
{
    time_ = now;
    proposeSeq_ = seqLeave;

    setFieldU32(sfSequence, proposeSeq_);
}

Blob STProposeSet::getSerialized() const
{
    Serializer s;
    add(s);
    return s.peekData();
}

Json::Value STProposeSet::getJson() const
{
    using std::to_string;

    Json::Value ret = Json::objectValue;
    ret[jss::previous_ledger] = to_string(previousLedger_);
    ret[jss::ledger_current_index] = curLedgerSeq_;

    if (!isBowOut())
    {
        ret[jss::transaction_hash] = to_string(position_);
        ret[jss::propose_seq] = proposeSeq_;
    }

    ret[jss::close_time] =
        to_string(closeTime_.time_since_epoch().count());

    return ret;
}

SOTemplate const& STProposeSet::getFormat()
{
    struct FormatHolder
    {
        SOTemplate format
        {
            { sfSequence,         soeREQUIRED },    // proposeSeq
            { sfConsensusHash,    soeREQUIRED },    // currentTxSetHash
            { sfParentHash,       soeREQUIRED },    // previousledger Hash
            { sfCloseTime,        soeREQUIRED },    // closeTime
            { sfLedgerSequence,   soeOPTIONAL },
            { sfView,             soeOPTIONAL },
            { sfTransactions,     soeOPTIONAL },

        };
    };

    static const FormatHolder holder;

    return holder.format;
}

} // ripple
