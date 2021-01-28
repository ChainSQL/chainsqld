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

#include <ripple/net/RPCErr.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/jss.h>
#include <ripple/rpc/Context.h>
#include <ripple/rpc/impl/RPCHelpers.h>
#include <peersafe/gmencrypt/GmCheck.h>


namespace ripple {

// FIXME: This leaks RPCSub objects for JSON-RPC.  Shouldn't matter for anyone
// sane.
Json::Value doCreateRandom(RPC::JsonContext& context)
{

    InfoSub::pointer ispSub;
    Json::Value jvResult (Json::objectValue);

	GMCheck* gmCheckObj = GMCheck::getInstance();
	gmCheckObj->generateRandom2File();

    //Json::Value& tx_json(context.params["tx_json"]);

    return jvResult;
}

Json::Value doCryptData(RPC::JsonContext& context)
{
    InfoSub::pointer ispSub;
    Json::Value jvResult(Json::objectValue);
	std::string errMsgStr("");
	int gmAlgType, dataSetCount, plainDataLen;

	Json::Value jsonParams = context.params;
	if (!jsonParams.isMember(jss::gm_alg_type))
	{
		errMsgStr = "field gm_alg_type";
		jvResult = RPC::make_error(rpcINVALID_PARAMS, errMsgStr);
		return jvResult;
	}
	else
	{
		gmAlgType = jsonParams[jss::gm_alg_type].asInt();
	}

	if (!jsonParams.isMember(jss::data_set_count))
	{
		errMsgStr = "field data_set_count";
		jvResult = RPC::make_error(rpcINVALID_PARAMS, errMsgStr);
		return jvResult;
	}
	else
	{
		dataSetCount = jsonParams[jss::data_set_count].asInt();
	}

	if (gmAlgType != GMCheck::SM2KEY)
	{
		if (!jsonParams.isMember(jss::plain_data_len))
		{
			errMsgStr = "field plain_data_len";
			jvResult = RPC::make_error(rpcINVALID_PARAMS, errMsgStr);
			return jvResult;
		}
		else
		{
			plainDataLen = jsonParams[jss::plain_data_len].asInt();
		}
	}
	else
	{
		plainDataLen = 0;
	}
    //Json::Value& tx_json(context.params["tx_json"]);
	GMCheck* gmCheckObj = GMCheck::getInstance();
	std::pair<bool, std::string> getRet = gmCheckObj->getAlgTypeData(gmAlgType, dataSetCount, plainDataLen);
	if (!getRet.first)
	{
		jvResult = RPC::make_error(rpcINVALID_PARAMS, getRet.second);
	}

	return jvResult;
}

} // ripple
