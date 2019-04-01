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
#include <ripple/rpc/impl/RPCHelpers.h>
#include <peersafe/app/table/TableSync.h>
#include <peersafe/basics/characterUtilities.h>

namespace ripple {

    extern bool isDirValid(std::string dir)
    {
        bool isValid = false;
#ifdef _WIN32
        struct _stat fileStat;
        if ((_stat(dir.c_str(), &fileStat) == 0) && (fileStat.st_mode & _S_IFDIR))
        {
            isValid = true;
        }
#else
        struct stat fileStat;
        if ((stat(dir.c_str(), &fileStat) == 0) && S_ISDIR(fileStat.st_mode))
        {
            isValid = true;
        }
#endif   
        return isValid;
    }

	Json::Value doTableDump(RPC::Context& context)
	{
		Json::Value ret(context.params);
       
        if (ret[jss::tx_json].size() != 2)
        {
			std::string errMsg = "must follow 2 params,in format:\"owner tableName secret\" \"path\".";
			ret.removeMember(jss::tx_json);
			return RPC::make_error(rpcINVALID_PARAMS, errMsg);
        }	

        //1. param similar to synctables
        std::string sNormal = ret[jss::tx_json][uint32_t(0)].asString();        
        //2.path 
        std::string sFullPath = ret[jss::tx_json][uint32_t(1)].asString();

		auto retPair = context.app.getTableSync().StartDumpTable(sNormal, sFullPath, NULL);        

		if(!retPair.first)
		{
            std::string sErrorMsg;
            TransGBK_UTF8(retPair.second, sErrorMsg, false);
			ret.removeMember(jss::tx_json);
			return RPC::make_error(rpcDUMP_GENERAL_ERR, sErrorMsg);
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

    Json::Value parseParam(RPC::Context& context, AccountID & ownerID, std::string &tableName)
    {
        Json::Value ret(context.params);

        if (ret[jss::tx_json].size() != 2)
        {
            std::string errMsg = "must follow 2 params,in format:owner tableName.";
            ret.removeMember(jss::tx_json);
			return RPC::make_error(rpcINVALID_PARAMS, errMsg);
        }

        std::string owner = ret[jss::tx_json][0U].asString();
        auto jvAccepted = RPC::accountFromString(ownerID, owner, true);
        if (jvAccepted)
        {
            return jvAccepted;
        }
        std::shared_ptr<ReadView const> ledger;
        auto result = RPC::lookupLedger(ledger, context);
        if (!ledger)
            return result;
        if (!ledger->exists(keylet::account(ownerID)))
            return rpcError(rpcACT_NOT_FOUND);

        tableName = ret[jss::tx_json][1U].asString();

        return ret;
    }

	Json::Value doTableDumpStop(RPC::Context& context)
	{
		AccountID ownerID;
		std::string tableName;
        Json::Value ret = parseParam(context, ownerID, tableName);
        if (isRpcError(ret))   return ret;

		auto retPair = context.app.getTableSync().StopDumpTable(ownerID, tableName);
        
		if (!retPair.first)
		{
            std::string sErrorMsg;
            TransGBK_UTF8(retPair.second, sErrorMsg, false);
			RPC::inject_error(rpcDUMPSTOP_GENERAL_ERR, sErrorMsg, ret);
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

    Json::Value getDumpCurPos(RPC::Context& context)
    {
        AccountID ownerID;
        std::string tableName;
        Json::Value ret = parseParam(context, ownerID, tableName);
        if (isRpcError(ret))   return ret;

        TableSyncItem::taskInfo info;
        bool bRet = context.app.getTableSync().GetCurrentDumpPos(ownerID, tableName, info);
        
        if (bRet)
        {
            ret[jss::start]     = info.uStartPos;
            ret[jss::stop]      = info.uStopPos;
            ret[jss::current]   = info.uCurPos;
        }
        else
        {
            std::string errMsg = "task has already completed.";
            ret.removeMember(jss::tx_json);
            return RPC::make_error(rpcFIELD_CONTENT_EMPTY, errMsg);
        }

        return ret;
    }
} // ripple
