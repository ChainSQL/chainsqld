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


class CertList
{
private:


    boost::shared_mutex mutable mutex_;

	std::vector<std::string>    root_cert_list_; // root cert list
	beast::Journal j_;
 
public:
	CertList(std::vector<std::string>& rootList,beast::Journal j);
    ~CertList();

	std::vector<std::string>  getCertList() const;

	void setCertList(std::vector<std::string>& certList);
};

} // ripple

#endif
