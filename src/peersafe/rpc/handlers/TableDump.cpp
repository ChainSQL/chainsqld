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
			ret[jss::error] = "error";
            ret[jss::error_message] = "must follow 2 params,in format:\"owner tableName secret\" \"path\".";
			ret.removeMember(jss::tx_json);
            return ret;
        }	

        //1. param similar to synctables
        std::string sNormal = ret[jss::tx_json][uint32_t(0)].asString();        
        //2.path 
        std::string sFullPath = ret[jss::tx_json][uint32_t(1)].asString();

		auto retPair = context.app.getTableSync().StartDumpTable(sNormal, sFullPath, NULL);        

		if(!retPair.first)
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

	Json::Value doTableDumpStop(RPC::Context& context)
	{
		Json::Value ret(context.params);

		if (ret[jss::tx_json].size() != 2)
		{
			ret[jss::error] = "error.";
			ret[jss::error_message] = "must follow 2 params,in format:owner tableName.";
			ret.removeMember(jss::tx_json);
			return ret;
		}

		std::string owner = ret[jss::tx_json][0U].asString();
		std::string tableName = ret[jss::tx_json][1U].asString();
        auto pOwnerID = ripple::parseBase58<AccountID>(owner);

        if (!pOwnerID)
        {
            ret[jss::error] = "error.";
            ret[jss::error_message] = "para error, owner is invalid.";
            ret.removeMember(jss::tx_json);
            return ret;
        }

		AccountID ownerID(*pOwnerID);
		auto retPair = context.app.getTableSync().StopDumpTable(ownerID, tableName);
        
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
