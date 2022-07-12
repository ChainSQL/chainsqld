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


#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/json/json_value.h>
#include <ripple/net/RPCErr.h>
#include <ripple/protocol/jss.h>
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

	void
	addUserAuth(Json::Value& jvUsers,AccountID& userId,uint32_t flags)
	{
        Json::Value& jvObj = jvUsers.append(Json::objectValue);
        jvObj[jss::account] = to_string(userId);
        Json::Value& jvAuth = jvObj["authority"];
        jvAuth["insert"] = flags & lsfInsert ? true : false;
        jvAuth["delete"] = flags & lsfDelete ? true : false;
        jvAuth["update"] = flags & lsfUpdate ? true : false;
        jvAuth["select"] = flags & lsfSelect ? true : false;
	}

	Json::Value doTableAuthority(RPC::JsonContext& context)
	{
		Json::Value params(context.params);
		Json::Value ret(Json::objectValue);
		std::string ownerStr;

		if (!params.isMember(jss::owner))
		{
			return RPC::missing_field_error(jss::owner);
		}
		if (!params.isMember(jss::tablename))
		{
			return RPC::missing_field_error(jss::tablename);
		}
		
		ripple::hash_set<AccountID> ids;
		if (context.params.isMember(jss::accounts))
		{
			if (!context.params[jss::accounts].isArray())
				return RPC::make_error(rpcINVALID_PARAMS, "Field accounts is not array");
			ids = RPC::parseAccountIds(context.params[jss::accounts]);
		}

		AccountID ownerID;
		ownerStr = params[jss::owner].asString();
		auto jvAcceptedOwner = RPC::accountFromString(ownerID, ownerStr, true);
		if (jvAcceptedOwner)
		{
			return jvAcceptedOwner;
		}
		std::shared_ptr<ReadView const> ledgerConst;
		auto result = RPC::lookupLedger(ledgerConst, context);
		if (!ledgerConst)
			return result;
		if (!ledgerConst->exists(keylet::account(ownerID)))
			return rpcError(rpcACT_NOT_FOUND);

		auto tableName = params[jss::tablename].asString();
		if (tableName.empty())
		{
			return RPC::invalid_field_error(jss::tablename);
		}

		ret[jss::owner] = params[jss::owner].asString();
		ret[jss::tablename] = tableName;

        auto tup = getTableEntry(*ledgerConst, ownerID, tableName);
		auto pEntry = std::get<1>(tup);
		if (!pEntry)
		{
			return rpcError(rpcTAB_NOT_EXIST);
		}

		Json::Value& jvUsers = (ret["users"] = Json::arrayValue);
        if (pEntry->isFieldPresent(sfUsers))
        {
			auto users = pEntry->peekFieldArray(sfUsers);
			for (auto & user : users)  //check if there same user
			{
				auto userID = user.getAccountID(sfUser);
				auto flags = user.getFieldU32(sfFlags);

				if (!ids.empty() && ids.find(userID) == ids.end())
					continue;

				addUserAuth(jvUsers, userID, flags);
			}
        }
        else
        {
            auto nameInDB = pEntry->getFieldH160(sfNameInDB);
            forEachItem(
                *ledgerConst,
                ownerID,
                [&ids,&jvUsers,&nameInDB](std::shared_ptr<SLE const> const& sleCur) {
                    if (sleCur->getType() == ltTABLEGRANT)
                    {
                        if (sleCur->getFieldH160(sfNameInDB) == nameInDB)
                        {
                            auto userID = sleCur->getAccountID(sfUser);
                            auto flags = sleCur->getFieldU32(sfFlags);
                            if (!ids.empty() && ids.find(userID) == ids.end())
                                return;
                            addUserAuth(jvUsers, userID, flags);
                        }
                    }
                });
        }

		return ret;
	}

} // ripple
