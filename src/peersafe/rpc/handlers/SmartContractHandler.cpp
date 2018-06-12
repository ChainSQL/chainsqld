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
#include <ripple/rpc/Role.h>
#include <ripple/rpc/handlers/Handlers.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/json/json_reader.h>
#include <ripple/protocol/SecretKey.h>
#include <ripple/app/main/Application.h>
#include <ripple/app/tx/impl/ApplyContext.h>
#include <ripple/app/ledger/OpenLedger.h>
#include <peersafe/app/misc/SleOps.h>
#include <peersafe/app/misc/ExtVM.h>
#include <peersafe/app/misc/Executive.h>
#include <peersafe/basics/TypeTransform.h>
#include <iostream> 
#include <fstream>

namespace ripple {

Json::Value ContractLocalCallResultImpl(Json::Value originJson, TER terResult, std::string exeResult)
{
	Json::Value jvResult;
	try
	{
		jvResult[jss::tx_json] = originJson;

		if (temUNCERTAIN != terResult)
		{
			std::string sToken;
			std::string sHuman;

			transResultInfo(terResult, sToken, sHuman);

			jvResult[jss::engine_result] = sToken;
			jvResult[jss::engine_result_code] = terResult;
			jvResult[jss::engine_result_message] = sHuman;
			jvResult[jss::contract_local_call_result] = exeResult;
		}
	}
	catch (std::exception&)
	{
		jvResult = RPC::make_error(rpcINTERNAL,
			"Exception occurred during JSON handling.");
	}
	return jvResult;
}

Json::Value contractLocalCallErrResultImpl(Json::Value originJson, std::string errMsgStr)
{
	Json::Value jvResult;
	
	jvResult[jss::tx_json] = originJson;
	jvResult[jss::error_message] = errMsgStr;
	jvResult[jss::error] = "error";

	return jvResult;
}

std::pair<TER, std::string> doEVMCall(ApplyContext& context)
{
	SleOps ops(context);
	auto pInfo = std::make_shared<EnvInfoImpl>(context.view().info().seq, 210000);
	Executive e(ops, *pInfo, 1);
	e.initialize();
	auto tx = context.tx;
	evmc_address contractAddr = toEvmC(tx.getAccountID(sfContractAddress));
	evmc_address senderAddr = toEvmC(tx.getAccountID(sfAccount));
	evmc_uint256be value = toEvmC(uint256());
	evmc_uint256be gasPrice = toEvmC(uint256());
	bytes contractData = tx.getFieldVL(sfContractData);
	bool callResult = !(e.call(contractAddr, senderAddr, value, gasPrice, bytesConstRef(&contractData), 5000000));
	if (callResult)
	{
		e.go();
	}

	TER terResult = e.getException();
	owning_bytes_ref localCallRet = e.takeOutput();
	std::string localCallRetStr = strHex(localCallRet.takeBytes());

	return std::make_pair(terResult, localCallRetStr);
}

std::pair<Json::Value, bool> checkJsonFields(Json::Value originJson)
{
	std::pair<Json::Value, bool> ret;
	ret.second = false;
	if (!originJson.isObject())
	{
		ret.first = RPC::object_field_error(jss::tx_json);
		return ret;
	}

	if (!originJson.isMember(jss::TransactionType))
	{
		ret.first = RPC::missing_field_error("tx_json.TransactionType");
		return ret;
	}

	if (!originJson.isMember(jss::Account))
	{
		ret.first = RPC::make_error(rpcSRC_ACT_MISSING,
			RPC::missing_field_message("tx_json.Account"));
		return ret;
	}

	if (!originJson.isMember(jss::ContractAddress))
	{
		ret.first = RPC::make_error(rpcCTR_ACT_MISSING,
			RPC::missing_field_message("tx_json.ContractAddress"));
		return ret;
	}

	if (!originJson.isMember(jss::ContractData))
	{
		ret.first = RPC::missing_field_message("tx_json.ContractData");
		return ret;
	}
	ret.first = Json::Value();
	ret.second = true;
	return ret;
}

Json::Value doCtractLocalCall(RPC::Context& context)
{
	Application& appTemp = context.app;
	std::string errMsgStr("");

	Json::Value jsonRpcObj = context.params[jss::tx_json];

	auto checkResult = checkJsonFields(jsonRpcObj);
	if (!checkResult.second)
		return checkResult.first;
	auto const srcAddressID = parseBase58<AccountID>(jsonRpcObj[jss::Account].asString());
	if (srcAddressID == boost::none)
	{
		errMsgStr = "Missing Account field";
		return contractLocalCallErrResultImpl(jsonRpcObj, errMsgStr);
	}
	auto const contractAddrID = parseBase58<AccountID>(jsonRpcObj[jss::ContractAddress].asString());
	if (contractAddrID == boost::none)
	{
		errMsgStr = "Missing ContractAddress field";
		return contractLocalCallErrResultImpl(jsonRpcObj, errMsgStr);
	}
	/*auto const contractDataStr = jsonRpcObj[jss::ContractData].asString();
	Blob contractDataBlob;
	contractDataBlob.resize(contractDataStr.size());
	contractDataBlob.assign(contractDataStr.begin(), contractDataStr.end());*/
	Blob contractDataBlob = strUnHex(jsonRpcObj[jss::ContractData].asString()).first;
	if (contractDataBlob.size() == 0)
	{
		errMsgStr = "Missing ContractData field";
		return contractLocalCallErrResultImpl(jsonRpcObj, errMsgStr);
	}
	//int64_t txValue = 0;
	STTx paymentTx(ttCONTRACT,
		[&srcAddressID, &contractAddrID, /*&txValue,*/ &contractDataBlob](auto& obj)
	{
		obj.setAccountID(sfAccount, *srcAddressID);
		obj.setAccountID(sfContractAddress, *contractAddrID);
		obj.setFieldVL(sfContractData, contractDataBlob);
		//obj.setFieldAmount(sfAmount, ZXCAmount(txValue));
	});
	OpenLedger& openLedgerTemp = appTemp.openLedger();
	OpenView& openViewTemp = const_cast<OpenView&>(*openLedgerTemp.current());
	ApplyContext applyContext(appTemp, openViewTemp, paymentTx, tesSUCCESS, 10,
		tapNO_CHECK_SIGN, appTemp.journal("ContractLocalCall"));

	auto localCallRet = doEVMCall(applyContext);
	int value = 0;

	return ContractLocalCallResultImpl(jsonRpcObj, localCallRet.first, localCallRet.second);
}

} // ripple