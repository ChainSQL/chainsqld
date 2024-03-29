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

#ifndef RIPPLE_OVERLAY_OVERLAY_H_INCLUDED
#define RIPPLE_OVERLAY_OVERLAY_H_INCLUDED

#include <ripple/beast/utility/PropertyStream.h>
#include <ripple/core/Stoppable.h>
#include <ripple/json/json_value.h>
#include <ripple/overlay/Peer.h>
#include <ripple/overlay/PeerSet.h>
#include <ripple/server/Handoff.h>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/optional.hpp>
#include <functional>
#include <memory>
#include <type_traits>

namespace boost {
namespace asio {
namespace ssl {
class context;
}
}  // namespace asio
}  // namespace boost


namespace ripple {

/** Manages the set of connected peers. */
class Overlay : public Stoppable, public beast::PropertyStream::Source
{
protected:
    using socket_type = boost::beast::tcp_stream;
    using stream_type = boost::beast::ssl_stream<socket_type>;

    // VFALCO NOTE The requirement of this constructor is an
    //             unfortunate problem with the API for
    //             Stoppable and PropertyStream
    //
    Overlay(Stoppable& parent)
        : Stoppable("Overlay", parent), beast::PropertyStream::Source("peers")
    {
    }

public:
    enum class Promote { automatic, never, always };

    struct Setup
    {
        explicit Setup() = default;

        std::shared_ptr<boost::asio::ssl::context> context;
        beast::IP::Address public_ip;
        int ipLimit = 0;
        std::uint32_t crawlOptions = 0;
        boost::optional<std::uint32_t> networkID;
        bool vlEnabled = true;
    };

    using PeerSequence = std::vector<std::shared_ptr<Peer>>;

    virtual ~Overlay() = default;

    /** Conditionally accept an incoming HTTP request. */
    virtual Handoff
    onHandoff(
        std::unique_ptr<stream_type>&& bundle,
        http_request_type&& request,
        boost::asio::ip::tcp::endpoint remote_address) = 0;

    /** Establish a peer connection to the specified endpoint.
        The call returns immediately, the connection attempt is
        performed asynchronously.
    */
    virtual void
    connect(beast::IP::Endpoint const& address) = 0;

    /** Returns the maximum number of peers we are configured to allow. */
    virtual int
    limit() = 0;

    /** Returns the number of active peers.
        Active peers are only those peers that have completed the
        handshake and are using the peer protocol.
    */
    virtual std::size_t
    size() const = 0;

    /** Return diagnostics on the status of all peers.
        @deprecated This is superceded by PropertyStream
    */
    virtual Json::Value
    json() = 0;

    /** Returns a sequence representing the current list of peers.
        The snapshot is made at the time of the call.
    */
    virtual PeerSequence
    getActivePeers() const = 0;

    ///** Calls the checkSanity function on each peer
    //    @param index the value to pass to the peer's checkSanity function
    //*/
    // virtual
    // void
    // checkSanity (std::uint32_t index) = 0;

    ///** Calls the check function on each peer
    //*/
    virtual void
    check() = 0;

    virtual void
    onSchemaCreated(uint256 const& schemaId) = 0;

    virtual void
    onSchemaAddPeer(
        uint256 const& schemaId,
        std::vector<std::string> const& bootstraps,
        std::vector<PublicKey> const& validators) = 0;

    template <typename UnaryFunc>
    std::enable_if_t<
        !std::is_void<typename UnaryFunc::return_type>::value,
        typename UnaryFunc::return_type>
    foreach(UnaryFunc f)
    {
        for (auto const& p : getActivePeers())
            f(p);
        return f();
    }

    /** Increment and retrieve counter for transaction job queue overflows. */
    virtual void
    incJqTransOverflow() = 0;
    virtual std::uint64_t
    getJqTransOverflow() const = 0;

    /** Increment and retrieve counters for total peer disconnects, and
     * disconnects we initiate for excessive resource consumption.
     */
    virtual void
    incPeerDisconnect() = 0;
    virtual std::uint64_t
    getPeerDisconnect() const = 0;
    virtual void
    incPeerDisconnectCharges() = 0;
    virtual std::uint64_t
    getPeerDisconnectCharges() const = 0;

    /** Returns the ID of the network this server is configured for, if any.

        The ID is just a numerical identifier, with the IDs 0, 1 and 2 used to
        identify the mainnet, the testnet and the devnet respectively.

        @return The numerical identifier configured by the administrator of the
                server. An unseated optional, otherwise.
    */
    virtual boost::optional<std::uint32_t>
    networkID() const = 0;
};

struct ScoreHasLedger
{
    uint256 const& schemaId_;
    uint256 const& hash_;
    std::uint32_t seq_;
    bool
    operator()(std::shared_ptr<Peer> const&) const;

    ScoreHasLedger(
        uint256 const& schemaId,
        uint256 const& hash,
        std::uint32_t seq)
        : schemaId_(schemaId), hash_(hash), seq_(seq)
    {
    }
};

struct ScoreHasTxSet
{
    SchemaID const& schemaId_;
    uint256 const& hash_;
    bool
    operator()(std::shared_ptr<Peer> const&) const;

    ScoreHasTxSet(uint256 const& schemaId, uint256 const& hash)
        : schemaId_(schemaId), hash_(hash)
    {
    }
};

}  // namespace ripple

#endif
