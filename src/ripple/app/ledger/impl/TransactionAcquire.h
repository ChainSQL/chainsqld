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

#ifndef RIPPLE_APP_LEDGER_TRANSACTIONACQUIRE_H_INCLUDED
#define RIPPLE_APP_LEDGER_TRANSACTIONACQUIRE_H_INCLUDED

#include <peersafe/schema/Schema.h>
#include <ripple/overlay/PeerSet.h>
#include <ripple/shamap/SHAMap.h>

namespace ripple {

// VFALCO TODO rename to PeerTxRequest
// A transaction set we are trying to acquire
class TransactionAcquire final
    : public PeerSet,
      public std::enable_shared_from_this<TransactionAcquire>,
      public CountedObject<TransactionAcquire>
{
public:
    static char const*
    getCountedObjectName()
    {
        return "TransactionAcquire";
    }

    using pointer = std::shared_ptr<TransactionAcquire>;

public:
    TransactionAcquire(Schema& app, uint256 const& hash);
    ~TransactionAcquire() = default;

    SHAMapAddNode
    takeNodes(
        const std::list<SHAMapNodeID>& IDs,
        const std::list<Blob>& data,
        std::shared_ptr<Peer> const&);

    void
    init(int startPeers);

    void
    stillNeed();

private:
    std::shared_ptr<SHAMap> mMap;
    bool mHaveRoot;

    void
    queueJob() override;

    void
    onTimer(bool progress, ScopedLockType& peerSetLock) override;

    void
    onPeerAdded(std::shared_ptr<Peer> const& peer) override
    {
        trigger(peer);
    }

    void
    done();

    void
    addPeers(std::size_t limit);

    void
    trigger(std::shared_ptr<Peer> const&);
    std::weak_ptr<PeerSet>
    pmDowncast() override;
};

}  // namespace ripple

#endif
