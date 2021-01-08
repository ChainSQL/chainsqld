//------------------------------------------------------------------------------
/*
	This file is part of rippled: https://github.com/ripple/rippled
	Copyright (c) 2016 Ripple Labs Inc.

	Permission to use, copy, modify, and/or distribute this software for any
	purpose  with  or without fee is hereby granted, provided that the above
	copyright notice and this permission notice appear in all copies.

	THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
	WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
	MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
	ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
	WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
	ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
	OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE
*/
//==============================================================================

#include <ripple/app/misc/ValidatorList.h>
#include <ripple/app/misc/ValidatorSite.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/consensus/RCLValidations.h>
#include <ripple/basics/base64.h>
#include <ripple/basics/Slice.h>
#include <ripple/json/json_reader.h>
#include <ripple/protocol/jss.h>
#include <boost/algorithm/clamp.hpp>
#include <boost/regex.hpp>
#include <algorithm>

namespace ripple {

ValidatorSite::ValidatorSite(
    Schema& app,
    ManifestCache& validatorManifests,
    boost::asio::io_service& ios,
    ValidatorList& validators,
    beast::Journal j,
    std::chrono::seconds timeout)
    : ConfigSite(app, ios, validatorManifests, j, timeout)
    , validators_(validators)
{
}

ValidatorSite::~ValidatorSite()
{
}

void
ValidatorSite::setWaitinBeginConsensus()
{
    waitingBeginConsensus_ = true;
}

Json::Value
ValidatorSite::getJson() const
{
    using namespace std::chrono;
    using Int = Json::Value::Int;

    Json::Value jrr(Json::objectValue);
    Json::Value& jSites = (jrr[jss::validator_sites] = Json::arrayValue);
    {
        std::lock_guard lock{sites_mutex_};
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
                v[jss::last_refresh_time] =
                    to_string(site.lastRefreshStatus->refreshed);
                v[jss::last_refresh_status] =
                    to_string(site.lastRefreshStatus->disposition);
                if (!site.lastRefreshStatus->message.empty())
                    v[jss::last_refresh_message] =
                        site.lastRefreshStatus->message;
            }
            v[jss::refresh_interval_min] =
                static_cast<Int>(site.refreshInterval.count());
        }
    }
    return jrr;
}

ripple::ListDisposition
ValidatorSite::applyList(
    std::string const& manifest,
    std::string const& blob,
    std::string const& signature,
    std::uint32_t version,
    std::string siteUri)
{
    auto ret =
        validators_.applyList(manifest, blob, signature, version, nullptr);

    if (ret.disposition == ListDisposition::accepted)
    {
        // begin consensus after apply success
        if (waitingBeginConsensus_)
        {
            app_.getOPs().beginConsensus(
                app_.getLedgerMaster().getClosedLedger()->info().hash);
            waitingBeginConsensus_ = false;
        }
    }

    return ret.disposition;
}

}  // namespace ripple
