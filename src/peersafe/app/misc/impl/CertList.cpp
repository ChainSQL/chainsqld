//------------------------------------------------------------------------------
/*
        This file is part of chainsqld: https://github.com/chainsql/chainsqld
        Copyright (c) 2016-2020 Peersafe Technology Co., Ltd.

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

#include <peersafe/app/misc/CertList.h>
#include <peersafe/crypto/X509.h>

namespace ripple {

std::pair<bool, std::string>
CertVerify::verifyCredWithRootCerts(
    std::vector<std::string> const& rootCerts,
    std::string const& cred)
{
    std::string err;
    bool v = verifyCert(rootCerts, cred, err);
    return std::make_pair(v, err);
}

// ------------------------------------------------------------------------------
//// User certificate for client access
//std::vector<std::string>
//UserCertList::getCertList() const
//{
//    boost::unique_lock<boost::shared_mutex> read_lock{mutex_};
//    std::vector<std::string> ret(rootCertList_);
//    return ret;
//}

void
UserCertList::setCertListFromSite(std::set<std::string> const& certList)
{
    boost::unique_lock<boost::shared_mutex> write_lock{mutex_};

    rootCertListFromSite_.clear();
    rootCertListFromSite_ = certList;
}

void
UserCertList::setRevoked(std::set<std::string>& revokedList)
{
    boost::unique_lock<boost::shared_mutex> write_lock{mutex_};
    revokedList_ = revokedList;
}

std::pair<bool, std::string>
UserCertList::verifyCred(std::string const& cred)
{
    boost::shared_lock<boost::shared_mutex> read_lock{mutex_};

    std::set<std::string> setList(rootCertList_);
    std::set<std::string> setSiteList(rootCertListFromSite_);
    setList.merge(setSiteList);
    std::vector<std::string> vecCerts(setList.begin(), setList.end());

    if (vecCerts.empty() && cred.empty())
    {
        return {true, ""};
    }

    if (vecCerts.empty())
    {
        return {false, "user root certificates not configured"};
    }

    if (cred.empty())
    {
        return {false, "missing user certificate"};
    }

    //check if revoked
    auto serial = getSerialNumber(cred);
    if (revokedList_.find(serial) != revokedList_.end())
        return {false, "certificate is revoked."};


    return verifyCredWithRootCerts(vecCerts, cred);
}

// ------------------------------------------------------------------------------
// Peer certificate for node access

std::string
PeerCertList::getSelfCred() const
{
    return selfCred_;
}

std::pair<bool, std::string>
PeerCertList::verifyCred(std::string const& cred)
{
    if (rootCertList_.empty() && cred.empty())
    {
        return {true, ""};
    }

    if (rootCertList_.empty())
    {
        return {false, "peer root certificates not configed"};
    }

    if (cred.empty())
    {
        return {false, "missing peer certificate"};
    }

    return verifyCredWithRootCerts(rootCertList_, cred);
}

}  // namespace ripple
