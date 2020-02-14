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

#ifndef PEERSAFE_APP_MISC_CONFIGSITE_H_INCLUDED
#define PEERSAFE_APP_MISC_CONFIGSITE_H_INCLUDED


#include <ripple/core/TimeKeeper.h>
#include <ripple/crypto/csprng.h>
#include <ripple/app/misc/Manifest.h>
#include <ripple/app/misc/detail/Work.h>
#include <ripple/basics/Log.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/json/json_value.h>
#include <boost/asio.hpp>
#include <mutex>

namespace ripple {


	enum class ListDisposition
	{
		/// List is valid
		accepted = 0,

		/// Same sequence as current list
		same_sequence,

		/// List version is not supported
		unsupported_version,

		/// List signed by untrusted publisher key
		untrusted,

		/// Trusted publisher key, but seq is too old
		stale,

		/// Invalid format or signature
		invalid
	};

	//std::string
	//	to_string(ListDisposition disposition);



/**
    Validator Sites
    ---------------

    This class manages the set of configured remote sites used to fetch the
    latest published recommended validator lists.

    Lists are fetched at a regular interval.
    Fetched lists are expected to be in JSON format and contain the following
    fields:

    @li @c "blob": Base64-encoded JSON string containing a @c "sequence", @c
        "expiration", and @c "validators" field. @c "expiration" contains the
        Ripple timestamp (seconds since January 1st, 2000 (00:00 UTC)) for when
        the list expires. @c "validators" contains an array of objects with a
        @c "validation_public_key" and optional @c "manifest" field.
        @c "validation_public_key" should be the hex-encoded master public key.
        @c "manifest" should be the base64-encoded validator manifest.

    @li @c "manifest": Base64-encoded serialization of a manifest containing the
        publisher's master and signing public keys.

    @li @c "signature": Hex-encoded signature of the blob using the publisher's
        signing key.

    @li @c "version": 1

    @li @c "refreshInterval" (optional)
*/
class ConfigSite
{
    friend class Work;

	struct PublisherLst
	{
		bool available;
	//	std::vector<PublicKey> list;
		std::size_t sequence;
		TimeKeeper::time_point expiration;
	};

private:
    using error_code = boost::system::error_code;
    using clock_type = std::chrono::system_clock;



    boost::asio::io_service& ios_;
 //   ValidatorList& validators_;


    std::mutex mutable state_mutex_;

    std::condition_variable cv_;
    std::weak_ptr<detail::Work> work_;
    boost::asio::basic_waitable_timer<clock_type> timer_;

    // A list is currently being fetched from a site
    std::atomic<bool> fetching_;

    // One or more lists are due to be fetched
    std::atomic<bool> pending_;
    std::atomic<bool> stopping_;

public:

	struct Site
	{
		struct Status
		{
			clock_type::time_point refreshed;
			ListDisposition disposition;
		};

		std::string uri;
		parsedURL pUrl;
		std::chrono::minutes refreshInterval;
		clock_type::time_point nextRefresh;
		boost::optional<Status> lastRefreshStatus;
	};

	std::mutex mutable  publisher_mutex_;

	std::mutex mutable sites_mutex_;
    // The configured list of URIs for fetching lists
    std::vector<Site> sites_;

public:

	beast::Journal j_;

	// Published lists stored by publisher master public key
	hash_map<PublicKey, PublisherLst> publisherLists_;
	ManifestCache& validatorManifests_;

public:
	ConfigSite(
        boost::asio::io_service& ios,
		ManifestCache& validatorManifests,
        beast::Journal j);
    ~ConfigSite();

    /** Load configured site URIs.

        @param siteURIs List of URIs to fetch published validator lists

        @par Thread Safety

        May be called concurrently

        @return `false` if an entry is invalid or unparsable
    */
	bool
		load(
			std::vector<std::string> const& publisherKeys,
			std::vector<std::string> const& siteURIs);

	bool
		load(
			std::vector<std::string> const& siteURIs);

    /** Start fetching lists from sites

        This does nothing if list fetching has already started

        @par Thread Safety

        May be called concurrently
    */
	virtual  void
    start ();

    /** Wait for current fetches from sites to complete

        @par Thread Safety

        May be called concurrently
    */
	virtual void
    join ();

    /** Stop fetching lists from sites

        This blocks until list fetching has stopped

        @par Thread Safety

        May be called concurrently
    */
	virtual  void
    stop ();

    /** Return JSON representation of configured validator sites
     */
	virtual Json::Value
		getJson() const = 0;

public:
    /// Queue next site to be fetched
	virtual  void
    setTimer ();

    /// Fetch site whose time has come
	virtual  void
    onTimer (
        std::size_t siteIdx,
        error_code const& ec);

    /// Store latest list fetched from site
	virtual  void
    onSiteFetch (
        boost::system::error_code const& ec,
        detail::response_type&& res,
        std::size_t siteIdx);


	virtual  ListDisposition applyList(
		std::string const& manifest,
		std::string const& blob,
		std::string const& signature,
		std::uint32_t version) = 0;

	virtual void onAccepted() = 0;

};

} // ripple

#endif
