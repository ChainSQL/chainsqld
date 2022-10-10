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

#include <ripple/json/json_value.h>
#include <ripple/net/RPCErr.h>
#include <ripple/protocol/TxFlags.h>
#include <ripple/rpc/impl/RPCHelpers.h>
#include <peersafe/basics/TypeTransform.h>
#include <peersafe/core/Tuning.h>
#include <peersafe/protocol/STMap256.h>

namespace ripple {

LedgerSpecificFlags
setFlag2LedgerFlag(std::uint32_t setFlag)
{
    switch (setFlag)
    {
        case asfPaymentAuth:
            return lsfPaymentAuth;
        case asfDeployContractAuth:
            return lsfDeployContractAuth;
        case asfCreateTableAuth:
            return lsfCreateTableAuth;
        case asfIssueCoinsAuth:
            return lsfIssueCoinsAuth;
        case asfAdminAuth:
            return lsfAdminAuth;
        default:
            return LedgerSpecificFlags(
                lsfPaymentAuth | lsfDeployContractAuth | lsfCreateTableAuth |
                lsfIssueCoinsAuth | lsfAdminAuth);
    }
}

std::string
setFlag2String(std::uint32_t setFlag)
{
    switch (setFlag)
    {
        case asfPaymentAuth:
            return "asfPaymentAuth";
        case asfDeployContractAuth:
            return "asfDeployContractAuth";
        case asfCreateTableAuth:
            return "asfCreateTableAuth";
        case asfIssueCoinsAuth:
            return "asfIssueCoinsAuth";
        case asfAdminAuth:
            return "asfAdminAuth";
        default:
            return "unknown";
    }
}

Json::Value
accountAuthorized(
    RPC::JsonContext& context,
    ReadView const& view,
    AccountID const& admin,
    uint256 startAfter,
    std::uint64_t startHint,
    unsigned int limit,
    std::uint32_t setFlag)
{
    Json::Value result;
    std::vector<std::pair<AccountID, unsigned int>> visitData;

    LedgerSpecificFlags flag = setFlag2LedgerFlag(setFlag);

    if (!forEachItemAfter(
            view,
            admin,
            startAfter,
            startHint,
            limit,
            [&visitData, flag](std::shared_ptr<SLE const> const& sleCur) {
                if (sleCur->getType() != ltACCOUNT_ROOT)
                    return false;
                unsigned int flags = sleCur->getFieldU32(sfFlags);
                if (!(flags & flag))
                    return false;
                visitData.emplace_back(sleCur->getAccountID(sfAccount), flags);
                return true;
            }))
    {
        return rpcError(rpcINVALID_PARAMS);
    }

    Json::Value authorized = Json::objectValue;
    if (setFlag)
    {
        Json::Value& accounts(
            authorized[setFlag2String(setFlag)] = Json::arrayValue);
        for (auto const& [accountID, _] : visitData)
        {
            boost::ignore_unused(_);
            accounts.append(context.app.accountIDCache().toBase58(accountID));
        }

        if (visitData.size() >= limit)
            result[jss::marker] =
                to_string(keylet::account(visitData.back().first).key);
    }
    else
    {
        Json::Value& paymentAuthAccounts(
            authorized[setFlag2String(asfPaymentAuth)] = Json::arrayValue);
        Json::Value& deployContractAuthAccounts(
            authorized[setFlag2String(asfDeployContractAuth)] =
                Json::arrayValue);
        Json::Value& createTableAuthAccounts(
            authorized[setFlag2String(asfCreateTableAuth)] = Json::arrayValue);
        Json::Value& issueCoinsAuthAccounts(
            authorized[setFlag2String(asfIssueCoinsAuth)] = Json::arrayValue);
        Json::Value& adminAuthAccounts(
            authorized[setFlag2String(asfAdminAuth)] = Json::arrayValue);
        for (auto const& [accountID, flags] : visitData)
        {
            auto const& accountStr =
                context.app.accountIDCache().toBase58(accountID);
            if (flags & lsfPaymentAuth)
                paymentAuthAccounts.append(accountStr);
            if (flags & lsfDeployContractAuth)
                deployContractAuthAccounts.append(accountStr);
            if (flags & lsfCreateTableAuth)
                createTableAuthAccounts.append(accountStr);
            if (flags & lsfIssueCoinsAuth)
                issueCoinsAuthAccounts.append(accountStr);
            if (flags & lsfAdminAuth)
                adminAuthAccounts.append(accountStr);
        }
    }

    result[jss::authorized] = authorized;

    return result;
}

Json::Value
doAccountAuthorized(RPC::JsonContext& context)
{
    auto& params = context.params;

    boost::optional<AccountID> admin = context.app.config().ADMIN;
    if (params.isMember(jss::Admin))
        admin = ripple::parseBase58<AccountID>(params[jss::Admin].asString());
    if (!admin)
        return RPC::missing_field_error(jss::Admin);

    std::uint32_t setFlag = 0;
    if (params.isMember(jss::SetFlag))
    {
        setFlag = params[jss::SetFlag].asUInt();
        if (setFlag != asfPaymentAuth && setFlag != asfDeployContractAuth &&
            setFlag != asfCreateTableAuth && setFlag != asfIssueCoinsAuth &&
            setFlag != asfAdminAuth)
        {
            return RPC::invalid_field_error(jss::SetFlag);
        }
    }

    std::shared_ptr<ReadView const> ledger;
    auto result = RPC::lookupLedger(ledger, context);

    if (!ledger)
        return result;

    if (!ledger->exists(keylet::account(*admin)))
    {
        result[jss::account] = context.app.accountIDCache().toBase58(*admin);
        RPC::inject_error(rpcACT_NOT_FOUND, result);
        return result;
    }

    // 针对授权数量比较多的链，可扩展接口参数，做分页查询
    unsigned int limit;
    if (auto err =
            readLimitField(limit, RPC::Tuning::accountAuthorized, context))
        return *err;

    uint256 startAfter{};
    std::uint64_t startHint = 0;
    if (params.isMember(jss::marker))
    {
        Json::Value const& marker(params[jss::marker]);

        if (!marker.isString())
            return RPC::expected_field_error(jss::marker, "string");

        startAfter.SetHex(marker.asString());
        auto const sleAccount = ledger->read({ltACCOUNT_ROOT, startAfter});

        if (!sleAccount)
            return rpcError(rpcINVALID_PARAMS);

        STMap256 const& mapExtension =
            sleAccount->getFieldM256(sfStorageExtension);
        if (mapExtension.has(NODE_TYPE_AUTHORIZE))
            startHint = fromUint256(mapExtension.at(NODE_TYPE_AUTHORIZE));
        else
            return rpcError(rpcINVALID_PARAMS);
    }

    bool pagination = false;
    if (limit != RPC::Tuning::accountAuthorized.rdefault ||
        startAfter != beast::zero)
        pagination = true;

    if (pagination)
    {
        // 分页查询必须指定权限数值
        if (!setFlag)
            return RPC::make_param_error(
                "Setflag must be set and not 0 when pagination");
        auto jvResult = accountAuthorized(
            context, *ledger, *admin, startAfter, startHint, limit, setFlag);
        if (jvResult.isMember(jss::error))
            return jvResult;
        result[jss::limit] = limit;
        result[jss::marker] = jvResult[jss::marker];
        result[jss::authorized] = jvResult[jss::authorized];
    }
    else
    {
        auto jvResult = accountAuthorized(
            context, *ledger, *admin, uint256{}, 0, limit, setFlag);
        if (jvResult.isMember(jss::error))
            return jvResult;
        result[jss::authorized] = jvResult[jss::authorized];
    }

    result[jss::Admin] = context.app.accountIDCache().toBase58(*admin);

    return result;
}

}  // namespace ripple
