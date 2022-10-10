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

#ifndef RIPPLE_PROTOCOL_STVALIDATION_H_INCLUDED
#define RIPPLE_PROTOCOL_STVALIDATION_H_INCLUDED

#include <ripple/basics/FeeUnits.h>
#include <ripple/basics/Log.h>
#include <ripple/protocol/STObject.h>
#include <ripple/protocol/PublicKey.h>
#include <ripple/protocol/SecretKey.h>
#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>

namespace ripple {

// Validation flags

// This is a full (as opposed to a partial) validation
constexpr std::uint32_t vfFullValidation = 0x00000001;

// The signature is fully canonical
constexpr std::uint32_t vfFullyCanonicalSig = 0x80000000;

class STValidation final : public STObject, public CountedObject<STValidation>
{
public:
    static char const*
    getCountedObjectName()
    {
        return "STValidation";
    }

    using pointer = std::shared_ptr<STValidation>;
    using ref = const std::shared_ptr<STValidation>&;

    /** Construct a STValidation from a peer.

        Construct a STValidation from serialized data previously shared by a
        peer.

        @param sit Iterator over serialized data
        @param lookupNodeID Invocable with signature
                               NodeID(PublicKey const&)
                            used to find the Node ID based on the public key
                            that signed the validation. For manifest based
                            validators, this should be the NodeID of the master
                            public key.
        @param checkSignature Whether to verify the data was signed properly

        @note Throws if the object is not valid
    */
    template <class LookupNodeID>
    STValidation(
        SerialIter& sit,
        PublicKey const& publicKey,
        LookupNodeID&& lookupNodeID)
        : STObject(validationFormat(), sit, sfValidation)
        , mSignerPublic(publicKey)
    {
        mNodeID = lookupNodeID(publicKey);
        assert(mNodeID.isNonZero());
    }

    /** Construct, sign and trust a new STValidation issued by this node.

        @param signTime When the validation is signed
        @param publicKey The current signing public key
        @param secretKey The current signing secret key
        @param nodeID ID corresponding to node's public master key
        @param f callback function to "fill" the validation with necessary data
    */
    template <typename F>
    STValidation(
        NetClock::time_point signTime,
        PublicKey const& publickey,
        NodeID const& nodeID,
        F&& f)
        : STObject(validationFormat(), sfValidation)
        , mNodeID(nodeID)
        , mSeen(signTime)
        , mSignerPublic(publickey)
    {
        setFieldU32(sfSigningTime, signTime.time_since_epoch().count());

        // Perform additional initialization
        f(*this);

        // Finally, sign the validation and mark it as trusted:
        setFlag(vfFullyCanonicalSig);
        setTrusted();

        // Check to ensure that all required fields are present.
        for (auto const& e : validationFormat())
        {
            if (e.style() == soeREQUIRED && !isFieldPresent(e.sField()))
                LogicError(
                    "Required field '" + e.sField().getName() +
                    "' missing from validation.");
        }
    }

    void
    sign(SecretKey const& secretKey)
    {
        setFieldVL(
            sfSignature,
            signDigest(getSignerPublic(), secretKey, getLedgerHash()));
    }

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

    // Hash of the validated ledger
    uint256
    getLedgerHash() const;

    // Hash of consensus transaction set used to generate ledger
    uint256
    getConsensusHash() const;

    NetClock::time_point
    getSignTime() const;

    bool
    isFull() const;

    NetClock::time_point
    getSeenTime() const
    {
        return mSeen;
    }

    PublicKey const&
    getSignerPublic() const
    {
        return mSignerPublic;
    };

    NodeID
    getNodeID() const
    {
        return mNodeID;
    }

    bool
    isTrusted() const
    {
        return mTrusted;
    }

    void
    setTrusted()
    {
        mTrusted = true;
    }

    void
    setUntrusted()
    {
        mTrusted = false;
    }

    void
    setSeen(NetClock::time_point s)
    {
        mSeen = s;
    }

    Blob
    getSerialized() const;

private:
    static SOTemplate const&
    validationFormat();

    NodeID mNodeID;
    bool mTrusted = false;
    NetClock::time_point mSeen = {};
    PublicKey mSignerPublic;
};

}  // namespace ripple

#endif
