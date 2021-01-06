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
	OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#ifndef RIPPLE_APP_MISC_VALIDATORSITE_H_INCLUDED
#define RIPPLE_APP_MISC_VALIDATORSITE_H_INCLUDED

#include <peersafe/app/misc/ConfigSite.h>
#include <ripple/app/misc/ValidatorList.h>
#include <ripple/app/misc/detail/Work.h>
#include <ripple/basics/Log.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/core/Config.h>
#include <ripple/json/json_value.h>
#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <memory>
#include <mutex>
#include <memory>

namespace ripple {

/**
   Validator Sites
   ---------------

   This class manages the set of configured remote sites used to fetch the
   latest published recommended validator lists.

   Lists are fetched at a regular interval.
   Fetched lists are expected to be in JSON format and contain the
   following fields:

        @li @c "blob": Base64-encoded JSON string containing a @c "sequence", @c
                "expiration", and @c "validators" field. @c "expiration"
   contains the Ripple timestamp (seconds since January 1st, 2000 (00:00 UTC))
   for when the list expires. @c "validators" contains an array of objects with
   a
        @c "validation_public_key" and optional @c "manifest" field.
        @c "validation_public_key" should be the hex-encoded master
   public key.
        @c "manifest" should be the base64-encoded validator manifest.

        @li @c "manifest": Base64-encoded serialization of a manifest containing
   the publisher's master and signing public keys.

        @li @c "signature": Hex-encoded signature of the blob using the
   publisher's signing key.

        @li @c "version": 1

        @li @c "refreshInterval" (optional, integer minutes).
                This value is clamped internally to [1,1440] (1 min - 1 day)
*/
class ValidatorSite : public ConfigSite
{
private:
    std::atomic<bool> waitingBeginConsensus_;

    ValidatorList& validators_;

public:
    ValidatorSite(
        Schema& app,
        ManifestCache& validatorManifests,
        boost::asio::io_service& ios,
        ValidatorList& validators,
        beast::Journal j,
        std::chrono::seconds timeout = std::chrono::seconds{20});
    ~ValidatorSite();

    void
    setWaitinBeginConsensus();

    /** Return JSON representation of configured validator sites
     */
    Json::Value
    getJson() const override;

public:
    virtual ListDisposition
    applyList(
        std::string const& manifest,
        std::string const& blob,
        std::string const& signature,
        std::uint32_t version,
        std::string siteUri) override;
};

}  // namespace ripple

#endif
