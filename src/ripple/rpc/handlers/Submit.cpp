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

#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/main/Application.h>
#include <ripple/app/misc/HashRouter.h>
#include <ripple/app/misc/Transaction.h>
#include <ripple/app/tx/apply.h>
#include <ripple/net/RPCErr.h>
#include <ripple/protocol/AccountID.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/resource/Fees.h>
#include <ripple/rpc/Context.h>
#include <ripple/rpc/GRPCHandlers.h>
#include <ripple/rpc/impl/GRPCHelpers.h>
#include <ripple/rpc/impl/RPCHelpers.h>
#include <ripple/rpc/impl/TransactionSign.h>
#include <ripple/rpc/impl/Tuning.h>
#include <peersafe/app/misc/StateManager.h>
#include <peersafe/schema/Schema.h>

namespace ripple {

static NetworkOPs::FailHard
getFailHard(RPC::JsonContext const& context)
{
    return NetworkOPs::doFailHard(
        context.params.isMember("fail_hard") &&
        context.params["fail_hard"].asBool());
}

// {
//   tx_json: <object>,
//   secret: <secret>
// }
Json::Value
doSubmit(RPC::JsonContext& context)
{
    context.loadType = Resource::feeMediumBurdenRPC;

    if (!context.params.isMember(jss::tx_blob))
    {
        if (!context.params.isMember(jss::tx_json))
            return RPC::missing_field_error(jss::tx_json);

        bool bSelfContainedSeq = false;
        Json::Value const& tx_json(context.params[jss::tx_json]);
        if (tx_json.isMember(jss::Sequence))
            bSelfContainedSeq = true;

        auto const failType = getFailHard(context);

        if (context.role != Role::ADMIN && !context.app.config().canSign())
            return RPC::make_error(
                rpcNOT_SUPPORTED, "Signing is not supported by this server.");

        auto ret = RPC::transactionSubmit(
            context.params,
            failType,
            context.role,
            context.ledgerMaster.getValidatedLedgerAge(),
            context.app,
            RPC::getProcessTxnFn(context.netOps));

        // ret[jss::deprecated] =
        //     "Signing support in the 'submit' command has been "
        //     "deprecated and will be removed in a future version "
        //     "of the server. Please migrate to a standalone "
        //     "signing tool.";

        if ((RPC::contains_error(ret) ||
             ret[jss::engine_result_code] == temBAD_PUT) &&
            !bSelfContainedSeq && tx_json.isMember(jss::Sequence))
        {
            context.j.info() << "sign-and-submit mode, return "
                             << RPC::contains_error(ret)
                ? "non engine errors"
                : "engine temBAD_PUT error";

            auto const srcAccountID =
                parseBase58<AccountID>(tx_json[jss::Account].asString());
            if (srcAccountID)
                context.app.getStateManager().addFailedSeq(
                    *srcAccountID, tx_json[jss::Sequence].asUInt());
        }

        return ret;
    }

    Json::Value jvResult;

    auto ret = strUnHex(context.params[jss::tx_blob].asString());

	// 500KB
	if (ret->size() > RPC::Tuning::max_txn_size) {
		return rpcError(rpcTXN_BIGGER_THAN_MAXSIZE);
	}

    if (!ret || !ret->size ())
        return rpcError (rpcINVALID_PARAMS);

    SerialIter sitTrans(makeSlice(*ret));

    std::shared_ptr<STTx const> stpTrans;

    try
    {
        stpTrans = std::make_shared<STTx const>(std::ref(sitTrans));
    }
    catch (std::exception& e)
    {    
        jvResult[jss::error]         = "invalidTransaction";
        jvResult[jss::error_code]    = rpcINVALID_TRANSACTION;  
        jvResult[jss::error_message] = e.what();
        jvResult[jss::request]       = context.params[jss::tx_blob];
        return jvResult;
    }

    {
		//check publicKey type
        boost::optional<KeyType> keyType;
        if (!stpTrans->getFieldVL(sfSigningPubKey).empty())
        {
            keyType = publicKeyType(makeSlice(stpTrans->getFieldVL(sfSigningPubKey)));
        }
        else if (stpTrans->isFieldPresent(sfSigners) && stpTrans->getFieldArray(sfSigners).size() > 0)
        {
            auto& signers = stpTrans->getFieldArray(sfSigners);
            if (signers[0].isFieldPresent(sfSigningPubKey))
            {
                keyType = publicKeyType(
                    makeSlice(signers[0].getFieldVL(sfSigningPubKey)));
            }            
        }

		if (!keyType)
        {
            jvResult[jss::error] = "invalidTransaction";
			jvResult[jss::error_message] = "fails local checks: get signingPublicKey type failed";
            return jvResult;
        }
		if (*keyType != CommonKey::chainAlgTypeG)
		{
			jvResult[jss::error] = "invalidTransaction";
			jvResult[jss::error_message] = "fails local checks: signingPublicKey type is " \
				"not consistent with the node.";
            return jvResult;
		}

        if (!context.app.checkSigs())
            forceValidity(
                context.app.getHashRouter(),
                stpTrans->getTransactionID(),
                Validity::SigGoodOnly);
        auto [validity, reason] = checkValidity(
			context.app,
            context.app.getHashRouter(),
            *stpTrans,
            context.ledgerMaster.getCurrentLedger()->rules(),
            context.app.config());
        if (validity != Validity::Valid)
        {
            jvResult[jss::error]         = "invalidTransaction";
            jvResult[jss::error_code]    = rpcINVALID_TRANSACTION;
            jvResult[jss::error_message] = "The validity verification of the transaction failed: " + reason;

            return jvResult;
        }
		// JLOG(context.j.custom()) << "Signature and Acount check successfully!";
    }

    std::string reason;
    auto tpTrans = std::make_shared<Transaction>(stpTrans, reason, context.app);
    if (tpTrans->getStatus() != NEW)
    {
        jvResult[jss::error]         = "invalidTransaction";
        jvResult[jss::error_code]    = rpcINVALID_TRANSACTION;
        jvResult[jss::error_message] = "Construction transaction failed: " + reason;
        jvResult[jss::request]       = stpTrans->getFullText();
        return jvResult;
    }

    try
    {
        auto const failType = getFailHard(context);

        context.netOps.processTransaction(
            tpTrans, isUnlimited(context.role), true, failType);
    }
    catch (std::exception& e)
    {
        jvResult[jss::error]         = "internalSubmit";
        jvResult[jss::error_code]    = rpcINTERNAL;
        jvResult[jss::error_message] = e.what();
        return jvResult;
    }

    try
    {
        jvResult[jss::tx_json] = tpTrans->getJson (JsonOptions::none);
        jvResult[jss::tx_blob] = strHex (
            tpTrans->getSTransaction ()->getSerializer ().peekData ());

		STer result = tpTrans->getResult();
        if (temUNCERTAIN != result.ter)
        {
            std::string sToken;
            std::string sHuman;

            transResultInfo(tpTrans->getResult(), sToken, sHuman);

            jvResult[jss::engine_result] = sToken;
            jvResult[jss::engine_result_code] = result.ter;
            jvResult[jss::engine_result_message] = sHuman;
			if (!result.msg.empty())
				jvResult[jss::engine_result_message_detail] = result.msg;

            auto const submitResult = tpTrans->getSubmitResult();

            jvResult[jss::accepted] = submitResult.any();
            jvResult[jss::applied] = submitResult.applied;
            jvResult[jss::broadcast] = submitResult.broadcast;
            jvResult[jss::queued] = submitResult.queued;
            jvResult[jss::kept] = submitResult.kept;

            if (auto currentLedgerState = tpTrans->getCurrentLedgerState())
            {
                jvResult[jss::account_sequence_next] =
                    safe_cast<Json::Value::UInt>(
                        currentLedgerState->accountSeqNext);
                jvResult[jss::account_sequence_available] =
                    safe_cast<Json::Value::UInt>(
                        currentLedgerState->accountSeqAvail);
                jvResult[jss::open_ledger_cost] =
                    to_string(currentLedgerState->minFeeRequired);
                jvResult[jss::validated_ledger_index] =
                    safe_cast<Json::Value::UInt>(
                        currentLedgerState->validatedLedger);
            }
        }

        return jvResult;
    }
    catch (std::exception& e)
    {
        jvResult[jss::error] = "internalJson";
        jvResult[jss::error_code] = rpcINTERNAL;
        jvResult[jss::error_message] = e.what();
        return jvResult;
    }
}

std::pair<org::zxcl::rpc::v1::SubmitTransactionResponse, grpc::Status>
doSubmitGrpc(
    RPC::GRPCContext<org::zxcl::rpc::v1::SubmitTransactionRequest>& context)
{
    // return values
    org::zxcl::rpc::v1::SubmitTransactionResponse result;
    grpc::Status status = grpc::Status::OK;

    // input
    auto request = context.params;

    std::string const& tx = request.signed_transaction();

    // convert to blob
    Blob blob{tx.begin(), tx.end()};

    // serialize
    SerialIter sitTrans(makeSlice(blob));
    std::shared_ptr<STTx const> stpTrans;
    try
    {
        stpTrans = std::make_shared<STTx const>(std::ref(sitTrans));
    }
    catch (std::exception& e)
    {
        grpc::Status errorStatus{
            grpc::StatusCode::INVALID_ARGUMENT,
            "invalid transaction: " + std::string(e.what())};
        return {result, errorStatus};
    }

    // check validity
    {
        if (!context.app.checkSigs())
            forceValidity(
                context.app.getHashRouter(),
                stpTrans->getTransactionID(),
                Validity::SigGoodOnly);
        auto [validity, reason] = checkValidity(
			context.app,
            context.app.getHashRouter(),
            *stpTrans,
            context.ledgerMaster.getCurrentLedger()->rules(),
            context.app.config());
        if (validity != Validity::Valid)
        {
            grpc::Status errorStatus{
                grpc::StatusCode::INVALID_ARGUMENT,
                "invalid transaction: " + reason};
            return {result, errorStatus};
        }
    }

    std::string reason;
    auto tpTrans = std::make_shared<Transaction>(stpTrans, reason, context.app);
    if (tpTrans->getStatus() != NEW)
    {
        grpc::Status errorStatus{
            grpc::StatusCode::INVALID_ARGUMENT,
            "invalid transaction: " + reason};
        return {result, errorStatus};
    }

    try
    {
        auto const failType = NetworkOPs::doFailHard(request.fail_hard());

        // submit to network
        context.netOps.processTransaction(
            tpTrans, isUnlimited(context.role), true, failType);
    }
    catch (std::exception& e)
    {
        grpc::Status errorStatus{
            grpc::StatusCode::INVALID_ARGUMENT,
            "invalid transaction : " + std::string(e.what())};
        return {result, errorStatus};
    }

    // return preliminary result
    if (temUNCERTAIN != tpTrans->getResult().ter)
    {
        RPC::convert(*result.mutable_engine_result(), tpTrans->getResult());

        std::string sToken;
        std::string sHuman;

        transResultInfo(tpTrans->getResult(), sToken, sHuman);

        result.mutable_engine_result()->set_result(sToken);
        result.set_engine_result_code(TERtoInt(tpTrans->getResult().ter));
        result.set_engine_result_message(sHuman);

        uint256 hash = tpTrans->getID();
        result.set_hash(hash.data(), hash.size());
    }
    return {result, status};
}

}  // namespace ripple
