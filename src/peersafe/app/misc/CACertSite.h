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

#ifndef PEERSAFE_APP_MISC_CACERTSITE_H_INCLUDED
#define PEERSAFE_APP_MISC_CACERTSITE_H_INCLUDED

#include <peersafe/app/misc/ConfigSite.h>
#include <peersafe/app/misc/CertList.h>
#include <ripple/app/misc/Manifest.h>

#include <ripple/core/TimeKeeper.h>
#include <ripple/crypto/csprng.h>
#include <ripple/json/json_value.h>
#include <ripple/protocol/PublicKey.h>
#include <boost/iterator/counting_iterator.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <mutex>
#include <numeric>

namespace ripple {

/**
    CA Cert Sites
    ---------------

    This class manages the set of configured remote sites used to fetch the
    latest published recommended validator lists.

    Lists are fetched at a regular interval.
    Fetched lists are expected to be in JSON format and contain the following
    fields:

    @li @c "blob": Base64-encoded JSON string containing a @c "sequence", @c
        "expiration", and @c "validators" field. @c "expiration" contains the
        Ripple timestamp (seconds since January 1st, 2000 (00:00 UTC)) for when
        the list expires. @c "certs" contains an array of objects with a
        @c "cert" and optional @c "manifest" field.
        @c "cert" should be the hex-encoded master public key.
        @c "manifest" should be the base64-encoded validator manifest.

    @li @c "manifest": Base64-encoded serialization of a manifest containing the
        publisher's master and signing public keys.

    @li @c "signature": Hex-encoded signature of the blob using the publisher's
        signing key.

    @li @c "version": 1

    @li @c "refreshInterval" (optional)
*/
	class CACertSite : public ConfigSite
{
   
public:
	CACertSite(
		ManifestCache& validatorManifests,
		ManifestCache& publisherManifests,
		CertList& certList,
		TimeKeeper& timeKeeper,
        boost::asio::io_service& ios,
        beast::Journal j);
    ~CACertSite();


	virtual Json::Value
		getJson() const ;

	virtual  ListDisposition applyList(
		std::string const& manifest,
		std::string const& blob,
		std::string const& signature,
		std::uint32_t version);



	/** Stop trusting publisher's list of keys.

	@param publisherKey Publisher public key

	@return `false` if key was not trusted

	@par Thread Safety

	Calling public member function is expected to lock mutex
	*/
	bool
		removePublisherList(PublicKey const& publisherKey);

private:
	/** Check response for trusted valid published list

	@return `ListDisposition::accepted` if list can be applied

	@par Thread Safety

	Calling public member function is expected to lock mutex
	*/
	ListDisposition
		verify(
			Json::Value& list,
			PublicKey& pubKey,
			std::string const& manifest,
			std::string const& blob,
			std::string const& signature);

	CertList&   certList_;

	//ManifestCache& validatorManifests_;
	ManifestCache& publisherManifests_;
	TimeKeeper& timeKeeper_;

	boost::shared_mutex mutable mutex_;

	// Currently supported version of publisher list format
	static constexpr std::uint32_t requiredListVersion = 1;


	virtual void onAccepted();

};

} // ripple

#endif
