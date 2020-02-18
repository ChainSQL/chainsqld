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

#include <peersafe/app/misc/CACertSite.h>

#include <ripple/basics/Slice.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/json/json_reader.h>
#include <ripple/protocol/JsonFields.h>
#include <beast/core/detail/base64.hpp>



namespace ripple {

	CACertSite::CACertSite(
		ManifestCache& validatorManifests,
		ManifestCache& publisherManifests,
		CertList& certList,
		TimeKeeper& timeKeeper,
		boost::asio::io_service& ios,
		beast::Journal j)
		: ConfigSite(ios, validatorManifests,j)
		, certList_(certList)
		, publisherManifests_(publisherManifests)
		, timeKeeper_(timeKeeper)
	{
	}


	CACertSite::~CACertSite()
	{

	}


	Json::Value CACertSite::getJson() const
	{
		Json::Value jrr(Json::objectValue);

		return jrr;
	}

	ripple::ListDisposition CACertSite::applyList(std::string const& manifest, std::string const& blob, std::string const& signature, std::uint32_t version)
	{
		if (version != requiredListVersion)
			return ListDisposition::unsupported_version;

		boost::unique_lock<boost::shared_mutex> lock{ mutex_ };

		Json::Value list;
		PublicKey pubKey;
		auto const result = verify(list, pubKey, manifest, blob, signature);
		if (result != ListDisposition::accepted)
			return result;

		// update CA root certs
		Json::Value const& newList = list["certs"];

		std::vector<std::string>    rootCertLst;
		for (auto const& val : newList)
		{
			if (val.isObject() &&
				val.isMember("cert") &&
				val["cert"].isString())
			{
				rootCertLst.push_back(val["cert"].asString());
			}
		}
		certList_.setCertList(rootCertLst);

		return ListDisposition::accepted;
		 
	}

	
	bool CACertSite::removePublisherList(PublicKey const& publisherKey)
	{
		auto const iList = publisherLists_.find(publisherKey);
		if (iList == publisherLists_.end())
			return false;

		JLOG(j_.debug()) <<
			"Removing validator list for revoked publisher " <<
			toBase58(TokenType::TOKEN_NODE_PUBLIC, publisherKey);

		//for (auto const& val : iList->second.list)
		//{
		//	auto const& iVal = keyListings_.find(val);
		//	if (iVal == keyListings_.end())
		//		continue;

		//	if (iVal->second <= 1)
		//		keyListings_.erase(iVal);
		//	else
		//		--iVal->second;
		//}

		//iList->second.list.clear();
		iList->second.available = false;

		return true;
	}

	ripple::ListDisposition CACertSite::verify(Json::Value& list, PublicKey& pubKey, std::string const& manifest, std::string const& blob, std::string const& signature)
	{
		auto m = Manifest::make_Manifest(beast::detail::base64_decode(manifest));

		if (!m || !publisherLists_.count(m->masterKey))
			return ListDisposition::untrusted;

		pubKey = m->masterKey;
		auto const revoked = m->revoked();

		auto const result = publisherManifests_.applyManifest(
			std::move(*m));

		if (revoked && result == ManifestDisposition::accepted)
		{
			removePublisherList(pubKey);
			publisherLists_.erase(pubKey);
		}

		if (revoked || result == ManifestDisposition::invalid)
			return ListDisposition::untrusted;

		auto const sig = strUnHex(signature);
		auto const data = beast::detail::base64_decode(blob);
		if (!sig.second ||
			!ripple::verify(
				publisherManifests_.getSigningKey(pubKey),
				makeSlice(data),
				makeSlice(sig.first)))
			return ListDisposition::invalid;

		Json::Reader r;
		if (!r.parse(data, list))
			return ListDisposition::invalid;

		if (list.isMember("sequence") && list["sequence"].isInt() &&
			list.isMember("expiration") && list["expiration"].isInt() &&
			list.isMember("certs") && list["certs"].isArray())
		{
			auto const sequence = list["sequence"].asUInt();
			auto const expiration = TimeKeeper::time_point{
				TimeKeeper::duration{ list["expiration"].asUInt() } };
			if (sequence < publisherLists_[pubKey].sequence ||
				expiration <= timeKeeper_.now())
				return ListDisposition::stale;
			else if (sequence == publisherLists_[pubKey].sequence)
				return ListDisposition::same_sequence;
		}
		else
		{
			return ListDisposition::invalid;
		}

		return ListDisposition::accepted;
	}

	void CACertSite::onAccepted()
	{

	}

} // ripple
