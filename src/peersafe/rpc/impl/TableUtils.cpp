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
#include <ripple/protocol/digest.h>
#include <ripple/app/main/Application.h>
#include <peersafe/app/sql/TxStore.h>
#include <ripple/protocol/ErrorCodes.h>

namespace ripple {

	std::string hash(std::string &pk)
	{
		ripesha_hasher rsh;
		rsh(pk.c_str(), pk.size());
		auto const d = static_cast<
			ripesha_hasher::result_type>(rsh);
		std::string str;
		str = strHex(d.data(), d.size());
		return str;
	}

	Json::Value generateRpcError(const std::string& errMsg)
	{
		Json::Value jvResult;
		jvResult[jss::error_message] = errMsg;
		//jvResult[jss::error] = "error";
		jvResult[jss::status] = "error";
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
			auto sTableName = strCopy(item.getFieldVL(sfTableName));
			return sTableName == sCheckName;
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

			return item.getFieldVL(sfTableName) == vCheckName;
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

	bool isChainSqlTableType(const std::string& transactionType) {
		return transactionType == "TableListSet" ||
			transactionType == "SQLStatement" ||
			transactionType == "SQLTransaction";
	}

    bool isChainsqlContractType(const std::string& transactionType)
    {
        return transactionType == "Contract";
    }

	uint160 generateNameInDB(uint32 ledgerSeq, AccountID account, std::string sTableName)
	{
		std::string tmp = to_string(ledgerSeq) + to_string(account) + sTableName;
		std::string str = hash(tmp);
		return from_hex_text<uint160>(str);
	}

	bool isDBConfigured(Application& app)
	{
		if (app.getTxStoreDBConn().GetDBConn() == nullptr ||
			app.getTxStoreDBConn().GetDBConn()->getSession().get_backend() == nullptr)
			return false;
		else
			return true;
	}
}
