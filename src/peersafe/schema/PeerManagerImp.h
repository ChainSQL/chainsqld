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

#pragma once
#include <ripple/core/Job.h>
#include <peersafe/schema/Schema.h>
#include <peersafe/schema/PeerManager.h>
#include <set>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace ripple {

class PeerImp;
class BasicConfig;

class PeerManagerImpl : public PeerManager
{
private:
	using clock_type = std::chrono::steady_clock;

	Schema& app_;
	std::recursive_mutex mutex_;
	beast::Journal journal_;
	hash_map<Peer::id_t, std::weak_ptr<PeerImp>> ids_;

	// Last time we crawled peers for shard info. 'cs' = crawl shards
	std::atomic<std::chrono::seconds> csLast_{ std::chrono::seconds{0} };
	std::mutex csMutex_;
	std::condition_variable csCV_;
	// Peer IDs expecting to receive a last link notification
	std::set<std::uint32_t> csIDs_;

	friend class PeerImp;
	//--------------------------------------------------------------------------
public:
	PeerManagerImpl(Schema& app);

	PeerManagerImpl(PeerManagerImpl const&) = delete;
	PeerManagerImpl& operator= (PeerManagerImpl const&) = delete;

	Json::Value
		json() override;

	PeerSequence
		getActivePeers() override;

	void
		lastLink(std::uint32_t id) override;

	std::size_t
		size() override;
	void
		checkSanity(std::uint32_t) override;

	std::shared_ptr<Peer>
		findPeerByShortID(Peer::id_t const& id) override;

	std::shared_ptr<Peer>
		findPeerByPublicKey(PublicKey const& pubKey) override;

	void
		send(protocol::TMProposeSet& m) override;

	void
		send(protocol::TMValidation& m) override;

	void
		send(protocol::TMViewChange& m) override;

	void
		relay(protocol::TMProposeSet& m,
			uint256 const& uid) override;

	void
		relay(protocol::TMValidation& m,
			uint256 const& uid) override;


	void
		relay(protocol::TMViewChange& m,
			uint256 const& uid) override;

	//--------------------------------------------------------------------------
	//
	// PeerManagerImpl
	//
	//
	template <class UnaryFunc>
	void
		for_each(UnaryFunc&& f)
	{
		std::vector<std::weak_ptr<PeerImp>> wp;
		{
			std::lock_guard<decltype(mutex_)> lock(mutex_);

			// Iterate over a copy of the peer list because peer
			// destruction can invalidate iterators.
			wp.reserve(ids_.size());

			for (auto& x : ids_)
				wp.push_back(x.second);
		}

		for (auto& w : wp)
		{
			if (auto p = w.lock())
				f(std::move(p));
		}
	}

	std::size_t
		selectPeers(PeerSet& set, std::size_t limit, std::function<
			bool(std::shared_ptr<Peer> const&)> score) override;

	// Called when TMManifests is received from a peer
	void
		onManifests(
			std::shared_ptr<protocol::TMManifests> const& m,
			std::shared_ptr<PeerImp> const& from) override;

	void add(std::shared_ptr<PeerImp> const& peer) override;

	void remove(std::vector<PublicKey> const& vecPubs) override;

	Json::Value
		crawlShards(bool pubKey, std::uint32_t hops) override;

	/** Returns information about peers on the overlay network.
		Reported through the /crawl API
		Controlled through the config section [crawl] overlay=[0|1]
	*/
	Json::Value
		getOverlayInfo();

	/** Returns information about the local server.
		Reported through the /crawl API
		Controlled through the config section [crawl] server=[0|1]
	*/
	Json::Value
		getServerInfo();

	/** Returns information about the local server's performance counters.
		Reported through the /crawl API
		Controlled through the config section [crawl] counts=[0|1]
	*/
	Json::Value
		getServerCounts();

	/** Returns information about the local server's UNL.
		Reported through the /crawl API
		Controlled through the config section [crawl] unl=[0|1]
	*/
	Json::Value
		getUnlInfo();
};
}