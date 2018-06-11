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
//#include <ripple/rpc/Context.h>
//#include <ripple/rpc/impl/TransactionSign.cpp>
#include <ripple/rpc/Role.h>
#include <ripple/rpc/handlers/Handlers.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/json/json_reader.h>
#include <ripple/protocol/SecretKey.h>
#include <ripple/app/main/Application.h>
#include <ripple/app/tx/impl/ApplyContext.h>
#include <ripple/app/ledger/OpenLedger.h>
#include <peersafe/app/tx/SmartContract.h>
#include <iostream> 
#include <fstream>

namespace ripple {

Json::Value ContractLocalCallResultImpl(Json::Value originJson, TER terResult, int value)
{
	Json::Value jvResult;
	try
	{
		jvResult[jss::tx_json] = originJson;
		//jvResult[jss::tx_blob] = strHex(tpTrans->getSTransaction()->getSerializer().peekData());

		if (temUNCERTAIN != terResult)
		{
			std::string sToken;
			std::string sHuman;

			transResultInfo(terResult, sToken, sHuman);

			jvResult[jss::engine_result] = sToken;
			jvResult[jss::engine_result_code] = terResult;
			jvResult[jss::engine_result_message] = sHuman;
			jvResult[jss::contract_local_call_result] = value;
		}
	}
	catch (std::exception&)
	{
		jvResult = RPC::make_error(rpcINTERNAL,
			"Exception occurred during JSON handling.");
	}
	return jvResult;
}

Json::Value doCtractLocalCall(RPC::Context& context)
{
	//context.app
	Application& appTemp = context.app;

	Json::Value jsonRpcObj = context.params;
	auto const srcAddressID = parseBase58<AccountID>(jsonRpcObj[jss::Account].asString());
	int64_t txValue = 0;
	STTx paymentTx(ttPAYMENT,[](auto& obj){});
	OpenLedger& openLedgerTemp = appTemp.openLedger();
	OpenView& openViewTemp = const_cast<OpenView&>(*openLedgerTemp.current());
	ApplyContext applyContext(appTemp, openViewTemp, paymentTx, tesSUCCESS, 10,
		tapNO_CHECK_SIGN, appTemp.journal("ContractLocalCall"));

	SmartContract SCObj(applyContext);
	auto localCallRet = SCObj.doLocalCall();
	int value = 0;

	return ContractLocalCallResultImpl(jsonRpcObj, localCallRet.first, value);
}

} // ripple