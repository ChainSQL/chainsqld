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

#ifndef PEERSAFE_PROTOCOL_STVIEWCHANGE_H_INCLUDED
#define PEERSAFE_PROTOCOL_STVIEWCHANGE_H_INCLUDED


#include <ripple/protocol/UintTypes.h>
#include <ripple/protocol/STObject.h>
#include <ripple/protocol/PublicKey.h>


namespace ripple {


class STViewChange final : public STObject, public CountedObject<STViewChange>
{
public:
    static char const* getCountedObjectName()
    {
        return "STViewChange";
    }

    using pointer = std::shared_ptr<STViewChange>;
    using ref = const std::shared_ptr<STViewChange>&;

    STViewChange(
        SerialIter& sit,
        PublicKey const& publicKey);

    STViewChange(
        std::uint32_t const prevSeq,
        uint256 const& prevHash,
        std::uint64_t const& toView,
        PublicKey const nodePublic,
        NetClock::time_point signTime,
        std::uint32_t const validatedSeq);

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

    std::uint32_t const& prevSeq() const { return prevSeq_; }
    uint256 const& prevHash() const { return prevHash_; }
    std::uint64_t const& toView() const { return toView_; }
    PublicKey const& nodePublic() const { return nodePublic_; }
    std::uint32_t const& validatedSeq() const { return validatedSeq_; }

    Blob getSerialized() const;

    Json::Value
    getJson(bool withView = true) const;

private:
    std::uint32_t   prevSeq_;
    uint256         prevHash_;
    std::uint64_t   toView_;

    PublicKey       nodePublic_;
    std::uint32_t   validatedSeq_;


private:
    static SOTemplate const& getFormat();
};

} // ripple

#endif
