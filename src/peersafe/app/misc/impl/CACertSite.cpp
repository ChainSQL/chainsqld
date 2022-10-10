//------------------------------------------------------------------------------
/*
This file is part of chainsqld: https://github.com/chainsql/chainsqld
Copyright (c) 2016-2021 Peersafe Technology Co., Ltd.

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

#include <ripple/app/misc/ValidatorList.h>
#include <ripple/basics/Slice.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/basics/base64.h>
#include <ripple/json/json_reader.h>
#include <ripple/protocol/jss.h>
#include <boost/algorithm/clamp.hpp>
#include <peersafe/app/misc/CACertSite.h>
#include <peersafe/app/misc/CertList.h>

namespace ripple {

CACertSite::CACertSite(Schema& app)
    : ValidatorSite(app)
    , timeKeeper_(app.timeKeeper())
    , publisherManifests_(app.publisherManifests())
    , userCertList_(app.userCertList())

{
}

CACertSite::~CACertSite()
{
}

void
CACertSite::parseJsonResponse(
    std::string const& res,
    std::size_t siteIdx,
    std::lock_guard<std::mutex>& lock)
{
    Json::Reader r;
    Json::Value body;
    if (!r.parse(res.data(), body))
    {
        JLOG(j_.warn()) << "Unable to parse JSON response from  "
                        << sites_[siteIdx].activeResource->uri;
        throw std::runtime_error{"bad json"};
    }

    if (!body.isObject() || !body.isMember("blob") ||
        !body["blob"].isString() || !body.isMember("manifest") ||
        !body["manifest"].isString() || !body.isMember("signature") ||
        !body["signature"].isString() || !body.isMember("version") ||
        !body["version"].isInt())
    {
        JLOG(j_.warn()) << "Missing fields in JSON response from  "
                        << sites_[siteIdx].activeResource->uri;
        throw std::runtime_error{"missing fields"};
    }

    auto const manifest = body["manifest"].asString();
    auto const blob = body["blob"].asString();
    auto const signature = body["signature"].asString();
    auto const& uri = sites_[siteIdx].activeResource->uri;

    Json::Value list;
    PublicKey pubKey;
    auto const disp = verify(list, pubKey, manifest, blob, signature);

    sites_[siteIdx].lastRefreshStatus.emplace(
        Site::Status{clock_type::now(), disp, ""});

    switch (disp)
    {
        case ListDisposition::accepted:
        {
            JLOG(j_.debug()) << "Applied new cert list from " << uri;
            // update CA root certs
            Json::Value const& newList = list["certs"];

            std::set<std::string> root_cert_list;  // root cert list
            for (auto const& val : newList)
            {
                if (val.isObject() && val.isMember("cert") &&
                    val["cert"].isString())
                {
                    root_cert_list.insert(val["cert"].asString());
                }
            }

            std::set<std::string> revoked_list;  //revoked cert list
            Json::Value const& revokeList = list["revoked"];
            for (auto const& val : revokeList)
            {
                if (val.isObject() && val.isMember("serial") &&
                    val["serial"].isString())
                {
                    revoked_list.insert(val["serial"].asString());
                }
            }

            // Update publisher's list
            auto& publisher = publisherLists_[pubKey];
            publisher.sequence = list["sequence"].asUInt();
            publisher.expiration = TimeKeeper::time_point::max();

            userCertList_.setCertListFromSite(root_cert_list);
            userCertList_.setRevoked(revoked_list);
        }
        break;
        case ListDisposition::same_sequence:
            JLOG(j_.debug())
                << "CA list with current sequence from " << uri;
            break;
        case ListDisposition::stale:
            JLOG(j_.warn()) << "Stale CA list from " << uri;
            break;
        case ListDisposition::untrusted:
            JLOG(j_.warn()) << "Untrusted CA list from " << uri;
            break;
        case ListDisposition::invalid:
            JLOG(j_.warn()) << "Invalid CA list from " << uri;
            break;
        //case ListDisposition::unsupported_version:
        //    JLOG(j_.warn())
        //        << "Unsupported version CA list from " << uri;
        //    break;
        default:
            BOOST_ASSERT(false);
    }

    if (body.isMember("refresh_interval") &&
        body["refresh_interval"].isNumeric())
    {
        using namespace std::chrono_literals;
        std::chrono::minutes const refresh = boost::algorithm::clamp(
            std::chrono::minutes{body["refresh_interval"].asUInt()}, 1min, 24h);
        sites_[siteIdx].refreshInterval = refresh;
        sites_[siteIdx].nextRefresh =
            clock_type::now() + sites_[siteIdx].refreshInterval;
    }
}

bool
CACertSite::load(
    std::vector<std::string> const& publisherKeys,
    std::vector<std::string> const& siteURIs)
{

    if (publisherKeys.empty() || siteURIs.empty()) {

        JLOG(j_.trace()) << "publisherKeys or siteURIs is empty ";
        return true;
    }

    JLOG(j_.debug())
        << "Loading configured trusted CA list siteURIs and  publisher keys";

    std::size_t count = 0;
    for (auto key : publisherKeys)
    {
        JLOG(j_.trace()) << "Processing '" << key << "'";

        auto const ret = strUnHex(key);

        if (!ret || !ret->size())
        {
            JLOG(j_.error()) << "Invalid CA list publisher key: " << key;
            return false;
        }

        auto id = PublicKey(Slice{ret->data(), ret->size()});
        if (publisherLists_.count(id))
        {
            JLOG(j_.warn()) << "Duplicate CA list publisher key: " << key;
            continue;
        }

        // init sequence and expiration
        publisherLists_[id].sequence   = 0;
        //publisherLists_[id].expiration = timeKeeper_.now();
        publisherLists_[id].expiration = TimeKeeper::time_point::max();;

        ++count;
    }

    JLOG(j_.debug()) << "Loaded " << count << " keys";

    return ValidatorSite::load(siteURIs);
}

ripple::ListDisposition
CACertSite::verify(
    Json::Value& list,
    PublicKey& pubKey,
    std::string const& manifest,
    std::string const& blob,
    std::string const& signature)
{
    auto m = deserializeManifest(base64_decode(manifest));

    if (!m || !publisherLists_.count(m->masterKey))
        return ListDisposition::untrusted;

    pubKey = m->masterKey;
    auto const revoked = m->revoked();

    auto const result = publisherManifests_.applyManifest(std::move(*m));

    if (revoked || result == ManifestDisposition::invalid)
        return ListDisposition::untrusted;

    auto const sig = strUnHex(signature);
    auto const data = base64_decode(blob);
    if (!sig ||
        !ripple::verify(
            publisherManifests_.getSigningKey(pubKey),
            makeSlice(data),
            makeSlice(*sig)))
        return ListDisposition::invalid;

    Json::Reader r;
    if (!r.parse(data, list))
        return ListDisposition::invalid;

    if (list.isMember("sequence") && list["sequence"].isInt() &&
        list.isMember("expiration") && (list["expiration"].isInt()||list["expiration"].isUInt())  &&
        list.isMember("certs") && list["certs"].isArray())
    {
        auto const sequence = list["sequence"].asUInt();
        auto const expiration = TimeKeeper::time_point{
            TimeKeeper::duration{list["expiration"].asUInt()}};
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

}  // namespace ripple
