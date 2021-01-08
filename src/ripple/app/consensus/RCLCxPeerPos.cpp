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

#include <ripple/app/consensus/RCLCxPeerPos.h>
#include <ripple/core/Config.h>
#include <ripple/protocol/HashPrefix.h>
#include <ripple/protocol/Serializer.h>
#include <ripple/protocol/digest.h>
#include <ripple/protocol/jss.h>

namespace ripple {

// Used to construct received proposals
RCLCxPeerPos::RCLCxPeerPos(
    PublicKey const& publicKey,
    Slice const& signature,
    uint256 const& suppression,
    Proposal&& proposal)
    : data_{std::make_shared<Data>(
          publicKey,
          signature,
          suppression,
          std::move(proposal))}
{
}

Json::Value
RCLCxPeerPos::getJson() const
{
    auto ret = proposal().getJson();

    if (publicKey().size())
        ret[jss::peer_id] = toBase58(TokenType::NodePublic, publicKey());

    return ret;
}

RCLCxPeerPos::Data::Data(
    PublicKey const& publicKey,
    Slice const& signature,
    uint256 const& suppress,
    Proposal&& proposal)
    : publicKey_{publicKey}
    , signature_{signature}
    , suppression_{suppress}
    , proposal_{std::move(proposal)}
{
}

}  // namespace ripple
