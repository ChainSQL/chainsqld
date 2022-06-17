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


#include <ripple/basics/base_uint.h>
#include <ripple/protocol/jss.h>
#include <peersafe/protocol/STViewChange.h>


namespace ripple {


STViewChange::STViewChange(
    SerialIter& sit,
    PublicKey const& publicKey)
    : STObject(getFormat(), sit, sfViewChange)
    , nodePublic_(publicKey)
{
    prevSeq_ = getFieldU32(sfSequence);
    prevHash_ = getFieldH256(sfParentHash);
    toView_ = getFieldU64(sfView);
}

STViewChange::STViewChange(
    std::uint32_t const prevSeq,
    uint256 const& prevHash,
    std::uint64_t const& toView,
    PublicKey const nodePublic,
    NetClock::time_point signTime,
    std::uint32_t const validatedSeq)
    : STObject(getFormat(), sfViewChange)
    , prevSeq_(prevSeq)
    , prevHash_(prevHash)
    , toView_(toView)
    , nodePublic_(nodePublic)
    , validatedSeq_(validatedSeq)
{
    // This is our own public key and it should always be valid.
    if (!publicKeyType(nodePublic))
        LogicError("Invalid proposeset public key");

    setFieldU32(sfSequence, prevSeq);
    setFieldH256(sfParentHash, prevHash);
    setFieldU64(sfView, toView);
    setFieldU32(sfSigningTime, signTime.time_since_epoch().count());
    setFieldU32(sfValidatedSequence, validatedSeq);
}

Blob STViewChange::getSerialized() const
{
    Serializer s;
    add(s);
    return s.peekData();
}

Json::Value
STViewChange::getJson(bool withView) const
{
    Json::Value ret(Json::objectValue);

    if (withView)
        ret[jss::view] = (Json::UInt)toView_;

    ret[jss::PreviousHash] = to_string(prevHash_);
    ret[jss::PreviousSeq] = prevSeq_;
    ret[jss::public_key] = toBase58(TokenType::NodePublic, nodePublic_);

    return ret;
}

SOTemplate const& STViewChange::getFormat()
{
    struct FormatHolder
    {
        SOTemplate format
        {
            { sfSequence,         soeREQUIRED },    // previousledger Sequence
            { sfParentHash,       soeREQUIRED },    // previousledger Hash
            { sfView,             soeREQUIRED },    // toView
            { sfSigningTime,      soeREQUIRED },     // compute unique ID
            { sfValidatedSequence,soeOPTIONAL }     // validatedLedger Sequence
        };
    };

    static const FormatHolder holder;

    return holder.format;
}

} // ripple
