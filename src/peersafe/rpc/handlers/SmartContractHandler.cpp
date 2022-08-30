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
#include <ripple/protocol/jss.h>
#include <ripple/rpc/impl/RPCHelpers.h>
#include <ripple/rpc/handlers/Handlers.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/tx/impl/Transactor.h>
#include <peersafe/schema/Schema.h>
#include <ripple/app/tx/impl/ApplyContext.h>
#include <ripple/app/ledger/OpenLedger.h>
#include <ripple/rpc/impl/RPCHelpers.h>
#include <peersafe/app/misc/SleOps.h>
#include <peersafe/app/misc/ExtVM.h>
#include <peersafe/app/misc/Executive.h>
#include <peersafe/basics/TypeTransform.h>
#include <peersafe/core/Tuning.h>
#include <peersafe/protocol/ContractDefines.h>
#include <peersafe/app/util/Common.h>
#include <iostream> 

namespace ripple {

Json::Value ContractLocalCallResultImpl(Json::Value originJson, TER terResult, std::string exeResult)
{
    Json::Value jvResult;
    std::string detailMethod = originJson[jss::command].asString();
    bool isEthCall = (detailMethod == "eth_call");
    jvResult[jss::request] = originJson;
	try
	{
		if (temUNCERTAIN != terResult)
		{
			if (tesSUCCESS != terResult)
			{
                if (isEthCall)
                    jvResult["result"] = "0x";
                else
                    return RPC::make_error(rpcCTR_EVMCALL_EXCEPTION, exeResult);
			}
			else
			{
                if(isEthCall)
                    jvResult["result"] = exeResult;
                else
                    jvResult[jss::contract_call_result] = exeResult;
			}
		}
	}
	catch (std::exception&)
	{
        auto errMsg = "Exception occurred during JSON handling.";
        if (isEthCall)
            return formatEthError(defaultEthErrorCode, "Exception occurred during JSON handling.");
        else
            return RPC::make_error(rpcINTERNAL, errMsg);
	}
	return jvResult;
}

std::pair<TER, std::string> doEVMCall(ApplyContext& context)
{
	SleOps ops(context);
	auto pInfo = std::make_shared<EnvInfoImpl>(context.view().info().seq, TX_GAS, 
                    context.view().fees().drops_per_byte, context.app.getPreContractFace());
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
	eth::owning_bytes_ref localCallRet = e.takeOutput();
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

Json::Value checkEthJsonFields(Json::Value originJson)
{
    Json::Value ret = Json::Value();

    if (!originJson.isObject())
    {
        ret = RPC::object_field_error(jss::params);
    }

    if (!originJson.isMember("to"))
    {
        ret = RPC::missing_field_error("to");
    }

    if (!originJson.isMember("data"))
    {
        ret = RPC::missing_field_error("data");
    }
    return ret;
}

Json::Value
doContractCall(RPC::JsonContext& context)
{
    Schema& appTemp = context.app;
    std::string errMsgStr("");

    // Json::Value jsonRpcObj = context.params[jss::tx_json];
    Json::Value jsonParams = context.params;
    Json::Value ethParams = jsonParams["realParams"][0u];

    std::string detailMethod = jsonParams[jss::command].asString();
    bool isEthCall = (detailMethod == "eth_call");
    Json::Value checkResult;
    if (isEthCall)
        checkResult = checkEthJsonFields(ethParams);
    else
        checkResult = checkJsonFields(jsonParams);

    if (isRpcError(checkResult))
    {
        if (isEthCall)
            return formatEthError(defaultEthErrorCode, checkResult[jss::error_message].asString());
        return checkResult;
    }

    AccountID accountID;
    if (isEthCall)
    {
        if(ethParams.isMember("from"))
        {
            auto accId = parseHex<AccountID>(ethParams["from"].asString());
            if (!accId)
                return formatEthError(
                    defaultEthErrorCode, rpcDST_ACT_MALFORMED);
            accountID = *accId;
        }
    }
    else
    {
        std::string accountStr = jsonParams[jss::account].asString();
        auto jvAccepted = RPC::accountFromString(accountID, accountStr, true);
        if (jvAccepted)
        {
            return jvAccepted;
        }
    }

    std::shared_ptr<ReadView const> ledger;
    auto result = RPC::lookupLedger(ledger, context);
    if (!ledger)
    {
        if (isEthCall && result.isMember(jss::error_message))
            return formatEthError(
                defaultEthErrorCode, result[jss::error_message].asString());
        return result;
    }


    AccountID contractAddrID;
    if (isEthCall)
    {
        auto optID = parseHex<AccountID>(ethParams["to"].asString());
        if (!optID)
            return formatEthError(defaultEthErrorCode, rpcDST_ACT_MALFORMED);
        contractAddrID = *optID;
        
        if(!ethParams.isMember("from"))
            accountID = *optID;
    }
    else
    {
        std::string ctrAddrStr = jsonParams[jss::contract_address].asString();

        auto jvAcceptedCtrAddr =
            RPC::accountFromString(contractAddrID, ctrAddrStr, true);
        if (jvAcceptedCtrAddr)
        {
            return jvAcceptedCtrAddr;
        }
    }

    if (!isEthCall && !ledger->exists(keylet::account(contractAddrID)))
        return rpcError(rpcACT_NOT_FOUND);

    auto strUnHexRes = strUnHex(
        isEthCall ? ethParams["data"].asString().substr(2)
                  : jsonParams[jss::contract_data].asString());
    if (!strUnHexRes || (*strUnHexRes).empty())
    {
        errMsgStr = "contract_data is invalid.";
        if (isEthCall)
            return formatEthError(defaultEthErrorCode, errMsgStr);
        return RPC::make_error(rpcINVALID_PARAMS, errMsgStr);
    }
    auto contractDataBlob = *strUnHexRes;

    // int64_t txValue = 0;
    STTx contractTx(
        ttCONTRACT,
        [&accountID, &contractAddrID, /*&txValue,*/ &contractDataBlob](
            auto& obj) {
            obj.setAccountID(sfAccount, accountID);
            obj.setAccountID(sfContractAddress, contractAddrID);
            obj.setFieldVL(sfContractData, contractDataBlob);
            obj.setFieldU16(sfContractOpType, QueryCall);
            // obj.setFieldAmount(sfAmount, ZXCAmount(txValue));
        });

    std::shared_ptr<OpenView> openViewTemp =
        std::make_shared<OpenView>(ledger.get());

    ApplyContext applyContext(
        appTemp,
        *openViewTemp,
        contractTx,
        tesSUCCESS,
        FeeUnit64{(std::uint64_t)openViewTemp->fees().base.drops()},
        tapNO_CHECK_SIGN,
        appTemp.journal("ContractLocalCall"));

    auto localCallRet = doEVMCall(applyContext);

    return ContractLocalCallResultImpl(
        jsonParams, localCallRet.first, localCallRet.second);
}

Json::Value
doEstimateGas(RPC::JsonContext& context)
{
    Json::Value jvResult;
    try
    {
        Schema& appTemp = context.app;
        Json::Value jsonParams = context.params;
        Json::Value ethParams = jsonParams["realParams"][0u];
        
        AccountID accountID;
        auto optID = parseHex<AccountID>(ethParams["from"].asString());
        if (!optID)
            return formatEthError(defaultEthErrorCode, rpcDST_ACT_MALFORMED);

        accountID = *optID;
        
        std::shared_ptr<ReadView const> ledger;
        auto result = RPC::lookupLedger(ledger, context);
        if (!ledger)
            return result;
        if (!ledger->exists(keylet::account(accountID)))
            return formatEthError(defaultEthErrorCode, rpcACT_NOT_FOUND);
        
        std::shared_ptr<OpenView> openViewTemp =
            std::make_shared<OpenView>(ledger.get());
        
        AccountID contractAddrID;
        bool isCreation = true;
        bool isCtrAddr = false;
        if(ethParams.isMember("to"))
        {
            auto optID = parseHex<AccountID>(ethParams["to"].asString());
            if (!optID)
                return formatEthError(
                    defaultEthErrorCode, rpcDST_ACT_MALFORMED);

            contractAddrID = *optID;
            isCreation = false;
            
            //check if the new openLedger not created.
            auto ledgerVal = context.app.getLedgerMaster().getValidatedLedger();
            if (ledger->open() && ledger->info().seq <= ledgerVal->info().seq)
                ledger = ledgerVal;
            
            if (ledger->exists(keylet::account(contractAddrID)))
            {
                auto ctrSle = *(ledger->read(keylet::account(contractAddrID)));

                if (ctrSle.isFieldPresent(sfContractCode))
                {
                    isCtrAddr = true;
                }
            }
        }
        
        Blob contractDataBlob;
        if(ethParams.isMember("data") && ethParams["data"].asString() != "")
        {
            auto strUnHexRes = strUnHex(ethParams["data"].asString().substr(2));
            if (!strUnHexRes)
            {
                return formatEthError(
                    defaultEthErrorCode, "contract_data is not in hex");
            }
            contractDataBlob = *strUnHexRes;
            if (contractDataBlob.size() == 0)
            {
                return RPC::invalid_field_error(jss::contract_data);
            }
        }
        else if(isCreation)
        {
            std::int64_t estimatedGas = TX_CREATE_GAS + (std::uint64_t)openViewTemp->fees().base.drops();
            jvResult["result"] = (boost::format("0x%x") % (estimatedGas)).str();
            return jvResult;
        }
        else if(!isCtrAddr)
        {
            std::int64_t estimatedGas = TX_GAS + (std::uint64_t)openViewTemp->fees().base.drops();
            jvResult["result"] = (boost::format("0x%x") % (estimatedGas)).str();
            return jvResult;
        }
        
        std::int64_t value = 0;
        if(ethParams.isMember("value"))
        {
            std::string valueStrHex = ethParams["value"].asString().substr(2);
            value = std::stoll(valueStrHex, 0, 16);
        }
        
        int64_t upperBound = 0;
        if(ethParams.isMember("gas"))
        {
            std::string gasStrHex = ethParams["gas"].asString().substr(2);
            upperBound = std::stoll(gasStrHex, 0, 16);
        }
        if (upperBound == 0 || upperBound == eth::Invalid256 || upperBound > eth::c_maxGasEstimate)
            upperBound = eth::c_maxGasEstimate;
        
        int64_t lowerBound = Executive::baseGasRequired(isCreation, &contractDataBlob);
        
        while (upperBound != lowerBound)
        {
            int64_t mid = (lowerBound + upperBound) / 2;
            STTx contractTx(
                ttCONTRACT,
                [&accountID, &contractAddrID, &value, &contractDataBlob, &mid, &isCreation](
                    auto& obj) {
                    obj.setAccountID(sfAccount, accountID);
                    obj.setAccountID(sfContractAddress, contractAddrID);
                    obj.setFieldVL(sfContractData, contractDataBlob);
                    obj.setFieldU16(sfContractOpType, isCreation ? ContractCreation : MessageCall);
                    if(value != 0) obj.setFieldAmount(sfAmount, ZXCAmount(value));
                    obj.setFieldU32(sfGas, mid);
                });
            ApplyContext applyContext(
                appTemp,
                *openViewTemp,
                contractTx,
                tesSUCCESS,
                FeeUnit64{(std::uint64_t)openViewTemp->fees().base.drops()},
                tapNO_CHECK_SIGN,
                appTemp.journal("EstimateGas"));
            
            SleOps ops(applyContext);
            auto pInfo = std::make_shared<EnvInfoImpl>(applyContext.view().info().seq, mid,
                                                       applyContext.view().fees().drops_per_byte,
                                                       applyContext.app.getPreContractFace());
            Executive e(ops, *pInfo, INITIAL_DEPTH);
            e.initialize();
            
            TER execRet = tesSUCCESS;
            if (!e.execute())
            {
                e.go();
                execRet = e.getException();
            }
            else
                execRet = e.getException();

            std::string errMsg;
            if(execRet != tesSUCCESS)
                errMsg = e.takeOutput().toString();
            
            if (execRet == tesSUCCESS)
            {
                upperBound = upperBound == mid ? lowerBound : mid;
            }
            else if (execRet == tefGAS_INSUFFICIENT ||
                errMsg == "OutOfGas" ||
                errMsg == "BadJumpDestination")
            {
                lowerBound = lowerBound == mid ? upperBound : mid;
            }
            else
            { 
                return formatEthError(defaultEthErrorCode, errMsg);
            }
            
            //JLOG(context.j.warn())
            //    << "eth_estimateGas execRet=" << execRet << ",errMsg="<<errMsg<<",bound=["
            //    << lowerBound << "," << upperBound << "]";
        }
        std::int64_t estimatedGas = upperBound +
            (std::uint64_t)openViewTemp->fees().base.drops() /
                openViewTemp->fees().gas_price;
        jvResult["result"] = (boost::format("0x%x") % (estimatedGas)).str();
        return jvResult;
    }
    catch (...)
    {
        // TODO: Some sort of notification of failure.
        jvResult["result"] = (boost::format("0x%x") % (eth::u256())).str();
        return jvResult;
    }
}



} // ripple
