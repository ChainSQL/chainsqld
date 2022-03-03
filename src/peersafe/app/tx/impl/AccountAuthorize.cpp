//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012, 2013 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
    WARRANTIES WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED
    WARRANTIES  OF MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE
    LIABLE FOR ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY
    DAMAGES WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER
    IN AN ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
    OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include <ripple/basics/Log.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/core/Config.h>
#include <ripple/ledger/View.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/st.h>
#include <peersafe/app/tx/AccountAuthorize.h>

namespace ripple {

bool
AccountAuthorize::affectsFlagCheck(
    std::uint32_t const setFlag,
    std::uint32_t const clearFlag)
{
    if (setFlag == clearFlag)
    {
        return false;
    }

    if (setFlag)
    {
        if (setFlag != asfPaymentAuth && setFlag != asfDeployContractAuth &&
            setFlag != asfCreateTableAuth && setFlag != asfIssueCoinsAuth &&
            setFlag != asfAdminAuth)
        {
            return false;
        }
    }

    if (clearFlag)
    {
        if (clearFlag != asfPaymentAuth && clearFlag != asfDeployContractAuth &&
            clearFlag != asfCreateTableAuth && clearFlag != asfIssueCoinsAuth &&
            clearFlag != asfAdminAuth)
        {
            return false;
        }
    }

    return true;
}

NotTEC
AccountAuthorize::preflight(PreflightContext const& ctx)
{
    auto const ret = preflight1(ctx);
    if (!isTesSuccess(ret))
        return ret;

    if (!ctx.tx.isFieldPresent(sfSetFlag) &&
        !ctx.tx.isFieldPresent(sfClearFlag))
        return temMALFORMED;

    return preflight2(ctx);
}

TER
AccountAuthorize::preclaim(PreclaimContext const& ctx)
{
    std::uint32_t const uSetFlag = ctx.tx.getFieldU32(sfSetFlag);
    std::uint32_t const uClearFlag = ctx.tx.getFieldU32(sfClearFlag);

    if (!affectsFlagCheck(uSetFlag, uClearFlag))
        return temINVALID_FLAG;

    AccountID const uSrcAccountID = ctx.tx[sfAccount];
    AccountID const uDstAccountID(ctx.tx.getAccountID(sfDestination));

    if (uSrcAccountID == uDstAccountID)
        return temDST_IS_SRC;

    auto const k = keylet::account(uDstAccountID);
    auto const sleDst = ctx.view.read(k);
    if (!sleDst)
        return tecNO_DST;

    if (uSetFlag == asfAdminAuth || uClearFlag == asfAdminAuth)
    {
        // Supper admin check
        if (!ctx.app.config().ADMIN)
            return tefNO_ADMIN_CONFIGURED;

        if (uSrcAccountID != *ctx.app.config().ADMIN)
            return tecNO_PERMISSION;
    }
    else
    {
        if (ctx.app.config().ADMIN && uSrcAccountID == *ctx.app.config().ADMIN)
            return tesSUCCESS;

        // Generic admin check
        auto const srcSle = ctx.view.read(keylet::account(uSrcAccountID));
        if (!(srcSle->getFlags() & lsfAdminAuth))
            return tecNO_PERMISSION;
    }

    return tesSUCCESS;
}

TER
AccountAuthorize::doApply()
{
    AccountID const uDstAccountID(ctx_.tx.getAccountID(sfDestination));
    auto const sle = view().peek(keylet::account(uDstAccountID));
    if (!sle)
        return tefINTERNAL;

    std::uint32_t const uSetFlag = ctx_.tx.getFieldU32(sfSetFlag);
    std::uint32_t const uClearFlag = ctx_.tx.getFieldU32(sfClearFlag);

    std::uint32_t const uFlagsIn = sle->getFieldU32(sfFlags);
    std::uint32_t uFlagsOut = uFlagsIn;

    switch (uSetFlag)
    {
        case asfPaymentAuth:
            setAuthority(uFlagsOut, lsfPaymentAuth);
            break;
        case asfDeployContractAuth:
            setAuthority(uFlagsOut, lsfDeployContractAuth);
            break;
        case asfCreateTableAuth:
            setAuthority(uFlagsOut, lsfCreateTableAuth);
            break;
        case asfIssueCoinsAuth:
            setAuthority(uFlagsOut, lsfIssueCoinsAuth);
            break;
        case asfAdminAuth:
            setAuthority(uFlagsOut, lsfPaymentAuth);
            setAuthority(uFlagsOut, lsfDeployContractAuth);
            setAuthority(uFlagsOut, lsfCreateTableAuth);
            setAuthority(uFlagsOut, lsfIssueCoinsAuth);
            uFlagsOut |= lsfAdminAuth;
            break;
        default:
            break;
    }

    switch (uClearFlag)
    {
        case asfPaymentAuth:
            clearAuthority(uFlagsOut, lsfPaymentAuth);
            break;
        case asfDeployContractAuth:
            clearAuthority(uFlagsOut, lsfDeployContractAuth);
            break;
        case asfCreateTableAuth:
            clearAuthority(uFlagsOut, lsfCreateTableAuth);
            break;
        case asfIssueCoinsAuth:
            clearAuthority(uFlagsOut, lsfIssueCoinsAuth);
            break;
        case asfAdminAuth:
            clearAuthority(uFlagsOut, lsfPaymentAuth);
            clearAuthority(uFlagsOut, lsfDeployContractAuth);
            clearAuthority(uFlagsOut, lsfCreateTableAuth);
            clearAuthority(uFlagsOut, lsfIssueCoinsAuth);
            uFlagsOut &= ~lsfAdminAuth;
            break;
        default:
            break;
    }

    if (uFlagsIn != uFlagsOut)
        sle->setFieldU32(sfFlags, uFlagsOut);

    ctx_.view().update(sle);

    return tesSUCCESS;
}

void
AccountAuthorize::setAuthority(std::uint32_t& flags, LedgerSpecificFlags flag)
{
    if (ctx_.app.config().DEFAULT_AUTHORITY_ENABLED)
        flags |= flag;
    else
        flags &= ~flag;
}

void
AccountAuthorize::clearAuthority(std::uint32_t& flags, LedgerSpecificFlags flag)
{
    if (ctx_.app.config().DEFAULT_AUTHORITY_ENABLED)
        flags &= ~flag;
    else
        flags |= flag;
}

}  // namespace ripple
