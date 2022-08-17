//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012, 2013 Ripple Labs Inc.

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

#ifndef RIPPLE_RPC_HANDLERS_HANDLERS_H_INCLUDED
#define RIPPLE_RPC_HANDLERS_HANDLERS_H_INCLUDED

#include <ripple/rpc/handlers/LedgerHandler.h>

namespace ripple {

Json::Value
doAccountCurrencies(RPC::JsonContext&);
Json::Value
doAccountInfo(RPC::JsonContext&);
Json::Value
doAccountLines(RPC::JsonContext&);
Json::Value
doAccountChannels(RPC::JsonContext&);
Json::Value
doAccountObjects(RPC::JsonContext&);
Json::Value
doAccountOffers(RPC::JsonContext&);
Json::Value
doAccountTxSwitch(RPC::JsonContext&);
Json::Value
doAccountTxOld(RPC::JsonContext&);
Json::Value
doAccountTxJson(RPC::JsonContext&);
Json::Value
doAccountAuthorized(RPC::JsonContext&);
Json::Value
doContractTxJson(RPC::JsonContext& context);
Json::Value
doBookOffers(RPC::JsonContext&);
Json::Value
doBlackList(RPC::JsonContext&);
Json::Value
doCanDelete(RPC::JsonContext&);
Json::Value
doChannelAuthorize(RPC::JsonContext&);
Json::Value
doChannelVerify(RPC::JsonContext&);
Json::Value
doConnect(RPC::JsonContext&);
Json::Value
doConsensusInfo(RPC::JsonContext&);
Json::Value
doDepositAuthorized(RPC::JsonContext&);
Json::Value
doDownloadShard(RPC::JsonContext&);
Json::Value
doFeature(RPC::JsonContext&);
Json::Value
doFee(RPC::JsonContext&);
Json::Value
doFetchInfo(RPC::JsonContext&);
Json::Value
doGatewayBalances(RPC::JsonContext&);
Json::Value
doGetCounts(RPC::JsonContext&);
Json::Value
doLedgerAccept(RPC::JsonContext&);
Json::Value
doLedgerCleaner(RPC::JsonContext&);
Json::Value
doLedgerClosed(RPC::JsonContext&);
Json::Value
doLedgerCurrent(RPC::JsonContext&);
Json::Value
doLedgerData(RPC::JsonContext&);
Json::Value
doLedgerEntry(RPC::JsonContext&);
Json::Value
doLedgerHeader(RPC::JsonContext&);
Json::Value
doLedgerRequest(RPC::JsonContext&);
Json::Value
doLogLevel(RPC::JsonContext&);
Json::Value
doLogRotate(RPC::JsonContext&);
Json::Value
doManifest(RPC::JsonContext&);
Json::Value
doNoRippleCheck(RPC::JsonContext&);
Json::Value
doOwnerInfo(RPC::JsonContext&);
Json::Value
doPathFind(RPC::JsonContext&);
Json::Value
doPause(RPC::JsonContext&);
Json::Value
doPeers(RPC::JsonContext&);
Json::Value
doPing(RPC::JsonContext&);
Json::Value
doPrint(RPC::JsonContext&);
Json::Value
doRandom(RPC::JsonContext&);
Json::Value
doResume(RPC::JsonContext&);
Json::Value
doPeerReservationsAdd(RPC::JsonContext&);
Json::Value
doPeerReservationsDel(RPC::JsonContext&);
Json::Value
doPeerReservationsList(RPC::JsonContext&);
Json::Value
doRipplePathFind(RPC::JsonContext&);
Json::Value
doServerInfo(RPC::JsonContext&);  // for humans
Json::Value
doServerState(RPC::JsonContext&);  // for machines
Json::Value
doSign(RPC::JsonContext&);
Json::Value
doSignFor(RPC::JsonContext&);
Json::Value
doCrawlShards(RPC::JsonContext&);
Json::Value
doStop(RPC::JsonContext&);
Json::Value
doSubmit(RPC::JsonContext&);
Json::Value
doSubmitMultiSigned(RPC::JsonContext&);
Json::Value
doSubscribe(RPC::JsonContext&);
Json::Value
doTransactionEntry(RPC::JsonContext&);
Json::Value
doTxJson(RPC::JsonContext&);
Json::Value
doTxResult(RPC::JsonContext&);
Json::Value
doTxMerkleProof(RPC::JsonContext&);
Json::Value
doTxMerkleVerify(RPC::JsonContext&);
Json::Value
doTxHistory(RPC::JsonContext&);
Json::Value
doUnlList(RPC::JsonContext&);
Json::Value
doUnsubscribe(RPC::JsonContext&);
Json::Value
doValidationCreate(RPC::JsonContext&);
Json::Value
doWalletPropose(RPC::JsonContext&);
Json::Value
doValidators(RPC::JsonContext&);
Json::Value
doValidatorListSites(RPC::JsonContext&);
Json::Value
doValidatorInfo(RPC::JsonContext&);

Json::Value doLedgerTxs             (RPC::JsonContext&);
Json::Value doGetCrossChainTx       (RPC::JsonContext&);
Json::Value doTxCount				(RPC::JsonContext&);
Json::Value doLedgerObjects			(RPC::JsonContext&);
Json::Value doNodeSize              (RPC::JsonContext&);
Json::Value doMallocTrim            (RPC::JsonContext&);
Json::Value doSchemaList			(RPC::JsonContext&);
Json::Value doSchemaInfo            (RPC::JsonContext&);
Json::Value doSchemaAccept          (RPC::JsonContext&);
Json::Value doSchemaStart          (RPC::JsonContext&);
//for sql operation
Json::Value doTableDump(RPC::JsonContext&);
Json::Value doTableDumpStop(RPC::JsonContext&);
Json::Value getDumpCurPos(RPC::JsonContext& context);
Json::Value doTableAudit(RPC::JsonContext&);
Json::Value doTableAuditStop(RPC::JsonContext&);
Json::Value getAuditCurPos(RPC::JsonContext& context);
Json::Value doTableAuthority(RPC::JsonContext&);
Json::Value doRpcSubmit(RPC::JsonContext&);
Json::Value doCreateFromRaw(RPC::JsonContext&);
Json::Value doGetRecord(RPC::JsonContext&);
Json::Value doGetRecordBySql(RPC::JsonContext&);
Json::Value doGetRecordBySqlUser(RPC::JsonContext&);
std::pair<std::vector<std::vector<Json::Value>>, std::string> doGetRecord2D(RPC::JsonContext&  context);
Json::Value doGetDBName(RPC::JsonContext&);
Json::Value doGetAccountTables(RPC::JsonContext&);
Json::Value doPrepare(RPC::JsonContext&);
Json::Value doGetUserToken(RPC::JsonContext&);
Json::Value doGetCheckHash(RPC::JsonContext&);
Json::Value doValidators            (RPC::JsonContext&);
Json::Value doValidatorListSites    (RPC::JsonContext&);
Json::Value doTxInPool(RPC::JsonContext&);
Json::Value doSyncInfo(RPC::JsonContext&);
Json::Value doLedgerProof(RPC::JsonContext&);
Json::Value doMonitorStatis(RPC::JsonContext&);

//for contract
Json::Value doContractCall(RPC::JsonContext&);
Json::Value doEstimateGas(RPC::JsonContext&);

Json::Value doGenCsr(RPC::JsonContext&); // for humans

//for gm algorithm data generation
Json::Value doCreateRandom(RPC::JsonContext&);
Json::Value doCryptData(RPC::JsonContext&);

// Ethereum-compatible JSON RPC API
Json::Value doEthChainId(RPC::JsonContext&);
Json::Value doNetVersion(RPC::JsonContext&);
Json::Value doEthBlockNumber(RPC::JsonContext&);
Json::Value doEthGetBlockByNumber(RPC::JsonContext&);
Json::Value doEthGetBalance(RPC::JsonContext&);
Json::Value doEthCall(RPC::JsonContext&);
Json::Value doEthEstimateGas(RPC::JsonContext&);
Json::Value doEthSendRawTransaction(RPC::JsonContext&);
Json::Value doEthGetTransactionReceipt(RPC::JsonContext&);
Json::Value doEthGetTransactionByHash(RPC::JsonContext&);
Json::Value doEthGetTransactionCount(RPC::JsonContext&);
Json::Value doEthGasPrice(RPC::JsonContext&);
Json::Value doEthFeeHistory(RPC::JsonContext&);
Json::Value doEthGetCode(RPC::JsonContext&);

}  // namespace ripple

#endif
