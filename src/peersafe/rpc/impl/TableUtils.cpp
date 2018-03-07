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

#include <ripple/protocol/JsonFields.h>
#include <ripple/basics/StringUtilities.h>
#include <peersafe/rpc/TableUtils.h>

namespace ripple {

	Json::Value generateRpcError(const std::string& errMsg)
	{
		Json::Value jvResult;
		jvResult[jss::error_message] = errMsg;
		jvResult[jss::error] = "error";
		return jvResult;
	}

	Json::Value generateWSError(const std::string& errMsg)
	{
		Json::Value jvResult;
		jvResult[jss::error_message] = errMsg;
		jvResult[jss::status] = "error";
		return jvResult;
	}

	Json::Value generateError(const std::string& errMsg, bool ws)
	{
		if (ws)
			return generateWSError(errMsg);
		else
			return generateRpcError(errMsg);
	}

	STEntry * getTableEntry(const STArray & aTables, std::string sCheckName)
	{
		auto iter(aTables.end());
		iter = std::find_if(aTables.begin(), aTables.end(),
			[sCheckName](STObject const &item) {
			if (!item.isFieldPresent(sfTableName))  return false;
			if (!item.isFieldPresent(sfDeleted))    return false;
			auto sTableName = strCopy(item.getFieldVL(sfTableName));
			return sTableName == sCheckName && item.getFieldU8(sfDeleted) != 1;
		});

		if (iter == aTables.end())  return NULL;

		return (STEntry*)(&(*iter));
	}

	STEntry * getTableEntry(const STArray & aTables, Blob& vCheckName)
	{
		auto iter(aTables.end());
		iter = std::find_if(aTables.begin(), aTables.end(),
			[vCheckName](STObject const &item) {
			if (!item.isFieldPresent(sfTableName))  return false;
			if (!item.isFieldPresent(sfDeleted))    return false;

			return item.getFieldVL(sfTableName) == vCheckName && item.getFieldU8(sfDeleted) != 1;
		});

		if (iter == aTables.end())  return NULL;

		return (STEntry*)(&(*iter));
	}

	STEntry * getTableEntry(ApplyView& view, const STTx& tx)
	{
		ripple::uint160  nameInDB;

		AccountID account;
		if (tx.isFieldPresent(sfOwner))
			account = tx.getAccountID(sfOwner);
		else if (tx.isFieldPresent(sfAccount))
			account = tx.getAccountID(sfAccount);
		else
			return NULL;
		auto const k = keylet::table(account);
		SLE::pointer pTableSle = view.peek(k);
		if (pTableSle == NULL)
			return NULL;

		auto &aTableEntries = pTableSle->peekFieldArray(sfTableEntries);

		if (!tx.isFieldPresent(sfTables))
			return NULL;
		auto const & sTxTables = tx.getFieldArray(sfTables);
		Blob vTxTableName = sTxTables[0].getFieldVL(sfTableName);
		uint160 uTxDBName = sTxTables[0].getFieldH160(sfNameInDB);
		return getTableEntry(aTableEntries, vTxTableName);
	}

	bool isChainSqlBaseType(const std::string& transactionType) {
		return transactionType == "TableListSet" ||
			transactionType == "SQLStatement" ||
			transactionType == "SQLTransaction";
	}
}
