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


#include <peersafe/protocol/STProposal.h>


namespace ripple {


STProposal::STProposal(SerialIter& sit, PublicKey const& publicKey)
    : STObject(getFormat(), sit, sfProposal)
    , nodePublic_(publicKey)
{
    // TODO deserialize
    //block_ = deserialize(getFieldVL(sfBlock));
    //syncInfo_ = deserialize(getFieldVL(sfSyncInfo));
}

STProposal::STProposal(hotstuff::Block const& block, hotstuff::SyncInfo const& syncInfo, PublicKey const nodePublic)
    : STObject(getFormat(), sfProposal)
    , block_(block)
    , syncInfo_(syncInfo)
    , nodePublic_(nodePublic)
{
    // This is our own public key and it should always be valid.
    if (!publicKeyType(nodePublic))
        LogicError("Invalid proposeset public key");

    Blob b, s;
    // TODO serialize
    // b = serialize(block);
    // s = serialize(syncInfo);

    setFieldVL(sfBlock, b);
    setFieldVL(sfSyncInfo, s);
}

Blob STProposal::getSerialized() const
{
    Serializer s;
    add(s);
    return s.peekData();
}

SOTemplate const& STProposal::getFormat()
{
    struct FormatHolder
    {
        SOTemplate format
        {
            { sfBlock,          soeREQUIRED },
            { sfSyncInfo,       soeREQUIRED }
        };
    };

    static const FormatHolder holder;

    return holder.format;
}

} // ripple