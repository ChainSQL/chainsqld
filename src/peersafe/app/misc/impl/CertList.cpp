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


namespace ripple {

	CertList::CertList(std::vector<std::string>& rootList, beast::Journal j)
		:root_cert_list_(rootList)
		,j_(j)
	{

	}

	CertList::~CertList()
	{

	}

	std::vector<std::string> CertList::getCertList() const
	{

		boost::unique_lock<boost::shared_mutex> read_lock{ mutex_ };
		std::vector<std::string>ret(root_cert_list_);
		return ret;
	}

	void CertList::setCertList(std::vector<std::string>& certList)
	{
		boost::unique_lock<boost::shared_mutex> write_lock{ mutex_ };

		root_cert_list_.clear();
		root_cert_list_ = certList;
	}

} // ripple
