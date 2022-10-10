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

#include <ripple/app/misc/ValidatorSite.h>
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


class CertList;


class CACertSite : public ValidatorSite
{
private:
    struct PublisherLst
    {
        //  bool available;
        //	std::vector<PublicKey> list;
        std::size_t sequence;
        TimeKeeper::time_point expiration;
    };

    enum class CAListDisposition {
        /// List is valid
        accepted = 0,

        /// Same sequence as current list
        same_sequence,

        /// List version is not supported
        //  unsupported_version,

        /// List signed by untrusted publisher key
        untrusted,

        /// Trusted publisher key, but seq is too old
        stale,

        /// Invalid format or signature
        invalid
    };

    using error_code = boost::system::error_code;
    using clock_type = std::chrono::system_clock;

public:
    CACertSite(Schema& app);
    ~CACertSite();

    void
    parseJsonResponse(
        std::string const& res,
        std::size_t siteIdx,
        std::lock_guard<std::mutex>& lock) override;

    bool
    load(
        std::vector<std::string> const& publisherKeys,
        std::vector<std::string> const& siteURIs);


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

    TimeKeeper&    timeKeeper_;
    ManifestCache& publisherManifests_;
    UserCertList&  userCertList_;

    // Currently supported version of publisher list format
    static constexpr std::uint32_t requiredListVersion = 1;

    //std::mutex mutable publisher_mutex_;
    //std::mutex mutable sites_mutex_;

    // Published lists stored by publisher master public key
    hash_map<PublicKey, PublisherLst> publisherLists_;
};

} // ripple
#endif