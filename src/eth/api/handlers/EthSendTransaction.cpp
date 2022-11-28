//
//  EthSendTransaction.cpp
//  chainsqld
//
//  Created by luleigreat on 2022/10/27
//

#include <ripple/app/misc/Transaction.h>
#include <ripple/app/tx/apply.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/json/json_reader.h>
#include <ripple/json/json_value.h>
#include <ripple/net/RPCErr.h>
#include <ripple/protocol/jss.h>
#include <ripple/rpc/handlers/Handlers.h>
#include <ripple/rpc/impl/RPCHelpers.h>
#include <ripple/app/ledger/OpenLedger.h>
#include <peersafe/app/tx/impl/Tuning.h>
#include <eth/api/utils/Helpers.h>
#include <peersafe/protocol/STETx.h>
#include <eth/api/utils/TransactionSkeleton.h>

namespace ripple {

void
setTransactionDefaults(TransactionSkeleton& _t, RPC::JsonContext& context)
{
    const u256 defaultTransactionGas = 90000;
    if (_t.nonce == Invalid256)
    {
        AccountID accountID;
        memcpy(accountID.data(),_t.from.data(), 20);
        auto sle = context.app.openLedger().current()->read(
            keylet::account(accountID));
        if (sle)
            _t.nonce = sle->getFieldU32(sfSequence);
        else
            _t.nonce = 0;
    }
    if (_t.gas == Invalid256)
        _t.gas = defaultTransactionGas;
    if (_t.gasPrice == Invalid256)
        _t.gasPrice = context.app.openLedger().current()->fees().gas_price;
}

std::pair<std::shared_ptr<STETx>,Json::Value>
signTransaction(RPC::JsonContext& context)
{
    std::shared_ptr<STETx> pTx = nullptr;
    Json::Value jvResult;

    if (context.role != Role::ADMIN && !context.app.config().canSign())
    {
        jvResult = formatEthError(
            ethERROR_DEFAULT, "Signing is not supported by this server.");
        return std::make_pair(pTx, jvResult);
    }
    if (!context.app.config().ETH_DEFAULT_ACCOUNT_PRIVATE)
    {
        jvResult = formatEthError(
            ethERROR_DEFAULT, "No singing account private configured.");
        return std::make_pair(pTx, jvResult);
    }
        

    Json::Value jsonParams = context.params;
    Json::Value ethParams = jsonParams["realParams"][0u];

    try
    {
        TransactionSkeleton ts = toTransactionSkeleton(ethParams);
        std::string secret = *context.app.config().ETH_DEFAULT_ACCOUNT_PRIVATE;
        if (!ts.from)
        {
            jvResult = formatEthError(ethERROR_DEFAULT, "Invalid 'from'");
            return std::make_pair(pTx, jvResult);
        }
        if (ts.from != addressFromSecret(secret))
        {
            jvResult = formatEthError(
                ethERROR_DEFAULT, "'from' not match configured private-key.");
            return std::make_pair(pTx, jvResult);
        }
            
        setTransactionDefaults(ts, context);

        auto chainID = getChainID(context.app.openLedger().current());
        auto lastLedgerSeq = context.ledgerMaster.getCurrentLedgerIndex() +
            LAST_LEDGER_SEQ_OFFSET;
        // Construct STETx
        pTx = std::make_shared<STETx>(ts, secret,chainID,lastLedgerSeq);
        return std::make_pair(pTx, jvResult);
    }
    catch (std::exception& e)
    {
        jvResult = formatEthError(ethERROR_DEFAULT,e.what());
        return std::make_pair(pTx, jvResult);
    }
}

Json::Value
processEtxTransaction(
    RPC::JsonContext& context,
    std::shared_ptr<STETx> stpTrans)
{
    Json::Value jvResult;
    // Check validity
    auto [validity, reason] = checkValidity(
        context.app,
        context.app.getHashRouter(),
        *stpTrans,
        context.ledgerMaster.getCurrentLedger()->rules(),
        context.app.config());
    if (validity != Validity::Valid)
    {
        return formatEthError(ethERROR_DEFAULT, "Check validity failed.");
    }

    std::string reason2;
    auto tpTrans =
        std::make_shared<Transaction>(stpTrans, reason2, context.app);
    if (tpTrans->getStatus() != NEW)
    {
        jvResult[jss::result] = "0x00";
        return jvResult;
    }

    context.netOps.processTransaction(
        tpTrans, isUnlimited(context.role), true, NetworkOPs::FailHard::no);

    std::string txIdStr = to_string(stpTrans->getTransactionID());
    jvResult[jss::result] = "0x" + toLowerStr(txIdStr);
    return jvResult;
}

Json::Value
doEthSignTransaction(RPC::JsonContext& context)
{
    auto pair = signTransaction(context);
    if (pair.second.isMember(jss::error))
        return pair.second;
    Json::Value jvResult;
    auto rlpData = pair.first->getRlpData();
    jvResult[jss::result] = "0x" + strHex(strCopy(rlpData));
    return jvResult;
}



Json::Value
doEthSendTransaction(RPC::JsonContext& context)
{
    Json::Value jvResult;
    auto pair = signTransaction(context);
    if (pair.second.isMember(jss::error))
        return pair.second;

    try
    {
        jvResult = processEtxTransaction(context,pair.first);
    }
    catch (std::exception& e)
    {
        jvResult = formatEthError(ethERROR_DEFAULT, e.what());
    }
    return jvResult;
}

Json::Value
doEthSendRawTransaction(RPC::JsonContext& context)
{
    Json::Value jvResult;
    try
    {
        auto ret =
            strUnHex(context.params["realParams"][0u].asString().substr(2));

        // 500KB
        if (ret->size() > RPC::Tuning::max_txn_size)
        {
            return formatEthError(ethERROR_DEFAULT, rpcTXN_BIGGER_THAN_MAXSIZE);
        }

        if (!ret || !ret->size())
            return formatEthError(ethERROR_DEFAULT, rpcINVALID_PARAMS);

        auto lastLedgerSeq = context.ledgerMaster.getCurrentLedgerIndex() +
            LAST_LEDGER_SEQ_OFFSET;
        // Construct STETx
        auto stpTrans = std::make_shared<STETx>(makeSlice(*ret), lastLedgerSeq);

        jvResult = processEtxTransaction(context, stpTrans);
    }
    catch (std::exception& e)
    {
        jvResult = formatEthError(ethERROR_DEFAULT, e.what());
    }
    return jvResult;
}
}  // namespace ripple