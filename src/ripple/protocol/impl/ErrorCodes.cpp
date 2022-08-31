//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012 - 2019 Ripple Labs Inc.

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

#include <ripple/protocol/ErrorCodes.h>
#include <cassert>
#include <stdexcept>

namespace ripple {
namespace RPC {

namespace detail {

// Unordered array of ErrorInfos, so we don't have to maintain the list
// ordering by hand.
//
// This array will be omitted from the object file; only the sorted version
// will remain in the object file.  But the string literals will remain.
constexpr static ErrorInfo unorderedErrorInfos[]{
    {rpcACT_BITCOIN, "actBitcoin", "Account is bitcoin address."},
    {rpcACT_MALFORMED, "actMalformed", "Account malformed."},
    {rpcACT_NOT_FOUND, "actNotFound", "Account not found."},
    {rpcALREADY_MULTISIG, "alreadyMultisig", "Already multisigned."},
    {rpcALREADY_SINGLE_SIG, "alreadySingleSig", "Already single-signed."},
    {rpcACT_NOT_MATCH_PUBKEY,
     "actNotMatchPublic",
     "Account is not match with publicKey."},
    {rpcAMENDMENT_BLOCKED,
     "amendmentBlocked",
     "Amendment blocked, need upgrade."},
    {rpcATX_DEPRECATED,
     "deprecated",
     "Use the new API or specify a ledger range."},
    {rpcBAD_KEY_TYPE, "badKeyType", "Bad key type."},
    {rpcBAD_FEATURE, "badFeature", "Feature unknown or invalid."},
    {rpcBAD_ISSUER, "badIssuer", "Issuer account malformed."},
    {rpcBAD_MARKET, "badMarket", "No such market."},
    {rpcBAD_SECRET, "badSecret", "Secret does not match account."},
    {rpcBAD_SEED, "badSeed", "Disallowed seed."},
    {rpcBAD_SYNTAX, "badSyntax", "Syntax error."},
    {rpcCHANNEL_MALFORMED, "channelMalformed", "Payment channel is malformed."},
    {rpcCHANNEL_AMT_MALFORMED,
     "channelAmtMalformed",
     "Payment channel amount is malformed."},
    {rpcCOMMAND_MISSING, "commandMissing", "Missing command entry."},
    {rpcDB_DESERIALIZATION,
     "dbDeserialization",
     "Database deserialization error."},
    {rpcDST_ACT_MALFORMED,
     "dstActMalformed",
     "Destination account is malformed."},
    {rpcDST_ACT_MISSING, "dstActMissing", "Destination account not provided."},
    {rpcDST_ACT_NOT_FOUND, "dstActNotFound", "Destination account not found."},
    {rpcDST_AMT_MALFORMED,
     "dstAmtMalformed",
     "Destination amount/currency/issuer is malformed."},
    {rpcDST_AMT_MISSING,
     "dstAmtMissing",
     "Destination amount/currency/issuer is missing."},
    {rpcDST_ISR_MALFORMED,
     "dstIsrMalformed",
     "Destination issuer is malformed."},
    {rpcEXCESSIVE_LGR_RANGE, "excessiveLgrRange", "Ledger range exceeds 1000."},
    {rpcFORBIDDEN, "forbidden", "Bad credentials."},
    {rpcGENERAL,   "general",   "Generic error reason."},
    {rpcHIGH_FEE, "highFee", "Current transaction fee exceeds your limit."},
    {rpcINTERNAL, "internal", "Internal error."},
    {rpcINVALID_LGR_RANGE, "invalidLgrRange", "Ledger range is invalid."},
    {rpcINVALID_PARAMS, "invalidParams", "Invalid parameters."},
    {rpcJSON_RPC, "json_rpc", "JSON-RPC transport error."},
    {rpcLGR_IDXS_INVALID, "lgrIdxsInvalid", "Ledger indexes invalid."},
    {rpcLGR_IDX_MALFORMED, "lgrIdxMalformed", "Ledger index malformed."},
    {rpcLGR_NOT_FOUND, "lgrNotFound", "Ledger not found."},
    {rpcLGR_NOT_VALIDATED, "lgrNotValidated", "Ledger not validated."},
    {rpcMASTER_DISABLED, "masterDisabled", "Master key is disabled."},
    {rpcNOT_ENABLED, "notEnabled", "Not enabled in configuration."},
    {rpcNOT_IMPL, "notImpl", "Not implemented."},
    {rpcNOT_READY, "notReady", "Not ready to handle this request."},
    {rpcNOT_SUPPORTED, "notSupported", "Operation not supported."},
    {rpcNO_CLOSED, "noClosed", "Closed ledger is unavailable."},
    {rpcNO_CURRENT, "noCurrent", "Current ledger is unavailable."},
    {rpcNOT_SYNCED, "notSynced", "Not synced to the network."},
    {rpcNO_EVENTS, "noEvents", "Current transport does not support events."},
    {rpcNO_NETWORK, "noNetwork", "Not synced to the network."},
    {rpcNO_PERMISSION,
     "noPermission",
     "You don't have permission for this command."},
    {rpcNO_PF_REQUEST, "noPathRequest", "No pathfinding request in progress."},
    {rpcPUBLIC_MALFORMED, "publicMalformed", "Public key is malformed."},
    {rpcSIGNING_MALFORMED,
     "signingMalformed",
     "Signing of transaction is malformed."},
    {rpcSLOW_DOWN, "slowDown", "You are placing too much load on the server."},
    {rpcSRC_ACT_MALFORMED, "srcActMalformed", "Source account is malformed."},
    {rpcSRC_ACT_MISSING, "srcActMissing", "Source account not provided."},
    {rpcSRC_ACT_NOT_FOUND, "srcActNotFound", "Source account not found."},
    {rpcSRC_CUR_MALFORMED, "srcCurMalformed", "Source currency is malformed."},
    {rpcSRC_ISR_MALFORMED, "srcIsrMalformed", "Source issuer is malformed."},
    {rpcSTREAM_MALFORMED, "malformedStream", "Stream malformed."},
    {rpcTOO_BUSY, "tooBusy", "The server is too busy to help you now."},
    {rpcTXN_NOT_FOUND, "txnNotFound", "Transaction not found."},
    {rpcTXN_NOT_VALIDATED, "txnNotValidated", "Transaction not validated."},
    {rpcUNKNOWN_COMMAND, "unknownCmd", "Unknown method."},
    {rpcSENDMAX_MALFORMED, "sendMaxMalformed", "SendMax amount malformed."},

    { rpcJSON_PARSED_ERR,       "txJsonParsedErr",   "Tx Json parsed error." },
	{ rpcSQL_DISPOSE_ERR,       "disposeSqlErr",     "Dispose SQL common error info" },
	{ rpcSQL_SELECT_ONLY,       "sqlSelectOnly",     "First word of SQL must be select." },
	{ rpcDB_NOT_SUPPORT,        "dbTypeNotSupport",  "Do not support this db type." },
	{ rpcDB_CONNECT_FAILED,     "dbConnectFailed",   "Database connection is failed." },
	{ rpcTAB_NOT_EXIST,         "tabNotExist",       "Table does not exist." },
	{ rpcTAB_UNAUTHORIZED,      "tabUnauthorized",   "The user is unauthorized to the table." },
	{ rpcRAW_INVALID,           "rawNotValidated",   "Raw field is not validated." },
	{ rpcNAMEINDB_NOT_MATCH,    "dBNameNotMatchTabName","DBName is not matched with table name." },
	{ rpcSLE_TOKEN_MISSING,     "userSleTokenMissing",  "Missing 'Token' field in sle of the corresponding user." },
	{ rpcSIGN_NOT_MATCH,        "signDataNotMatch",     "Signing data does not match tx_json." },
	{ rpcSIGN_NOT_IN_HEX,       "signNotInHex",         "Signature is not in hex." },
	{ rpcGET_VALUE_INVALID,     "getValueInvalid",      "Get value invalid from syncTableState." },
	{ rpcGET_LGR_FAILED,        "getLedgerFailed",      "Get validated ledger failed." },
	{ rpcDUMP_GENERAL_ERR,      "dumpGeneralError",     "General error when start dump." },
	{ rpcDUMPSTOP_GENERAL_ERR,  "dumpStopGeneralError", "General error when stop dump." },
	{ rpcAUDIT_GENERAL_ERR,     "auditGeneralError",    "General error when start audit." },
	{ rpcAUDITSTOP_GENERAL_ERR, "auditStopGeneralError","General error when stop audit." },
	{ rpcFIELD_CONTENT_EMPTY,   "fieldContentEmpty",	   "Field content is empty." },
	{ rpcCTR_EVMEXE_EXCEPTION,  "contractEVMexeError",  "Contract execution exception." },
	{ rpcCTR_EVMCALL_EXCEPTION, "contractEVMcallError", "Contract execution exception." },
    { rpcTXN_BIGGER_THAN_MAXSIZE, "txnBiggerThanMaxsize","txn size is bigger than maxsize >500kb."},
	{ rpcSQL_MULQUERY_NOT_SUPPORT, "mulQueryNotSupport", "OperationRule Table not support multi_table sql_query." },
	{ rpcNO_SCHEMA,			    "schemaNotExist",		"No schema with the specified shema_id exist." },
	{ rpcSCHEMA_CREATED,	    "schemaCreated",		"Schema have already been created,will not create again." },
    { rpcSCHEMA_NOTMEMBER,	    "cannotJoinSchema",		"You are not a member of this schema." },
    { rpcNODB,				   "NoDbConfig",	    "Get db connection error,maybe db not configured." },
    { rpcUNRELIABLE_LEDGER_HEADER, "unreliableLedgerHeader",    "Verify ledger hash failed."},
    { rpcTX_HASH_NOT_MATCH,        "txHashNotMatch",            "Verify tx hash failed."},
    { rpcTX_NODEHASH_NOT_MATCH,    "txNodeHashNotMatch",        "Verify tx node hash failed."},
    { rpcBAD_PROOF,                "badProof",                  "Verify proof failed."},
    { rpcACCOUNT_ALREADY_DELETED,  "accountAlreadyDeleted",     "The account was already deleted."}

    // add (rpcACT_EXISTS,            "actExists",         "Account already exists.");
    // add (rpcBAD_BLOB,              "badBlob",           "Blob must be a non-empty hex string.");
    
    // add (rpcGETS_ACT_MALFORMED,    "getsActMalformed",  "Gets account malformed.");
    // add (rpcGETS_AMT_MALFORMED,    "getsAmtMalformed",  "Gets amount malformed.");
    // add (rpcHOST_IP_MALFORMED,     "hostIpMalformed",   "Host IP is malformed.");
    // add (rpcINSUF_FUNDS,           "insufFunds",        "Insufficient funds.");
    // add (rpcLOAD_FAILED,           "loadFailed",        "Load failed");
    // add (rpcNOT_STANDALONE,        "notStandAlone",     "Operation valid in debug mode only.");
    // add (rpcNO_ACCOUNT,            "noAccount",         "No such account.");
    // add (rpcNO_PATH,               "noPath",            "Unable to find a ripple path.");
    // add (rpcPASSWD_CHANGED,        "passwdChanged",     "Wrong key, password changed.");
    // add (rpcPAYS_ACT_MALFORMED,    "paysActMalformed",  "Pays account malformed.");
    // add (rpcPAYS_AMT_MALFORMED,    "paysAmtMalformed",  "Pays amount malformed.");
    // add (rpcPORT_MALFORMED,        "portMalformed",     "Port is malformed.");
    // add (rpcQUALITY_MALFORMED,     "qualityMalformed",  "Quality malformed.");
    // add (rpcSIGN_FOR_MALFORMED,    "signForMalformed",  "Signing for account is malformed.");
    // add (rpcSRC_AMT_MALFORMED,     "srcAmtMalformed",   "Source amount/currency/issuer is malformed.");
    // add (rpcSRC_MISSING,           "srcMissing",        "Source is missing.");
    // add (rpcSRC_UNCLAIMED,         "srcUnclaimed",      "Source account is not claimed.");
    // add (rpcWRONG_SEED,            "wrongSeed",         "The regular key does not point as the master key.");
};

// C++ does not allow you to return an array from a function.  You must
// return an object which may in turn contain an array.  The following
// struct is simply defined so the enclosed array can be returned from a
// constexpr function.
//
// In C++17 this struct can be replaced by a std::array.  But in C++14
// the constexpr methods of a std::array are not sufficient to perform the
// necessary work at compile time.
template <int N>
struct ErrorInfoArray
{
    // Visual Studio doesn't treat a templated aggregate as an aggregate.
    // So, for Visual Studio, we define a constexpr default constructor.
    constexpr ErrorInfoArray() : infos{}
    {
    }

    ErrorInfo infos[N];
};

// Sort and validate unorderedErrorInfos at compile time.  Should be
// converted to consteval when get to C++20.
template <int M, int N>
constexpr auto
sortErrorInfos(ErrorInfo const (&unordered)[N]) -> ErrorInfoArray<M>
{
    ErrorInfoArray<M> ret;

    for (ErrorInfo const& info : unordered)
    {
        if (info.code <= rpcSUCCESS || info.code > rpcLAST)
            throw(std::out_of_range("Invalid error_code_i"));

        // The first valid code follows rpcSUCCESS immediately.
        static_assert(rpcSUCCESS == 0, "Unexpected error_code_i layout.");
        int const index{info.code - 1};

        if (ret.infos[index].code != rpcUNKNOWN)
            throw(std::invalid_argument("Duplicate error_code_i in list"));

        ret.infos[index].code = info.code;
        ret.infos[index].token = info.token;
        ret.infos[index].message = info.message;
    }

    // Verify that all entries are filled in starting with 1 and proceeding
    // to rpcLAST.
    //
    // It's okay for there to be missing entries; they will contain the code
    // rpcUNKNOWN.  But other than that all entries should match their index.
    int codeCount{0};
    int expect{rpcBAD_SYNTAX - 1};
    for (ErrorInfo const& info : ret.infos)
    {
        ++expect;
        if (info.code == rpcUNKNOWN)
            continue;

        if (info.code != expect)
            throw(std::invalid_argument("Empty error_code_i in list"));
        ++codeCount;
    }
    if (expect != rpcLAST)
        throw(std::invalid_argument("Insufficient list entries"));
    if (codeCount != N)
        throw(std::invalid_argument("Bad handling of unorderedErrorInfos"));

    return ret;
}

constexpr auto sortedErrorInfos{sortErrorInfos<rpcLAST>(unorderedErrorInfos)};

constexpr ErrorInfo unknownError;

}  // namespace detail

//------------------------------------------------------------------------------

ErrorInfo const&
get_error_info(error_code_i code)
{
    if (code <= rpcSUCCESS || code > rpcLAST)
        return detail::unknownError;
    return detail::sortedErrorInfos.infos[code - 1];
}

Json::Value
make_error(error_code_i code)
{
    Json::Value json;
    inject_error(code, json);
    return json;
}

Json::Value
make_error(error_code_i code, std::string const& message)
{
    Json::Value json;
    inject_error(code, message, json);
    return json;
}

bool
contains_error(Json::Value const& json)
{
    if (json.isObject() && json.isMember(jss::error))
        return true;
    return false;
}

}  // namespace RPC

std::string
rpcErrorString(Json::Value const& jv)
{
    assert(RPC::contains_error(jv));
    return jv[jss::error].asString() + jv[jss::error_message].asString();
}

}  // namespace ripple
