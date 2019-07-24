//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012-2014 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include <BeastConfig.h>
#include <ripple/net/RPCErr.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/JsonFields.h>
#include <ripple/rpc/Context.h>
#include <ripple/rpc/impl/RPCHelpers.h>
#include <peersafe/gmencrypt/hardencrypt/gmCheck.h>


namespace ripple {

// FIXME: This leaks RPCSub objects for JSON-RPC.  Shouldn't matter for anyone
// sane.
Json::Value doCreateRandom(RPC::Context& context)
{

    InfoSub::pointer ispSub;
    Json::Value jvResult (Json::objectValue);

	GMCheck* gmCheckObj = GMCheck::getInstance();
	gmCheckObj->generateRandom2File();

    //Json::Value& tx_json(context.params["tx_json"]);

    return jvResult;
}

Json::Value doCryptData(RPC::Context& context)
{
    InfoSub::pointer ispSub;
    Json::Value jvResult(Json::objectValue);
	std::string errMsgStr("");
	int gmAlgType, dataSetCount, plainDataLen;

	Json::Value jsonParams = context.params;
	//jsonParams[jss::alg_type] =
	if (!jsonParams.isMember(jss::alg_type))
	{
		errMsgStr = "field alg_type";
		jvResult = RPC::make_error(rpcINVALID_PARAMS, errMsgStr);
		return jvResult;
	}
	else
	{
		gmAlgType = jsonParams[jss::alg_type].asInt();
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
