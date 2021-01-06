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

#ifndef RIPPLE_APP_CONSENSUS_RCLCXPEERPOS_H_INCLUDED
#define RIPPLE_APP_CONSENSUS_RCLCXPEERPOS_H_INCLUDED

#include <ripple/basics/CountedObject.h>
#include <ripple/basics/base_uint.h>
#include <ripple/beast/hash/hash_append.h>
#include <ripple/json/json_value.h>
#include <ripple/protocol/HashPrefix.h>
#include <ripple/protocol/PublicKey.h>
#include <ripple/protocol/SecretKey.h>
#include <peersafe/protocol/STProposeSet.h>
#include <chrono>
#include <cstdint>
#include <string>

namespace ripple {

/** A peer's signed, proposed position for use in RCLConsensus.

    Carries a ConsensusProposal signed by a peer. Provides value semantics
    but manages shared storage of the peer position internally.
*/
class RCLCxPeerPos
{
public:
    //< The type of the proposed position
    // using Proposal = ConsensusProposal<NodeID, uint256, uint256>;
    using Proposal = STProposeSet;

    /** Constructor

        Constructs a signed peer position.

        @param publicKey Public key of the peer
        @param signature Signature provided with the proposal
        @param suppress Unique id used for hash router suppression
        @param proposal The consensus proposal
    */

    RCLCxPeerPos(
        PublicKey const& publicKey,
        Slice const& signature,
        uint256 const& suppress,
        Proposal&& proposal);

    //! Signature of the proposal (not necessarily verified)
    Slice
    signature() const
    {
        return data_->signature_;
    }

    //! Public key of peer that sent the proposal
    PublicKey const&
    publicKey() const
    {
        return data_->publicKey_;
    }

    //! Unique id used by hash router to suppress duplicates
    uint256 const&
    suppressionID() const
    {
        return data_->suppression_;
    }

    Proposal const&
    proposal() const
    {
        return data_->proposal_;
    }

    //! JSON representation of proposal
    Json::Value
    getJson() const;

private:
    struct Data : public CountedObject<Data>
    {
        PublicKey publicKey_;
        Buffer signature_;
        uint256 suppression_;
        Proposal proposal_;

        Data(
            PublicKey const& publicKey,
            Slice const& signature,
            uint256 const& suppress,
            Proposal&& proposal);

        static char const*
        getCountedObjectName()
        {
            return "RCLCxPeerPos::Data";
        }
    };

    std::shared_ptr<Data> data_;

    template <class Hasher>
    void
    hash_append(Hasher& h) const
    {
        using beast::hash_append;
        hash_append(h, proposal().getSerialized());
    }
};

}  // namespace ripple

#endif
