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

#include <ripple/protocol/jss.h>
#include <ripple/basics/StringUtilities.h>
#include <peersafe/rpc/TableUtils.h>
#include <ripple/protocol/digest.h>
#include <peersafe/schema/Schema.h>
#include <peersafe/app/sql/TxStore.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/Feature.h>

namespace ripple {

	std::string hash(std::string &pk)
	{
		ripesha_hasher rsh;
		rsh(pk.c_str(), pk.size());
		auto const d = static_cast<
			ripesha_hasher::result_type>(rsh);
		std::string str;
		str = strHex(d);
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

	std::tuple<std::shared_ptr<SLE const>, STObject*, STArray*>
    getTableEntryInner(ReadView const& view,AccountID const& accountId,std::string const& sTableName)
    {
        STObject* pEntry = nullptr;
        STArray* tableEntries = nullptr;
        Keylet key = keylet::table(accountId, sTableName);
        if (auto sle = view.read(key))
        {
            pEntry = (STObject*)&(sle->getFieldObject(sfTableEntry));
            return std::make_tuple(sle, pEntry, tableEntries);
        }
        else
        {
            key = keylet::tablelist(accountId);
            sle = view.read(key);
            if (sle)  // table exist
            {
		auto tableNameBlob = strCopy(sTableName);
                tableEntries = (STArray*)&(sle->getFieldArray(sfTableEntries));
                pEntry = getTableEntry(*tableEntries, tableNameBlob);
            }
            return std::make_tuple(sle, pEntry, tableEntries);
        }
        return std::make_tuple(nullptr, pEntry, tableEntries);
    }

    std::tuple<std::shared_ptr<SLE>, STObject*, STArray*>
    getTableEntryVar(ApplyView& view, const STTx& tx)
    {
        auto accountId = tx.getAccountID(sfAccount);
        auto tables = tx.getFieldArray(sfTables);
        Blob vTableNameStr = tables[0].getFieldVL(sfTableName);
	const std::string sTableName = strCopy(vTableNameStr);
        STObject* pEntry = nullptr;
        STArray* tableEntries = nullptr;
        Keylet key = keylet::table(accountId, sTableName);
        if (auto sle = view.peek(key))
        {
            pEntry = &(sle->peekFieldObject(sfTableEntry));
            return std::make_tuple(sle, pEntry, tableEntries);
        }
        else
        {
            key = keylet::tablelist(accountId);
            sle = view.peek(key);
            if (sle)  // table exist
            {
		auto tableNameBlob = strCopy(sTableName);
                tableEntries = (STArray*)&(sle->getFieldArray(sfTableEntries));
                pEntry = getTableEntry(*tableEntries, tableNameBlob);
            }
            return std::make_tuple(sle, pEntry, tableEntries);
        }
        return std::make_tuple(nullptr, pEntry, tableEntries);
    }
    STEntry*
    getTableEntry(const STArray& aTables, Blob& vCheckName)
    {
        auto iter(aTables.end());
        iter = std::find_if(
            aTables.begin(), aTables.end(), [vCheckName](STObject const& item) {
                if (!item.isFieldPresent(sfTableName))
                    return false;

                return item.getFieldVL(sfTableName) == vCheckName;
            });

        if (iter == aTables.end())
            return NULL;

        return (STEntry*)(&(*iter));
    }

    std::tuple<std::shared_ptr<SLE const>, STObject*, STArray*>
    getTableEntry(ReadView const& view, const STTx& tx)
    {
        auto accountId = tx.getAccountID(sfAccount);
        auto tables = tx.getFieldArray(sfTables);
        Blob vTableNameStr = tables[0].getFieldVL(sfTableName);
        auto sTableName = strCopy(vTableNameStr);

        return getTableEntryInner(view, accountId, sTableName);
    }

	std::tuple<std::shared_ptr<SLE const>, STObject*, STArray*>
    getTableEntry(ReadView const& view, AccountID const& accountId, std::string const& sTableName)
    {
        return getTableEntryInner(view, accountId, sTableName);
    }

	bool
    isTableSLEChanged(STObject* pEntry, LedgerIndex iLastSeq, bool bStrictEqual)
    {
		if (pEntry == nullptr)
			return false;
		return (bStrictEqual ? pEntry->getFieldU32(sfPreviousTxnLgrSeq) == iLastSeq
                : pEntry->getFieldU32(sfPreviousTxnLgrSeq) >= iLastSeq);
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

	uint160 generateNameInDB(uint32_t ledgerSeq, AccountID account, std::string sTableName)
	{
		std::string tmp = std::to_string(ledgerSeq) + to_string(account) + sTableName;
		std::string str = hash(tmp);
		return from_hex_text<uint160>(str);
	}

	bool isDBConfigured(Schema& app)
	{
		if (app.getTxStoreDBConn().GetDBConn() == nullptr ||
			app.getTxStoreDBConn().GetDBConn()->getSession().get_backend() == nullptr)
			return false;
		else
			return true;
	}
}
