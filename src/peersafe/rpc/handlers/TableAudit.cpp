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

namespace ripple {

    Json::Value doTableAudit(RPC::Context& context)
    {
        Json::Value ret(context.params);

        if (ret[jss::tx_json].size() != 3)
        {
            ret[jss::error] = "error";
            ret[jss::error_message] = "must follow 3 params,in format: 'owner tableName' 'sql' 'path'";
            ret.removeMember(jss::tx_json);
            return ret;
        }

        std::string sTableName, sNameInDB, sAccount;
        //1. param similar to synctables
        std::string sNormal = ret[jss::tx_json][uint32_t(0)].asString();
        //2. SQL for check
        std::string sSQL = ret[jss::tx_json][uint32_t(1)].asString();
        //3.path 
        std::string sFullPath = ret[jss::tx_json][uint32_t(2)].asString();        
       
        auto retSet = context.app.getTableSync().StartAuditTable(sNormal, sSQL,sFullPath);
        if (!retSet.first)
        {
            ret[jss::error] = "error.";

            std::string sErrorMsg;
            TransGBK_UTF8(retSet.second, sErrorMsg, false);
            ret[jss::error_message] = sErrorMsg;

            ret.removeMember(jss::tx_json);
            return ret;
        }
        else
        {

            for (int i = 0; i < ret[jss::tx_json].size(); i++)
            {
                std::string sDest = "";
                if (!TransGBK_UTF8(ret[jss::tx_json][i].asString(), sDest, false))
                {
                    return rpcError(rpcINTERNAL);
                }

                ret[jss::tx_json][i] = sDest;
            }
        }

        ret["nickName"] = retSet.second;
        return ret;
    }

    Json::Value doTableAuditStop(RPC::Context& context)
    {
        Json::Value ret(context.params);

        if (ret[jss::tx_json].size() != 1)
        {
            ret[jss::error] = "error.";
            ret[jss::error_message] = "must follow 1 params,in format: job_id.";
            ret.removeMember(jss::tx_json);
            return ret;
        }

        std::string sNickName = ret[jss::tx_json][0U].asString();

        auto retPair = context.app.getTableSync().StopAuditTable(sNickName);        
        if (!retPair.first)
        {
            ret[jss::error] = "error.";

            std::string sErrorMsg;
            TransGBK_UTF8(retPair.second, sErrorMsg, false);
            ret[jss::error_message] = sErrorMsg;

            ret.removeMember(jss::tx_json);
        }
        else
        {

            for (int i = 0; i < ret[jss::tx_json].size(); i++)
            {
                std::string sDest = "";
                if (!TransGBK_UTF8(ret[jss::tx_json][i].asString(), sDest, false))
                {
                    return rpcError(rpcINTERNAL);
                }

                ret[jss::tx_json][i] = sDest;
            }
        }

        return ret;
    }	
} // ripple
