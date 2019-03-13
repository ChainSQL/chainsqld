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

#include <BeastConfig.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/json/json_value.h>
#include <ripple/net/RPCErr.h>
#include <ripple/protocol/JsonFields.h>
#include <ripple/rpc/Context.h>
#include <ripple/rpc/impl/TransactionSign.h>
#include <ripple/rpc/Role.h>
#include <ripple/rpc/handlers/Handlers.h>
#include <peersafe/app/table/TableSync.h>
#include <peersafe/app/sql/TxStore.h>
#include <peersafe/basics/characterUtilities.h>
#include <peersafe/rpc/TableUtils.h>
#include <ripple/rpc/impl/RPCHelpers.h>

namespace ripple {

	Json::Value doTableAuthority(RPC::Context& context)
	{
		Json::Value params(context.params);
		Json::Value ret(Json::objectValue);
		std::string ownerStr;

		if (!params.isMember(jss::Owner))
		{
			return RPC::missing_field_error(jss::Owner);
		}
		if (!params.isMember(jss::tablename))
		{
			return RPC::missing_field_error(jss::tablename);
		}
		
		ripple::hash_set<AccountID> ids;
		if (context.params.isMember(jss::accounts))
		{
			if (!context.params[jss::accounts].isArray())
				return rpcError(rpcINVALID_PARAMS, ret);
				//return generateError("accounts is not array");
			ids = RPC::parseAccountIds(context.params[jss::accounts]);
			if (ids.empty())
				return rpcError(rpcINVALID_PARAMS, ret);
				//return generateError("accounts does not include account");
		}

		AccountID ownerID;
		auto jvAcceptedOwner = RPC::accountFromString(ownerID, ownerStr, true);
		if (jvAcceptedOwner)
		{
			return jvAcceptedOwner;
		}

		auto tableName = params[jss::tablename].asString();

		auto ledger = context.ledgerMaster.getValidatedLedger();
		STEntry* pEntry = nullptr;
		if (ledger)
		{
			auto id = keylet::table(ownerID);
			auto const tablesle = ledger->read(id);

			if (tablesle)
			{
				auto aTableEntries = tablesle->getFieldArray(sfTableEntries);
				pEntry = getTableEntry(aTableEntries, tableName);
				if (!pEntry)
				{
					return rpcError(rpcTAB_NOT_EXIST, ret);
					//return generateError("Table not found!");
				}

				Json::Value& jvUsers = (ret["users"] = Json::arrayValue);
				auto users = pEntry->peekFieldArray(sfUsers);
				for (auto & user : users)  //check if there same user
				{
					auto userID = user.getAccountID(sfUser);
					auto flags = user.getFieldU32(sfFlags);

					if (!ids.empty() && ids.find(userID) == ids.end())
						continue;

					Json::Value& jvObj = jvUsers.append(Json::objectValue);
					jvObj[jss::account] = to_string(userID);
					Json::Value& jvAuth = jvObj["authority"];
					jvAuth["insert"] = flags & lsfInsert ? true : false;
					jvAuth["delete"] = flags & lsfDelete ? true : false;
					jvAuth["update"] = flags & lsfUpdate ? true : false;
					jvAuth["select"] = flags & lsfSelect ? true : false;
				}
			}
			else
			{
				return rpcError(rpcTAB_NOT_EXIST, ret);
				//return generateError("Table not found!");
			}
		}
		ret[jss::status] = "success";
		return ret;
	}

} // ripple
