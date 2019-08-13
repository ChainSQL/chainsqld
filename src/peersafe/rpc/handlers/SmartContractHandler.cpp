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

#include <ripple/json/json_value.h>
#include <ripple/net/RPCErr.h>
#include <ripple/protocol/JsonFields.h>
#include <ripple/rpc/impl/RPCHelpers.h>
#include <ripple/rpc/handlers/Handlers.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/main/Application.h>
#include <ripple/app/tx/impl/ApplyContext.h>
#include <ripple/app/ledger/OpenLedger.h>
#include <peersafe/app/misc/SleOps.h>
#include <peersafe/app/misc/ExtVM.h>
#include <peersafe/app/misc/Executive.h>
#include <peersafe/basics/TypeTransform.h>
#include <peersafe/core/Tuning.h>
#include <iostream> 

namespace ripple {

Json::Value ContractLocalCallResultImpl(Json::Value originJson, TER terResult, std::string exeResult)
{
	Json::Value jvResult;
	try
	{
		jvResult[jss::request] = originJson;

		if (temUNCERTAIN != terResult)
		{
			if (tesSUCCESS != terResult)
			{
				return RPC::make_error(rpcCTR_EVMCALL_EXCEPTION, exeResult);
				//jvResult[jss::error] = exeResult;
			}
			else
			{
				jvResult[jss::contract_call_result] = exeResult;
			}
		}
	}
	catch (std::exception&)
	{
		jvResult = RPC::make_error(rpcINTERNAL,
			"Exception occurred during JSON handling.");
	}
	return jvResult;
}

std::pair<TER, std::string> doEVMCall(ApplyContext& context)
{
	SleOps ops(context);
	auto pInfo = std::make_shared<EnvInfoImpl>(context.view().info().seq, TX_GAS, context.view().fees().drops_per_byte);
	Executive e(ops, *pInfo, INITIAL_DEPTH);
	e.initialize();
	auto tx = context.tx;
	AccountID contractAddr = tx.getAccountID(sfContractAddress);
	AccountID senderAddr = tx.getAccountID(sfAccount);
	uint256 value = uint256();
	uint256 gasPrice = uint256();
	Blob contractData = tx.getFieldVL(sfContractData);
	bool callResult = !(e.call(contractAddr, senderAddr, value, gasPrice, &contractData, maxInt64/2));
	if (callResult)
	{
		e.go();
	}

	TER terResult = e.getException();
	owning_bytes_ref localCallRet = e.takeOutput();;
	std::string localCallRetStr = "";
	if (terResult == tesSUCCESS)
		localCallRetStr = "0x" + strHex(localCallRet.takeBytes());
	else
		localCallRetStr = localCallRet.toString();

	return std::make_pair(terResult, localCallRetStr);	
}

Json::Value checkJsonFields(Json::Value originJson)
{
	Json::Value ret = Json::Value();

	if (!originJson.isObject())
	{
		ret = RPC::object_field_error(jss::params);
	}

	if (!originJson.isMember(jss::account))
	{
		ret = RPC::missing_field_error(jss::account);
	}

	if (!originJson.isMember(jss::contract_address))
	{
		ret = RPC::missing_field_error(jss::contract_address);
	}

	if (!originJson.isMember(jss::contract_data))
	{
		ret = RPC::missing_field_error(jss::contract_data);
	}
	return ret;
}

Json::Value doContractCall(RPC::Context& context)
{
	Application& appTemp = context.app;
	std::string errMsgStr("");

	//Json::Value jsonRpcObj = context.params[jss::tx_json];
	Json::Value jsonParams = context.params;

	auto checkResult = checkJsonFields(jsonParams);
	if (isRpcError(checkResult))
		return checkResult;

	std::string accountStr = jsonParams[jss::account].asString();
	AccountID accountID;
	auto jvAccepted = RPC::accountFromString(accountID, accountStr, true);
	if (jvAccepted)
	{
		return jvAccepted;
	}
	std::shared_ptr<ReadView const> ledger;
	auto result = RPC::lookupLedger(ledger, context);
	if (!ledger)
		return result;
	if (!ledger->exists(keylet::account(accountID)))
		return rpcError(rpcACT_NOT_FOUND);

	std::string ctrAddrStr = jsonParams[jss::contract_address].asString();
	AccountID contractAddrID;
	auto jvAcceptedCtrAddr = RPC::accountFromString(contractAddrID, ctrAddrStr, true);
	if (jvAcceptedCtrAddr)
	{
		return jvAcceptedCtrAddr;
	}
	if (!ledger->exists(keylet::account(contractAddrID)))
		return rpcError(rpcACT_NOT_FOUND);
	
	auto strUnHexRes = strUnHex(jsonParams[jss::contract_data].asString());
	if (!strUnHexRes.second)
	{
		errMsgStr = "contract_data is not in hex";
		return RPC::make_error(rpcINVALID_PARAMS, errMsgStr);
	}
	Blob contractDataBlob = strUnHexRes.first;
	if (contractDataBlob.size() == 0)
	{
		return RPC::invalid_field_error(jss::contract_data);
	}
	//int64_t txValue = 0;
	STTx contractTx(ttCONTRACT,
		[&accountID, &contractAddrID, /*&txValue,*/ &contractDataBlob](auto& obj)
	{
		obj.setAccountID(sfAccount, accountID);
		obj.setAccountID(sfContractAddress, contractAddrID);
		obj.setFieldVL(sfContractData, contractDataBlob);
		//obj.setFieldAmount(sfAmount, ZXCAmount(txValue));
	});
	OpenLedger& openLedgerTemp = appTemp.openLedger();
	//OpenView& openViewTemp = const_cast<OpenView&>(*openLedgerTemp.current());
	std::shared_ptr<OpenView> openViewTemp = std::make_shared<OpenView>(*openLedgerTemp.current());
	ApplyContext applyContext(appTemp, *openViewTemp, contractTx, tesSUCCESS, openViewTemp->fees().base,
		tapNO_CHECK_SIGN, appTemp.journal("ContractLocalCall"));

	auto localCallRet = doEVMCall(applyContext);

	return ContractLocalCallResultImpl(jsonParams, localCallRet.first, localCallRet.second);
}

} // ripple