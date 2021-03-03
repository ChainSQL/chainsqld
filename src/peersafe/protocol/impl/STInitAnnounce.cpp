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


#include <peersafe/protocol/STInitAnnounce.h>


namespace ripple {


STInitAnnounce::STInitAnnounce(
    SerialIter& sit,
    PublicKey const& publicKey)
    : STObject(getFormat(), sit, sfInitAnnounce)
    , nodePublic_(publicKey)
{
    prevSeq_ = getFieldU32(sfSequence);
    prevHash_ = getFieldH256(sfParentHash);
}

STInitAnnounce::STInitAnnounce(
    std::uint32_t const prevSeq,
    uint256 const& prevHash,
    PublicKey const nodePublic,
    NetClock::time_point signTime)
    : STObject(getFormat(), sfInitAnnounce)
    , prevSeq_(prevSeq)
    , prevHash_(prevHash)
    , nodePublic_(nodePublic)
{
    // This is our own public key and it should always be valid.
    if (!publicKeyType(nodePublic))
        LogicError("Invalid proposeset public key");

    setFieldU32(sfSequence, prevSeq);
    setFieldH256(sfParentHash, prevHash);
    setFieldU32(sfSigningTime, signTime.time_since_epoch().count());
}

Blob
STInitAnnounce::getSerialized() const
{
    Serializer s;
    add(s);
    return s.peekData();
}

SOTemplate const&
STInitAnnounce::getFormat()
{
    struct FormatHolder
    {
        SOTemplate format
        {
            { sfSequence,         soeREQUIRED },    // previousledger Sequence
            { sfParentHash,       soeREQUIRED },    // previousledger Hash
            { sfSigningTime,      soeREQUIRED }     // compute unique ID
        };
    };

    static const FormatHolder holder;

    return holder.format;
}

} // ripple
