//------------------------------------------------------------------------------
/*
This file is part of chainsqld: https://github.com/chainsql/chainsqld
Copyright (c) 2016-2019 Peersafe Technology Co., Ltd.

chainsqld is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

chainsqld is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
//==============================================================================

#include <peersafe/app/misc/ConfigSite.h>
#include <ripple/app/misc/detail/WorkFile.h>
#include <ripple/app/misc/detail/WorkPlain.h>
#include <ripple/app/misc/detail/WorkSSL.h>
#include <ripple/basics/Slice.h>
#include <ripple/json/json_reader.h>
#include <ripple/protocol/jss.h>
#include <beast/core/detail/base64.hpp>
#include <boost/algorithm/clamp.hpp>
#include <boost/regex.hpp>
#include <algorithm>

namespace ripple {

	auto           constexpr default_refresh_interval = std::chrono::minutes{ 5 };
	auto           constexpr error_retry_interval = std::chrono::seconds{ 30 };
	unsigned short constexpr max_redirects = 3;


	ConfigSite::Site::Resource::Resource(std::string uri_)
		: uri{ std::move(uri_) }
	{
		if (!parseUrl(pUrl, uri))
			throw std::runtime_error("URI '" + uri + "' cannot be parsed");

		if (pUrl.scheme == "file")
		{
			if (!pUrl.domain.empty())
				throw std::runtime_error("file URI cannot contain a hostname");

#if _MSC_VER    // MSVC: Windows paths need the leading / removed
			{
				if (pUrl.path[0] == '/')
					pUrl.path = pUrl.path.substr(1);

			}
#endif

			if (pUrl.path.empty())
				throw std::runtime_error("file URI must contain a path");
		}
		else if (pUrl.scheme == "http")
		{
			if (pUrl.domain.empty())
				throw std::runtime_error("http URI must contain a hostname");

			if (!pUrl.port)
				pUrl.port = 80;
		}
		else if (pUrl.scheme == "https")
		{
			if (pUrl.domain.empty())
				throw std::runtime_error("https URI must contain a hostname");

			if (!pUrl.port)
				pUrl.port = 443;
		}
		else
			throw std::runtime_error("Unsupported scheme: '" + pUrl.scheme + "'");
	}

	ConfigSite::Site::Site(std::string uri)
		: loadedResource{ std::make_shared<Resource>(std::move(uri)) }
		, startingResource{ loadedResource }
		, redirCount{ 0 }
		, refreshInterval{ default_refresh_interval }
		, nextRefresh{ clock_type::now() }
	{
	}


	ConfigSite::ConfigSite(
		boost::asio::io_service& ios,
		ManifestCache& validatorManifests,
		beast::Journal j,
		std::chrono::seconds timeout )
		: ios_(ios)
		, validatorManifests_(validatorManifests)
		, j_(j)
		, timer_(ios_)
		, fetching_(false)
		, pending_(false)
		, stopping_(false)
		, requestTimeout_(timeout)
	{
	}

	ConfigSite::~ConfigSite()
	{
		std::unique_lock<std::mutex> lock{ state_mutex_ };
		if (timer_.expires_at().time_since_epoch().count())
		{
			if (!stopping_)
			{
				lock.unlock();
				stop();
			}
			else
			{
				cv_.wait(lock, [&] { return !fetching_; });
			}
		}
	}

	bool
		ConfigSite::load(
			std::vector<std::string> const& publisherKeys,
			std::vector<std::string> const& siteURIs)
	{
		JLOG(j_.debug()) <<
			"Loading configured validator list sites";

		std::lock_guard <std::mutex> lock{ publisher_mutex_ };

		JLOG(j_.debug()) <<
			"Loading configured trusted validator list publisher keys";

		std::size_t count = 0;
		for (auto key : publisherKeys)
		{
			JLOG(j_.trace()) <<
				"Processing '" << key << "'";

			auto const ret = strUnHex(key);

			if (!ret.second || !ret.first.size())
			{
				JLOG(j_.error()) <<
					"Invalid validator list publisher key: " << key;
				return false;
			}

			auto id = PublicKey(Slice{ ret.first.data(), ret.first.size() });

			if (validatorManifests_.revoked(id))
			{
				JLOG(j_.warn()) <<
					"Configured validator list publisher key is revoked: " << key;
				continue;
			}

			if (publisherLists_.count(id))
			{
				JLOG(j_.warn()) <<
					"Duplicate validator list publisher key: " << key;
				continue;
			}

			publisherLists_[id].available = false;
			++count;
		}

		JLOG(j_.debug()) <<
			"Loaded " << count << " keys";

		return load(siteURIs);
	}

	bool ConfigSite::load(std::vector<std::string> const& siteURIs)
	{
		JLOG(j_.debug()) <<
			"Loading configured validator list sites";

		std::lock_guard <std::mutex> lock{ sites_mutex_ };

		for (auto const& uri : siteURIs)
		{
			try
			{
				sites_.emplace_back(uri);
			}
			catch (std::exception const& e)
			{
				JLOG(j_.error()) <<
					"Invalid validator site uri: " << uri <<
					": " << e.what();
				return false;
			}
		}

		JLOG(j_.debug()) <<
			"Loaded " << siteURIs.size() << " sites";

		return true;
	}

	void
		ConfigSite::start()
	{
		std::lock_guard <std::mutex> lock{ state_mutex_ };
		if (timer_.expires_at() == clock_type::time_point{})
			setTimer(lock);
	}

	void
		ConfigSite::join()
	{
		std::unique_lock<std::mutex> lock{ state_mutex_ };
		cv_.wait(lock, [&] { return !pending_; });
	}

	void
		ConfigSite::stop()
	{
		std::unique_lock<std::mutex> lock{ state_mutex_ };
		stopping_ = true;
		// work::cancel() must be called before the
		// cv wait in order to kick any asio async operations
		// that might be pending.
		if (auto sp = work_.lock())
			sp->cancel();
		cv_.wait(lock, [&] { return !fetching_; });

		// docs indicate cancel() can throw, but this should be
		// reconsidered if it changes to noexcept
		try
		{
			timer_.cancel();
		}
		catch (boost::system::system_error const&)
		{
		}
		stopping_ = false;
		pending_ = false;
		cv_.notify_all();
	}

	void
		ConfigSite::setTimer(std::lock_guard<std::mutex>& state_lock)
	{
		std::lock_guard <std::mutex> lock{ sites_mutex_ };

		auto next = std::min_element(sites_.begin(), sites_.end(),
			[](Site const& a, Site const& b)
		{
			return a.nextRefresh < b.nextRefresh;
		});

		if (next != sites_.end())
		{
			pending_ = next->nextRefresh <= clock_type::now();
			cv_.notify_all();
			timer_.expires_at(next->nextRefresh);
			auto idx = std::distance(sites_.begin(), next);
			timer_.async_wait([this, idx](boost::system::error_code const& ec)
			{
				this->onTimer(idx, ec);
			});
		}
	}

	void ConfigSite::onRequestTimeout(std::size_t siteIdx, error_code const& ec)
	{
		if (ec)
			return;

		{
			std::lock_guard <std::mutex> lock_site{ sites_mutex_ };
			JLOG(j_.warn()) <<
				"Request for " << sites_[siteIdx].activeResource->uri <<
				" took too long";
		}

		std::lock_guard<std::mutex> lock_state{ state_mutex_ };
		if (auto sp = work_.lock())
			sp->cancel();
	}

	void
		ConfigSite::onTimer(
			std::size_t siteIdx,
			error_code const& ec)
	{
		if (ec)
		{
			// Restart the timer if any errors are encountered, unless the error
			// is from the wait operating being aborted due to a shutdown request.
			if (ec != boost::asio::error::operation_aborted)
				onSiteFetch(ec, detail::response_type{}, siteIdx);
			return;
		}

		try
		{
			std::lock_guard <std::mutex> lock{ sites_mutex_ };
			sites_[siteIdx].nextRefresh =
				clock_type::now() + sites_[siteIdx].refreshInterval;
			sites_[siteIdx].redirCount = 0;
			// the WorkSSL client can throw if SSL init fails
			makeRequest(sites_[siteIdx].startingResource, siteIdx, lock);
		}
		catch (std::exception &)
		{
			onSiteFetch(
				boost::system::error_code{ -1, boost::system::generic_category() },
				detail::response_type{},
				siteIdx);
		}
	}




	void
		ConfigSite::parseJsonResponse(
			std::string const& res,
			std::size_t siteIdx,
			std::lock_guard<std::mutex>& sites_lock)
	{
		Json::Reader r;
		Json::Value body;
		if (!r.parse(res.data(), body))
		{
			JLOG(j_.warn()) <<
				"Unable to parse JSON response from  " <<
				sites_[siteIdx].activeResource->uri;
			throw std::runtime_error{ "bad json" };
		}

		if (!body.isObject() ||
			!body.isMember("blob") || !body["blob"].isString() ||
			!body.isMember("manifest") || !body["manifest"].isString() ||
			!body.isMember("signature") || !body["signature"].isString() ||
			!body.isMember("version") || !body["version"].isInt())
		{
			JLOG(j_.warn()) <<
				"Missing fields in JSON response from  " <<
				sites_[siteIdx].activeResource->uri;
			throw std::runtime_error{ "missing fields" };
		}

		auto const disp = applyList(
			body["manifest"].asString(),
			body["blob"].asString(),
			body["signature"].asString(),
			body["version"].asUInt(),
			sites_[siteIdx].activeResource->uri);

		//auto const disp = validators_.applyList(
		//	body["manifest"].asString(),
		//	body["blob"].asString(),
		//	body["signature"].asString(),
		//	body["version"].asUInt(),
		//	sites_[siteIdx].activeResource->uri);

		sites_[siteIdx].lastRefreshStatus.emplace(
			Site::Status{ clock_type::now(), disp, "" });

		if (ListDisposition::accepted == disp)
		{
			JLOG(j_.debug()) <<
				"Applied new validator list from " <<
				sites_[siteIdx].activeResource->uri;
		}
		else if (ListDisposition::same_sequence == disp)
		{
			JLOG(j_.debug()) <<
				"Validator list with current sequence from " <<
				sites_[siteIdx].activeResource->uri;
		}
		else if (ListDisposition::stale == disp)
		{
			JLOG(j_.warn()) <<
				"Stale validator list from " <<
				sites_[siteIdx].activeResource->uri;
		}
		else if (ListDisposition::untrusted == disp)
		{
			JLOG(j_.warn()) <<
				"Untrusted validator list from " <<
				sites_[siteIdx].activeResource->uri;
		}
		else if (ListDisposition::invalid == disp)
		{
			JLOG(j_.warn()) <<
				"Invalid validator list from " <<
				sites_[siteIdx].activeResource->uri;
		}
		else if (ListDisposition::unsupported_version == disp)
		{
			JLOG(j_.warn()) <<
				"Unsupported version validator list from " <<
				sites_[siteIdx].activeResource->uri;
		}
		else
		{
			BOOST_ASSERT(false);
		}

		if (body.isMember("refresh_interval") &&
			body["refresh_interval"].isNumeric())
		{
			using namespace std::chrono_literals;
			std::chrono::minutes const refresh =
				boost::algorithm::clamp(
					std::chrono::minutes{ body["refresh_interval"].asUInt() },
					1min,
					24h);
			sites_[siteIdx].refreshInterval = refresh;
			sites_[siteIdx].nextRefresh =
				clock_type::now() + sites_[siteIdx].refreshInterval;
		}
	}

	std::shared_ptr<ConfigSite::Site::Resource>
		ConfigSite::processRedirect(
			detail::response_type& res,
			std::size_t siteIdx,
			std::lock_guard<std::mutex>& sites_lock)
	{
		using namespace boost::beast::http;
		std::shared_ptr<Site::Resource> newLocation;
		if (res.find(field::location) == res.end() ||
			res[field::location].empty())
		{
			JLOG(j_.warn()) <<
				"Request for validator list at " <<
				sites_[siteIdx].activeResource->uri <<
				" returned a redirect with no Location.";
			throw std::runtime_error{ "missing location" };
		}

		if (sites_[siteIdx].redirCount == max_redirects)
		{
			JLOG(j_.warn()) <<
				"Exceeded max redirects for validator list at " <<
				sites_[siteIdx].loadedResource->uri;
			throw std::runtime_error{ "max redirects" };
		}

		JLOG(j_.debug()) <<
			"Got redirect for validator list from " <<
			sites_[siteIdx].activeResource->uri <<
			" to new location " << res[field::location];

		try
		{
			newLocation = std::make_shared<Site::Resource>(
				std::string(res[field::location]));
			++sites_[siteIdx].redirCount;
			if (newLocation->pUrl.scheme != "http" &&
				newLocation->pUrl.scheme != "https")
				throw std::runtime_error("invalid scheme in redirect " +
					newLocation->pUrl.scheme);
		}
		catch (std::exception &)
		{
			JLOG(j_.error()) <<
				"Invalid redirect location: " << res[field::location];
			throw;
		}
		return newLocation;
	}

	void
		ConfigSite::onSiteFetch(
			boost::system::error_code const& ec,
			detail::response_type&& res,
			std::size_t siteIdx)
	{
		{
			std::lock_guard <std::mutex> lock_sites{ sites_mutex_ };
			JLOG(j_.debug()) << "Got completion for "
				<< sites_[siteIdx].activeResource->uri;
			auto onError = [&](std::string const& errMsg, bool retry)
			{
				sites_[siteIdx].lastRefreshStatus.emplace(
					Site::Status{ clock_type::now(),
								ListDisposition::invalid,
								errMsg });
				if (retry)
					sites_[siteIdx].nextRefresh =
					clock_type::now() + error_retry_interval;
			};
			if (ec)
			{
				JLOG(j_.warn()) <<
					"Problem retrieving from " <<
					sites_[siteIdx].activeResource->uri <<
					" " <<
					ec.value() <<
					":" <<
					ec.message();
				onError("fetch error", true);
			}
			else
			{
				try
				{
					using namespace boost::beast::http;
					switch (res.result())
					{
					case status::ok:
						parseJsonResponse(res.body(), siteIdx, lock_sites);
						break;
					case status::moved_permanently:
					case status::permanent_redirect:
					case status::found:
					case status::temporary_redirect:
					{
						auto newLocation =
							processRedirect(res, siteIdx, lock_sites);
						assert(newLocation);
						// for perm redirects, also update our starting URI
						if (res.result() == status::moved_permanently ||
							res.result() == status::permanent_redirect)
						{
							sites_[siteIdx].startingResource = newLocation;
						}
						makeRequest(newLocation, siteIdx, lock_sites);
						return; // we are still fetching, so skip
								// state update/notify below
					}
					default:
					{
						JLOG(j_.warn()) <<
							"Request for validator list at " <<
							sites_[siteIdx].activeResource->uri <<
							" returned bad status: " <<
							res.result_int();
						onError("bad result code", true);
					}
					}
				}
				catch (std::exception& ex)
				{
					onError(ex.what(), false);
				}
			}
			sites_[siteIdx].activeResource.reset();
		}

		std::lock_guard <std::mutex> lock_state{ state_mutex_ };
		fetching_ = false;
		if (!stopping_)
			setTimer(lock_state);
		cv_.notify_all();
	}

	void
	ConfigSite::makeRequest (
		std::shared_ptr<Site::Resource> resource,
		std::size_t siteIdx,
		std::lock_guard<std::mutex>& sites_lock)
	{
		fetching_ = true;
		sites_[siteIdx].activeResource = resource;
		std::shared_ptr<detail::Work> sp;
		auto timeoutCancel =
			[this] ()
			{
				std::lock_guard <std::mutex> lock_state{state_mutex_};
				// docs indicate cancel_one() can throw, but this
				// should be reconsidered if it changes to noexcept
				try
				{
					timer_.cancel_one();
				}
				catch (boost::system::system_error const&)
				{
				}
			};
		auto onFetch =
			[this, siteIdx, timeoutCancel] (
				error_code const& err, detail::response_type&& resp)
			{
				timeoutCancel ();
				onSiteFetch (err, std::move(resp), siteIdx);
			};

		auto onFetchFile =
			[this, siteIdx, timeoutCancel] (
				error_code const& err, std::string const& resp)
			{
				timeoutCancel ();
				onTextFetch (err, resp, siteIdx);
			};

		JLOG (j_.debug()) << "Starting request for " << resource->uri;

		if (resource->pUrl.scheme == "https")
		{
			sp = std::make_shared<detail::WorkSSL>(
				resource->pUrl.domain,
				resource->pUrl.path,
				std::to_string(*resource->pUrl.port),
				ios_,
				j_,
				onFetch);
		}
		else if(resource->pUrl.scheme == "http")
		{
			sp = std::make_shared<detail::WorkPlain>(
				resource->pUrl.domain,
				resource->pUrl.path,
				std::to_string(*resource->pUrl.port),
				ios_,
				onFetch);
		}
		else
		{
			BOOST_ASSERT(resource->pUrl.scheme == "file");
			sp = std::make_shared<detail::WorkFile>(
				resource->pUrl.path,
				ios_,
				onFetchFile);
		}

		work_ = sp;
		sp->run ();
		// start a timer for the request, which shouldn't take more
		// than requestTimeout_ to complete
		std::lock_guard <std::mutex> lock_state{state_mutex_};
		timer_.expires_after (requestTimeout_);
		timer_.async_wait ([this, siteIdx] (boost::system::error_code const& ec)
			{
				this->onRequestTimeout (siteIdx, ec);
			});
	}


	void
		ConfigSite::onTextFetch(
			boost::system::error_code const& ec,
			std::string const& res,
			std::size_t siteIdx)
	{
		{
			std::lock_guard <std::mutex> lock_sites{ sites_mutex_ };
			try
			{
				if (ec)
				{
					JLOG(j_.warn()) <<
						"Problem retrieving from " <<
						sites_[siteIdx].activeResource->uri <<
						" " <<
						ec.value() <<
						":" <<
						ec.message();
					throw std::runtime_error{ "fetch error" };
				}

				parseJsonResponse(res, siteIdx, lock_sites);
			}
			catch (std::exception& ex)
			{
				sites_[siteIdx].lastRefreshStatus.emplace(
					Site::Status{ clock_type::now(),
					ListDisposition::invalid,
					ex.what() });
			}
			sites_[siteIdx].activeResource.reset();
		}

		std::lock_guard <std::mutex> lock_state{ state_mutex_ };
		fetching_ = false;
		if (!stopping_)
			setTimer(lock_state);
		cv_.notify_all();
	}

	Json::Value
		ConfigSite::getJson() const
	{
		using namespace std::chrono;
		using Int = Json::Value::Int;

		Json::Value jrr(Json::objectValue);
		Json::Value& jSites = (jrr[jss::validator_sites] = Json::arrayValue);
		{
			std::lock_guard<std::mutex> lock{ sites_mutex_ };
			for (Site const& site : sites_)
			{
				Json::Value& v = jSites.append(Json::objectValue);
				std::stringstream uri;
				uri << site.loadedResource->uri;
				if (site.loadedResource != site.startingResource)
					uri << " (redirects to " << site.startingResource->uri + ")";
				v[jss::uri] = uri.str();
				v[jss::next_refresh_time] = to_string(site.nextRefresh);
				if (site.lastRefreshStatus)
				{
					////v[jss::last_refresh_time] =
					////	to_string(site.lastRefreshStatus->refreshed);
					////v[jss::last_refresh_status] =
					////	to_string(site.lastRefreshStatus->disposition);
					////if (!site.lastRefreshStatus->message.empty())
					////	v[jss::last_refresh_message] =
					////	site.lastRefreshStatus->message;
				}
				v[jss::refresh_interval_min] =
					static_cast<Int>(site.refreshInterval.count());
			}
		}
		return jrr;
	}



	////void
	////	ConfigSite::onSiteFetch(
	////		boost::system::error_code const& ec,
	////		detail::response_type&& res,
	////		std::size_t siteIdx)
	////{
	////	if (!ec && res.result() != boost::beast::http::status::ok)
	////	{
	////		std::lock_guard <std::mutex> lock{ sites_mutex_ };
	////		JLOG(j_.warn()) <<
	////			"Request for validator list at " <<
	////			sites_[siteIdx].uri << " returned " << res.result_int();

	////		sites_[siteIdx].lastRefreshStatus.emplace(
	////			Site::Status{ clock_type::now(), ListDisposition::invalid });
	////	}
	////	else if (!ec)
	////	{
	////		std::lock_guard <std::mutex> lock{ sites_mutex_ };
	////		Json::Reader r;
	////		Json::Value body;
	////		if (r.parse(res.body().data(), body) &&
	////			body.isObject() &&
	////			body.isMember("blob") && body["blob"].isString() &&
	////			body.isMember("manifest") && body["manifest"].isString() &&
	////			body.isMember("signature") && body["signature"].isString() &&
	////			body.isMember("version") && body["version"].isInt())
	////		{

	////			auto const disp = applyList(
	////				body["manifest"].asString(),
	////				body["blob"].asString(),
	////				body["signature"].asString(),
	////				body["version"].asUInt(),
	////				sites_[siteIdx].activeResource->uri);

	////			sites_[siteIdx].lastRefreshStatus.emplace(
	////				Site::Status{ clock_type::now(), disp });

	////			if (ListDisposition::accepted == disp)
	////			{
	////				JLOG(j_.debug()) <<
	////					"Applied new validator list from " <<
	////					sites_[siteIdx].uri;
	////			}
	////			else if (ListDisposition::same_sequence == disp)
	////			{
	////				JLOG(j_.debug()) <<
	////					"Validator list with current sequence from " <<
	////					sites_[siteIdx].uri;
	////			}
	////			else if (ListDisposition::stale == disp)
	////			{
	////				JLOG(j_.warn()) <<
	////					"Stale validator list from " << sites_[siteIdx].uri;
	////			}
	////			else if (ListDisposition::untrusted == disp)
	////			{
	////				JLOG(j_.warn()) <<
	////					"Untrusted validator list from " <<
	////					sites_[siteIdx].uri;
	////			}
	////			else if (ListDisposition::invalid == disp)
	////			{
	////				JLOG(j_.warn()) <<
	////					"Invalid validator list from " <<
	////					sites_[siteIdx].uri;
	////			}
	////			else if (ListDisposition::unsupported_version == disp)
	////			{
	////				JLOG(j_.warn()) <<
	////					"Unsupported version validator list from " <<
	////					sites_[siteIdx].uri;
	////			}
	////			else
	////			{
	////				BOOST_ASSERT(false);
	////			}

	////			if (body.isMember("refresh_interval") &&
	////				body["refresh_interval"].isNumeric())
	////			{
	////				sites_[siteIdx].refreshInterval =
	////					std::chrono::minutes{ body["refresh_interval"].asUInt() };
	////			}
	////		}
	////		else
	////		{
	////			JLOG(j_.warn()) <<
	////				"Unable to parse JSON response from  " <<
	////				sites_[siteIdx].uri;

	////			sites_[siteIdx].lastRefreshStatus.emplace(
	////				Site::Status{ clock_type::now(), ListDisposition::invalid });
	////		}
	////	}
	////	else
	////	{
	////		std::lock_guard <std::mutex> lock{ sites_mutex_ };
	////		sites_[siteIdx].lastRefreshStatus.emplace(
	////			Site::Status{ clock_type::now(), ListDisposition::invalid });

	////		JLOG(j_.warn()) <<
	////			"Problem retrieving from " <<
	////			sites_[siteIdx].uri <<
	////			" " <<
	////			ec.value() <<
	////			":" <<
	////			ec.message();
	////	}

	////	std::lock_guard <std::mutex> lock{ state_mutex_ };
	////	fetching_ = false;
	////	if (!stopping_)
	////		setTimer();
	////	cv_.notify_all();
	////}

	//Json::Value
	//	ConfigSite::getJson() const
	//{
	//	using namespace std::chrono;
	//	using Int = Json::Value::Int;

	//	Json::Value jrr(Json::objectValue);
	//	Json::Value& jSites = (jrr[jss::validator_sites] = Json::arrayValue);
	//	{
	//		std::lock_guard<std::mutex> lock{ sites_mutex_ };
	//		for (Site const& site : sites_)
	//		{
	//			Json::Value& v = jSites.append(Json::objectValue);
	//			v[jss::uri] = site.uri;
	//			if (site.lastRefreshStatus)
	//			{
	//				//v[jss::last_refresh_time] =
	//				//	to_string(site.lastRefreshStatus->refreshed);
	//				//v[jss::last_refresh_status] =
	//				//	to_string(site.lastRefreshStatus->disposition);
	//			}

	//			v[jss::refresh_interval_min] =
	//				static_cast<Int>(site.refreshInterval.count());
	//		}
	//	}
	//	return jrr;
	//}
} // ripple
