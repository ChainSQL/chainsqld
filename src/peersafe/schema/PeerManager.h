#pragma once

#include <ripple/json/json_value.h>
#include <ripple/overlay/Peer.h>
#include <ripple/overlay/PeerSet.h>

namespace ripple {

class PeerImp;

class PeerManager
{
public:
    using PeerSequence = std::vector<std::shared_ptr<Peer>>;

public:
    PeerManager()
    {
    }

    virtual ~PeerManager()
    {
    }

public:
    /** Return diagnostics on the status of all peers.
            @deprecated This is superceded by PropertyStream
    */
    virtual Json::Value
    json() = 0;

    virtual std::size_t
    size() const = 0;

    /** Returns the peer with the matching short id, or null. */
    virtual std::shared_ptr<Peer>
    findPeerByShortID(Peer::id_t const& id) = 0;

    /** Returns the peer with the matching public key, or null. */
    virtual std::shared_ptr<Peer>
    findPeerByPublicKey(PublicKey const& pubKey) = 0;

    /** Returns the peer with the matching validate public key, or null. */
    virtual std::shared_ptr<Peer>
    findPeerByValPublicKey(PublicKey const& pubKey) = 0;

    /** Returns a sequence representing the current list of peers.
            The snapshot is made at the time of the call.
    */
    virtual PeerSequence
    getActivePeers() const = 0;

    virtual void
    lastLink(std::uint32_t id) = 0;

    /** Calls the checkSanity function on each peer
            @param index the value to pass to the peer's checkSanity function
    */
    virtual void
    checkSanity(std::uint32_t index) = 0;

    ///** Calls the check function on each peer
    //*/
    // virtual
    //	void
    //	check() = 0;

    /** Broadcast a consensus meessage. */
    virtual void
    send(protocol::TMConsensus& m) = 0;

    /* Send a consensus message to specified peer */
    virtual void
    send(PublicKey const& pubKey, protocol::TMConsensus& m) = 0;

    /** Relay a consensus meessage. */
    virtual void
    relay(protocol::TMConsensus& m, uint256 const& uid) = 0;

    ///** Visit every active peer and return a value
    //	The functor must:
    //	- Be callable as:
    //		void operator()(std::shared_ptr<Peer> const& peer);
    //	 - Must have the following type alias:
    //		using return_type = void;
    //	 - Be callable as:
    //		Function::return_type operator()() const;

    //	@param f the functor to call with every peer
    //	@returns `f()`

    //	@note The functor is passed by value!
    //*/
    // template <typename UnaryFunc>
    // std::enable_if_t<!std::is_void<
    //	typename UnaryFunc::return_type>::value,
    //	typename UnaryFunc::return_type>
    //	foreach(UnaryFunc f)
    //{
    //	for (auto const& p : getActivePeers())
    //		f(p);
    //	return f();
    //}

    ///** Visit every active peer
    //	The visitor functor must:
    //	 - Be callable as:
    //		void operator()(std::shared_ptr<Peer> const& peer);
    //	 - Must have the following type alias:
    //		using return_type = void;

    //	@param f the functor to call with every peer
    //*/
    // template <class Function>
    // std::enable_if_t <
    //	std::is_void <typename Function::return_type>::value,
    //	typename Function::return_type
    //>
    //	foreach(Function f)
    //{
    //	for (auto const& p : getActivePeers())
    //		f(p);
    //}
    /** Visit every active peer.
     *
     * The visitor must be invocable as:
     *     Function(std::shared_ptr<Peer> const& peer);
     *
     * @param f the invocable to call with every peer
     */
    template <class Function>
    void
    foreach(Function f) const
    {
        for (auto const& p : getActivePeers())
            f(p);
    }
    /** Returns information reported to the crawl shard RPC command.

            @param hops the maximum jumps the crawler will attempt.
            The number of hops achieved is not guaranteed.
    */
    virtual Json::Value
    crawlShards(bool pubKey, std::uint32_t hops) = 0;

    virtual void
    onManifests(
        std::shared_ptr<protocol::TMManifests> const& m,
        std::shared_ptr<PeerImp> const& from) = 0;

    virtual void
    add(std::shared_ptr<PeerImp> const& peer) = 0;

    virtual void
    remove(std::vector<PublicKey> const& vecPubs) = 0;
};

std::unique_ptr<PeerManager>
make_PeerManager(Schema& schema);

}  // namespace ripple