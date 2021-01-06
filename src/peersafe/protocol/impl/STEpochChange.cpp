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


#include <peersafe/protocol/STEpochChange.h>
#include <peersafe/serialization/hotstuff/EpochChange.h>
#include <peersafe/serialization/hotstuff/SyncInfo.h>


namespace ripple {


STEpochChange::STEpochChange(SerialIter& sit, PublicKey const& publicKey)
    : STObject(getFormat(), sit, sfEpochChange)
    , nodePublic_(publicKey)
{
    // deserialize
    epochChange_ = serialization::deserialize<hotstuff::EpochChange>(Buffer(makeSlice(getFieldVL(sfEpochChangeImp))));
    syncInfo_ = serialization::deserialize<hotstuff::SyncInfo>(Buffer(makeSlice(getFieldVL(sfSyncInfo))));
}

STEpochChange::STEpochChange(hotstuff::EpochChange const& epochChange, hotstuff::SyncInfo const& syncInfo, PublicKey const nodePublic)
    : STObject(getFormat(), sfEpochChange)
    , epochChange_(epochChange)
    , syncInfo_(syncInfo)
    , nodePublic_(nodePublic)
{
    // This is our own public key and it should always be valid.
    if (!publicKeyType(nodePublic))
        LogicError("Invalid proposeset public key");

    Buffer e(std::move(serialization::serialize(epochChange)));
    Buffer s(std::move(serialization::serialize(syncInfo)));

    setFieldVL(sfEpochChangeImp, Slice(e.data(), e.size()));
    setFieldVL(sfSyncInfo, Slice(s.data(), s.size()));
}

Blob STEpochChange::getSerialized() const
{
    Serializer s;
    add(s);
    return s.peekData();
}

SOTemplate const& STEpochChange::getFormat()
{
    struct FormatHolder
    {
        SOTemplate format
        {
            { sfEpochChangeImp, soeREQUIRED },
            { sfSyncInfo,       soeREQUIRED }
        };
    };

    static const FormatHolder holder;

    return holder.format;
}

} // ripple