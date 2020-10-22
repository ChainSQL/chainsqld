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

#include <ripple/overlay/impl/OverlayImpl.h>
#include <ripple/overlay/impl/PeerImp.h>
#include <ripple/core/SociDB.h>
#include <ripple/core/DatabaseCon.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/misc/HashRouter.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/app/misc/ValidatorList.h>
#include <ripple/app/misc/ValidatorSite.h>
#include <ripple/basics/base64.h>
#include <ripple/overlay/predicates.h>
#include <ripple/rpc/handlers/GetCounts.h>
#include <peersafe/schema/Schema.h>
#include <peersafe/schema/PeerManagerImp.h>

namespace ripple {
	PeerManagerImpl::PeerManagerImpl(Schema& app):
		app_(app),
		journal_(app_.journal("PeerManager"))
	{
	}

	// Returns information on verified peers.
	Json::Value
		PeerManagerImpl::json()
	{
		return foreach(get_peer_json(app_.schemaId()));
	}

	void
		PeerManagerImpl::lastLink(std::uint32_t id)
	{
		// Notify threads when every peer has received a last link.
		// This doesn't account for every node that might reply but
		// it is adequate.
		std::lock_guard<std::mutex> l{ csMutex_ };
		if (csIDs_.erase(id) && csIDs_.empty())
			csCV_.notify_all();
	}

	void
		PeerManagerImpl::onManifests(
			std::shared_ptr<protocol::TMManifests> const& m,
			std::shared_ptr<PeerImp> const& from)
	{
		auto& hashRouter = app_.getHashRouter();
		auto const n = m->list_size();
		auto const& journal = from->pjournal();

		JLOG(journal.debug()) << "TMManifest, " << n << (n == 1 ? " item" : " items");

		for (std::size_t i = 0; i < n; ++i)
		{
			auto& s = m->list().Get(i).stobject();

			if (auto mo = deserializeManifest(s))
			{
				uint256 const hash = mo->hash();
				if (!hashRouter.addSuppressionPeer(hash, from->id())) {
					JLOG(journal.info()) << "Duplicate manifest #" << i + 1;
					continue;
				}

				if (!app_.validators().listed(mo->masterKey))
				{
					JLOG(journal.info()) << "Untrusted manifest #" << i + 1;
					app_.getOPs().pubManifest(*mo);
					continue;
				}

				auto const serialized = mo->serialized;

				auto const result = app_.validatorManifests().applyManifest(
					std::move(*mo));

				if (result == ManifestDisposition::accepted)
				{
					app_.getOPs().pubManifest(*deserializeManifest(serialized));
				}

				if (result == ManifestDisposition::accepted)
				{
					auto db = app_.getWalletDB().checkoutDb();

					soci::transaction tr(*db);
					static const char* const sql =
						"INSERT INTO ValidatorManifests (RawData) VALUES (:rawData);";
					soci::blob rawData(*db);
					convert(serialized, rawData);
					*db << sql, soci::use(rawData);
					tr.commit();

					protocol::TMManifests o;
					o.add_list()->set_stobject(s);

					auto const toSkip = hashRouter.shouldRelay(hash);
					if (toSkip)
						foreach(send_if_not(
							std::make_shared<Message>(o, protocol::mtMANIFESTS),
							peer_in_set(*toSkip)));
				}
				else
				{
					JLOG(journal.info()) << "Bad manifest #" << i + 1 <<
						": " << to_string(result);
				}
			}
			else
			{
				JLOG(journal.warn()) << "Malformed manifest #" << i + 1;
				continue;
			}
		}
	}

	void PeerManagerImpl::add(std::shared_ptr<PeerImp> const& peer)
	{
		auto const result = ids_.emplace(
			std::piecewise_construct,
			std::make_tuple(peer->id()),
			std::make_tuple(peer));
		assert(result.second);
	}

	Json::Value
		PeerManagerImpl::crawlShards(bool pubKey, std::uint32_t hops)
	{
		using namespace std::chrono;
		using namespace std::chrono_literals;

		Json::Value jv(Json::objectValue);
		auto const numPeers{ size() };
		if (numPeers == 0)
			return jv;

		// If greater than a hop away, we may need to gather or freshen data
		if (hops > 0)
		{
			// Prevent crawl spamming
			clock_type::time_point const last(csLast_.load());
			if ((clock_type::now() - last) > 60s)
			{
				auto const timeout(seconds((hops * hops) * 10));
				std::unique_lock<std::mutex> l{ csMutex_ };

				// Check if already requested
				if (csIDs_.empty())
				{
					{
						std::lock_guard <decltype(mutex_)> lock{ mutex_ };
						for (auto& id : ids_)
							csIDs_.emplace(id.first);
					}

					// Relay request to active peers
					protocol::TMGetPeerShardInfo tmGPS;
					tmGPS.set_hops(hops);
					tmGPS.set_schemaid(app_.schemaId().begin(), uint256::size());
					foreach(send_always(std::make_shared<Message>(
						tmGPS, protocol::mtGET_PEER_SHARD_INFO)));

					if (csCV_.wait_for(l, timeout) == std::cv_status::timeout)
					{
						csIDs_.clear();
						csCV_.notify_all();
					}
					csLast_ = duration_cast<seconds>(
						clock_type::now().time_since_epoch());
				}
				else
					csCV_.wait_for(l, timeout);
			}
		}

		// Combine the shard info from peers and their sub peers
		hash_map<PublicKey, PeerImp::ShardInfo> peerShardInfo;
		for_each([&](std::shared_ptr<PeerImp> const& peer)
		{
			if (auto psi = peer->getPeerShardInfo(app_.schemaId()))
			{
				for (auto const& e : *psi)
				{
					auto it{ peerShardInfo.find(e.first) };
					if (it != peerShardInfo.end())
						// The key exists so join the shard indexes.
						it->second.shardIndexes += e.second.shardIndexes;
					else
						peerShardInfo.emplace(std::move(e));
				}
			}
		});

		// Prepare json reply
		auto& av = jv[jss::peers] = Json::Value(Json::arrayValue);
		for (auto const& e : peerShardInfo)
		{
			auto& pv{ av.append(Json::Value(Json::objectValue)) };
			if (pubKey)
				pv[jss::public_key] = toBase58(TokenType::NodePublic, e.first);

			auto const& address{ e.second.endpoint.address() };
			if (!address.is_unspecified())
				pv[jss::ip] = address.to_string();

			pv[jss::complete_shards] = to_string(e.second.shardIndexes);
		}

		return jv;
	}


	std::size_t
	PeerManagerImpl::selectPeers(PeerSet& set, std::size_t limit,
			std::function<bool(std::shared_ptr<Peer> const&)> score)
	{
		using item = std::pair<int, std::shared_ptr<PeerImp>>;

		std::vector<item> v;
		v.reserve(size());

		for_each([&](std::shared_ptr<PeerImp>&& e)
		{
			auto const s = e->getScore(score(e));
			v.emplace_back(s, std::move(e));
		});

		std::sort(v.begin(), v.end(),
			[](item const& lhs, item const&rhs)
		{
			return lhs.first > rhs.first;
		});

		std::size_t accepted = 0;
		for (auto const& e : v)
		{
			if (set.insert(e.second) && ++accepted >= limit)
				break;
		}
		return accepted;
	}

	/** The number of active peers on the network
		Active peers are only those peers that have completed the handshake
		and are running the Ripple protocol.
	*/
	std::size_t
		PeerManagerImpl::size()
	{
		std::lock_guard <decltype(mutex_)> lock(mutex_);
		return ids_.size();
	}

	Json::Value
		PeerManagerImpl::getOverlayInfo()
	{
		using namespace std::chrono;
		Json::Value jv;
		auto& av = jv["active"] = Json::Value(Json::arrayValue);

		for_each([&](std::shared_ptr<PeerImp>&& sp)
		{
			auto& pv = av.append(Json::Value(Json::objectValue));
			pv[jss::public_key] = base64_encode(
				sp->getNodePublic().data(),
				sp->getNodePublic().size());
			pv[jss::type] = sp->slot()->inbound() ?
				"in" : "out";
			pv[jss::uptime] =
				static_cast<std::uint32_t>(duration_cast<seconds>(
					sp->uptime()).count());
			if (sp->crawl())
			{
				pv[jss::ip] = sp->getRemoteAddress().address().to_string();
				if (sp->slot()->inbound())
				{
					if (auto port = sp->slot()->listening_port())
						pv[jss::port] = *port;
				}
				else
				{
					pv[jss::port] = std::to_string(
						sp->getRemoteAddress().port());
				}
			}

			{
				auto version{ sp->getVersion() };
				if (!version.empty())
					pv[jss::version] = std::move(version);
			}

			std::uint32_t minSeq, maxSeq;
			sp->ledgerRange(app_.schemaId(),minSeq, maxSeq);
			if (minSeq != 0 || maxSeq != 0)
				pv[jss::complete_ledgers] =
				std::to_string(minSeq) + "-" +
				std::to_string(maxSeq);

			if (auto shardIndexes = sp->getShardIndexes(app_.schemaId()))
				pv[jss::complete_shards] = to_string(*shardIndexes);
		});

		return jv;
	}

	Json::Value
		PeerManagerImpl::getServerInfo()
	{
		bool const humanReadable = false;
		bool const admin = false;
		bool const counters = false;

		Json::Value server_info = app_.getOPs().getServerInfo(humanReadable, admin, counters);

		// Filter out some information
		server_info.removeMember(jss::hostid);
		server_info.removeMember(jss::load_factor_fee_escalation);
		server_info.removeMember(jss::load_factor_fee_queue);
		server_info.removeMember(jss::validation_quorum);

		if (server_info.isMember(jss::validated_ledger))
		{
			Json::Value& validated_ledger = server_info[jss::validated_ledger];

			validated_ledger.removeMember(jss::base_fee);
			validated_ledger.removeMember(jss::reserve_base_zxc);
			validated_ledger.removeMember(jss::reserve_inc_zxc);
		}

		return server_info;
	}

	Json::Value
		PeerManagerImpl::getServerCounts()
	{
		return getCountsJson(app_, 10);
	}

	Json::Value
		PeerManagerImpl::getUnlInfo()
	{
		Json::Value validators = app_.validators().getJson();

		if (validators.isMember(jss::publisher_lists))
		{
			Json::Value& publisher_lists = validators[jss::publisher_lists];

			for (auto& publisher : publisher_lists)
			{
				publisher.removeMember(jss::list);
			}
		}

		validators.removeMember(jss::signing_keys);
		validators.removeMember(jss::trusted_validator_keys);
		validators.removeMember(jss::validation_quorum);

		Json::Value validatorSites = app_.validatorSites().getJson();

		if (validatorSites.isMember(jss::validator_sites))
		{
			validators[jss::validator_sites] = std::move(validatorSites[jss::validator_sites]);
		}

		return validators;
	}


	PeerManager::PeerSequence
		PeerManagerImpl::getActivePeers()
	{
		PeerManager::PeerSequence ret;
		ret.reserve(size());

		for_each([&ret](std::shared_ptr<PeerImp>&& sp)
		{
			ret.emplace_back(std::move(sp));
		});

		return ret;
	}

	void
		PeerManagerImpl::checkSanity(std::uint32_t index)
	{
		for_each([index,this](std::shared_ptr<PeerImp>&& sp)
		{
			sp->checkSanity(app_.schemaId(),index);
		});
	}


	std::shared_ptr<Peer>
		PeerManagerImpl::findPeerByShortID(Peer::id_t const& id)
	{
		std::lock_guard <decltype(mutex_)> lock(mutex_);
		auto const iter = ids_.find(id);
		if (iter != ids_.end())
			return iter->second.lock();
		return {};
	}

	// A public key hash map was not used due to the peer connect/disconnect
	// update overhead outweighing the performance of a small set linear search.
	std::shared_ptr<Peer>
		PeerManagerImpl::findPeerByPublicKey(PublicKey const& pubKey)
	{
		std::lock_guard <decltype(mutex_)> lock(mutex_);
		for (auto const& e : ids_)
		{
			if (auto peer = e.second.lock())
			{
				if (peer->getNodePublic() == pubKey)
					return peer;
			}
		}
		return {};
	}

	void
		PeerManagerImpl::send(protocol::TMProposeSet& m)
	{
		//if (setup_.expire)
		//	m.set_hops(0);
		auto const sm = std::make_shared<Message>(m, protocol::mtPROPOSE_LEDGER);
		for_each([&](std::shared_ptr<PeerImp>&& p)
		{
			p->send(sm);
		});
	}
	void
		PeerManagerImpl::send(protocol::TMValidation& m)
	{
		//if (setup_.expire)
		//	m.set_hops(0);
		auto const sm = std::make_shared<Message>(m, protocol::mtVALIDATION);
		for_each([&](std::shared_ptr<PeerImp>&& p)
		{
			p->send(sm);
		});

		SerialIter sit(m.validation().data(), m.validation().size());
		auto val = std::make_shared<STValidation>(
			std::ref(sit),
			[this](PublicKey const& pk) {
			return calcNodeID(app_.validatorManifests().getMasterKey(pk));
		},
			false);
		app_.getOPs().pubValidation(val);
	}

	void
		PeerManagerImpl::send(protocol::TMViewChange& m)
	{
		auto const sm = std::make_shared<Message>(
			m, protocol::mtVIEW_CHANGE);
		for_each([&](std::shared_ptr<PeerImp>&& p)
		{
			p->send(sm);
		});
	}

	void
		PeerManagerImpl::relay(protocol::TMProposeSet& m, uint256 const& uid)
	{
		if (m.has_hops() && m.hops() >= maxTTL)
			return;
		if (auto const toSkip = app_.getHashRouter().shouldRelay(uid))
		{
			auto const sm = std::make_shared<Message>(m, protocol::mtPROPOSE_LEDGER);
			for_each([&](std::shared_ptr<PeerImp>&& p)
			{
				if (toSkip->find(p->id()) == toSkip->end())
					p->send(sm);
			});
		}
	}

	void
		PeerManagerImpl::relay(protocol::TMValidation& m, uint256 const& uid)
	{
		if (m.has_hops() && m.hops() >= maxTTL)
			return;
		if (auto const toSkip = app_.getHashRouter().shouldRelay(uid))
		{
			auto const sm = std::make_shared<Message>(m, protocol::mtVALIDATION);
			for_each([&](std::shared_ptr<PeerImp>&& p)
			{
				if (toSkip->find(p->id()) == toSkip->end())
					p->send(sm);
			});
		}
	}

	void
		PeerManagerImpl::relay(protocol::TMViewChange& m,
			uint256 const& uid)
	{
		//if (m.has_hops() && m.hops() >= maxTTL)
		//	return;
		auto const toSkip = app_.getHashRouter().shouldRelay(uid);
		if (!toSkip)
			return;
		auto const sm = std::make_shared<Message>(
			m, protocol::mtVIEW_CHANGE);
		for_each([&](std::shared_ptr<PeerImp>&& p)
		{
			if (toSkip->find(p->id()) != toSkip->end())
				return;
			//if (!m.has_hops() || p->hopsAware())
			p->send(sm);
		});
	}

	std::unique_ptr <PeerManager> make_PeerManager(Schema& schema)
	{
		return std::make_unique<PeerManagerImpl>(schema);
	}
}