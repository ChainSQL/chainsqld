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


#include <peersafe/protocol/STVote.h>
#include <peersafe/serialization/hotstuff/Vote.h>
#include <peersafe/serialization/hotstuff/SyncInfo.h>


namespace ripple {


STVote::STVote(SerialIter& sit, PublicKey const& publicKey)
    : STObject(getFormat(), sit, sfVote)
    , nodePublic_(publicKey)
{
    // deserialize
    vote_ = serialization::deserialize<hotstuff::Vote>(Buffer(makeSlice(getFieldVL(sfVoteImp))));
    syncInfo_ = serialization::deserialize<hotstuff::SyncInfo>(Buffer(makeSlice(getFieldVL(sfSyncInfo))));
}

STVote::STVote(hotstuff::Vote const& vote, hotstuff::SyncInfo const& syncInfo, PublicKey const nodePublic)
    : STObject(getFormat(), sfVote)
    , vote_(vote)
    , syncInfo_(syncInfo)
    , nodePublic_(nodePublic)
{
    // This is our own public key and it should always be valid.
    if (!publicKeyType(nodePublic))
        LogicError("Invalid proposeset public key");

    Buffer const& v = serialization::serialize(vote);
    Buffer const& s = serialization::serialize(syncInfo);

    setFieldVL(sfVoteImp, Slice(v.data(), v.size()));
    setFieldVL(sfSyncInfo, Slice(s.data(), s.size()));
}

Blob STVote::getSerialized() const
{
    Serializer s;
    add(s);
    return s.peekData();
}

SOTemplate const& STVote::getFormat()
{
    struct FormatHolder
    {
        SOTemplate format
        {
            { sfVoteImp,        soeREQUIRED },
            { sfSyncInfo,       soeREQUIRED }
        };
    };

    static const FormatHolder holder;

    return holder.format;
}

} // ripple