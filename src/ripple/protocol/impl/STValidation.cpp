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


#include <ripple/protocol/STValidation.h>

namespace ripple {


uint256
STValidation::getLedgerHash() const
{
    return getFieldH256(sfLedgerHash);
}

uint256
STValidation::getConsensusHash() const
{
    return getFieldH256(sfConsensusHash);
}

NetClock::time_point
STValidation::getSignTime() const
{
    return NetClock::time_point{NetClock::duration{getFieldU32(sfSigningTime)}};
}

bool
STValidation::isFull() const
{
    return (getFlags() & vfFullValidation) != 0;
}

Blob
STValidation::getSerialized() const
{
    Serializer s;
    add(s);
    return s.peekData();
}

SOTemplate const&
STValidation::validationFormat()
{
    static SOTemplate const format{
        { sfFlags,              soeREQUIRED},
        { sfLedgerHash,         soeREQUIRED},
        { sfSignature,          soeOPTIONAL},
        { sfLedgerSequence,     soeOPTIONAL},
        { sfCloseTime,          soeOPTIONAL},
        { sfLoadFee,            soeOPTIONAL},
        { sfAmendments,         soeOPTIONAL},
        { sfBaseFee,            soeOPTIONAL},
        { sfReserveBase,        soeOPTIONAL},
        { sfReserveIncrement,   soeOPTIONAL},
        { sfSigningTime,        soeREQUIRED},
        { sfConsensusHash,      soeOPTIONAL},
        { sfCookie,             soeOPTIONAL},
        { sfValidatedHash,      soeOPTIONAL},
        { sfServerVersion,      soeOPTIONAL},
        { sfDropsPerByte,       soeOPTIONAL},
        { sfGasPrice,           soeOPTIONAL},
        { sfSigningPubKey,      soeOPTIONAL},
    };

    return format;
}

}  // namespace ripple
