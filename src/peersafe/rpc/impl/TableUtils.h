//------------------------------------------------------------------------------
/*
 This file is part of chainsqld: https://github.com/chainsql/chainsqld
 Copyright (c) 2016-2018 Peersafe Technology Co., Ltd.
 
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

#ifndef RIPPLE_RPC_TABLE_UTILS_H_INCLUDED
#define RIPPLE_RPC_TABLE_UTILS_H_INCLUDED


#include <ripple/json/json_value.h>
#include <peersafe/protocol/STEntry.h>

namespace ripple {

	Json::Value generateError(const std::string& errMsg, bool ws = false);
	STEntry * getTableEntry(const STArray & aTables, std::string sCheckName);
	bool isChainSqlBaseType(const std::string& transactionType);
}

#endif
