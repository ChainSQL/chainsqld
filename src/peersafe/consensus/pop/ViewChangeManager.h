//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012-2017 Ripple Labs Inc.

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


#ifndef PEERSAFE_CONSENSUS_VIEWCHANGE_MANAGER_H_INCLUDE
#define PEERSAFE_CONSENSUS_VIEWCHANGE_MANAGER_H_INCLUDE


#include <map>
#include <cstdint>
#include <ripple/basics/base_uint.h>
#include <ripple/protocol/PublicKey.h>
#include <ripple/beast/utility/Journal.h>
#include <ripple/app/consensus/RCLCxLedger.h>
#include <peersafe/protocol/STViewChange.h>


namespace ripple {

class ViewChangeManager
{
    using VIEWTYPE = std::uint64_t;
    using ViewChange = STViewChange::pointer;

private:
    std::map<VIEWTYPE, std::map<PublicKey, ViewChange>> viewChangeReq_;
    beast::Journal j_;

public:
    ViewChangeManager(beast::Journal const& j) : j_(j)
    {
    }

    std::size_t
    viewCount(VIEWTYPE toView);

    // Erase invalid ViewChange object from the cache on new round started.
    void
    onNewRound(RCLCxLedger const& ledger);

    /** Receive a view change message.
        Return:
                - true if the first time receiving this msg.
                - false if msg duplicate.
    */
    bool
    recvViewChange(ViewChange const& change);

    /** Check if we can trigger view-change.
        Return:
                - true if condition for view-change met.
                - false if condition not met.
    */
    bool
    haveConsensus(
        VIEWTYPE const& toView,
        VIEWTYPE const& curView,
        RCLCxLedger::ID const& preHash,
        std::size_t quorum);

    // Erase some old view-change cache when view_change happen.
    void
    onViewChanged(VIEWTYPE const& newView, std::uint32_t preSeq);

    std::tuple<bool, uint32_t, uint256>
    shouldTriggerViewChange(
        VIEWTYPE const& toView,
        RCLCxLedger const& prevLedger,
        std::size_t quorum);

    void
    clearCache();

    Json::Value
    getJson() const;
};

}  // namespace ripple
#endif