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
#ifndef PEERSAFE_APP_MISC_CERTLIST_H_INCLUDED
#define PEERSAFE_APP_MISC_CERTLIST_H_INCLUDED

#include <ripple/basics/Log.h>
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <mutex>

namespace ripple {

class CertVerify
{
public:
    CertVerify()
    {
    }
    virtual ~CertVerify() = default;

public:
    std::pair<bool, std::string>
    verifyCredWithRootCerts(
        std::vector<std::string> const& rootCerts,
        std::string const& cred);

    virtual std::pair<bool, std::string>
    verifyCred(std::string const& cred) = 0;
};

class UserCertList : public CertVerify
{
private:
    boost::shared_mutex mutable mutex_;

    std::set<std::string> rootCertList_;
    std::set<std::string> rootCertListFromSite_;
    std::set<std::string> revokedList_;
    beast::Journal j_;

public:
    UserCertList(std::vector<std::string>& rootList, beast::Journal j)
        : rootCertList_(rootList.begin(),rootList.end()), j_(j)
    {
    }
    ~UserCertList() = default;

public:
    //std::vector<std::string>
    //getCertList() const;

    void
    setCertListFromSite(std::set<std::string> const& certList);

    void
    setRevoked(std::set<std::string>& revokedList);

public:
    std::pair<bool, std::string>
    verifyCred(std::string const& cred) override;
};

// ------------------------------------------------------------------------------
class PeerCertList : public CertVerify
{
private:
    std::vector<std::string> rootCertList_;
    std::string selfCred_;
    beast::Journal j_;

public:
    PeerCertList(
        std::vector<std::string>& rootList,
        std::string selfCred,
        beast::Journal j)
        : rootCertList_(rootList), selfCred_(selfCred), j_(j)
    {
    }
    ~PeerCertList() = default;

public:
    std::string getSelfCred() const;

public:
    std::pair<bool, std::string>
    verifyCred(std::string const& cred) override;
};

}  // namespace ripple

#endif
