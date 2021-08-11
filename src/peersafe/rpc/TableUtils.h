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

	class Schema;

	Json::Value generateError(const std::string& errMsg, bool ws = false);
	bool isChainSqlTableType(const std::string& transactionType);
    bool isChainsqlContractType(const std::string& transactionType);
	std::string hash(std::string &pk);
	uint160 generateNameInDB(uint32_t ledgerSeq,AccountID account,std::string sTableName);
	bool isDBConfigured(Schema& app);
	STEntry* getTableEntry(const STArray& aTables, Blob& vCheckName);
    std::tuple<std::shared_ptr<SLE const>, STObject*, STArray*>
		getTableEntry(ReadView const& view, const STTx& tx);
	std::tuple<std::shared_ptr<SLE>, STObject*, STArray*>
        getTableEntryVar(ApplyView& view, const STTx& tx);
    std::tuple<std::shared_ptr<SLE const>, STObject*, STArray*>
    getTableEntry(ReadView const& view, AccountID const& account, std::string sNameInDB);

	bool isTableSLEChanged(STObject* pEntry, LedgerIndex iLastSeq, bool bStrictEqual);
}

#endif
