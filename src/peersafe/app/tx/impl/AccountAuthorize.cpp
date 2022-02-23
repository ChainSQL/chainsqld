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

#include <peersafe/app/tx/AccountAuthorize.h>
#include <ripple/basics/Log.h>
#include <ripple/core/Config.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/st.h>
#include <ripple/ledger/View.h>
#include <ripple/basics/StringUtilities.h>

namespace ripple {

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
    auto& j = ctx.j;
    if (!ctx.app.config().NEED_AUTHORIZE)
        return tefNO_NEED_AUTHORIZE;
    auto const uSrcAccountID = ctx.tx[sfAccount];

    auto const uSetFlag = ctx.tx.getFieldU32(sfSetFlag);
    auto const uClearFlag = ctx.tx.getFieldU32(sfClearFlag);
    if ((uSetFlag != 0) && (uSetFlag == uClearFlag))
    {
        JLOG(j.trace()) << "Malformed transaction: Set and clear same flag.";
        return temINVALID_FLAG;
    }
    if (uSetFlag !=0 && !(uSetFlag & asfPaymentAuth) && !(uSetFlag & asfDeployContractAuth)
        && !(uSetFlag & asfCreateTableAuth) && !(uSetFlag & asfIssueCoinsAuth)
        && !(uSetFlag & asfAdminAuth))
        return temINVALID_FLAG;
    if (uClearFlag !=0 && !(uClearFlag & asfPaymentAuth) && !(uClearFlag & asfDeployContractAuth)
        && !(uClearFlag & asfCreateTableAuth) && !(uClearFlag & asfIssueCoinsAuth)
        && !(uClearFlag & asfAdminAuth))
        return temINVALID_FLAG;


    bool bSetAdminAuth = (uSetFlag == asfAdminAuth);
    bool bClearAdminAuth = (uClearFlag == asfAdminAuth);

    if (bSetAdminAuth && bClearAdminAuth)
    {
        JLOG(j.trace()) << "Malformed transaction: Contradictory flags set.";
        return temINVALID_FLAG;
    }

    auto const sleAdmin = ctx.view.read(keylet::admin());

    if (bSetAdminAuth || bClearAdminAuth)
    {
        if (!sleAdmin)
            return tefNO_ADMIN_CONFIGURED;
        AccountID const admin(sleAdmin->getAccountID(sfAccount));
        if (uSrcAccountID != admin)
            return tecNO_PERMISSION;
    }
    else
    {
        auto const srcSle = ctx.view.read(keylet::account(uSrcAccountID));
        if (!(srcSle->getFlags() & lsfAdminAuth))
        {
            return tecNO_PERMISSION;
        }
    }

    AccountID const uDstAccountID(ctx.tx.getAccountID(sfDestination));
    auto const k = keylet::account(uDstAccountID);
    auto const sleDst = ctx.view.read(k);
    if (!sleDst)
        return tecNO_DST;

    if (uSrcAccountID == uDstAccountID)
        return temDST_IS_SRC;

    return tesSUCCESS;
}

TER
AccountAuthorize::doApply()
{
   
    AccountID const uDstAccountID(ctx_.tx.getAccountID(sfDestination));
    auto const sle = view().peek(keylet::account(uDstAccountID));
    if (!sle)
        return tefINTERNAL;
    std::uint32_t const uFlagsIn = sle->getFieldU32(sfFlags);
    std::uint32_t uFlagsOut = uFlagsIn;

    STTx const& tx{ctx_.tx};
    std::uint32_t const uSetFlag{tx.getFieldU32(sfSetFlag)};
    std::uint32_t const uClearFlag{tx.getFieldU32(sfClearFlag)};

    bool const bSetPaymentAuth{(uSetFlag == asfPaymentAuth)};
    bool const bClearPaymentAuth{(uClearFlag == asfPaymentAuth)};
    bool const bSetDeployContractAuth{(uSetFlag == asfDeployContractAuth)};
    bool const bClearDeployContractAuth{(uClearFlag == asfDeployContractAuth)};
    bool const bSetCreateTableAuth{(uSetFlag == asfCreateTableAuth)};
    bool const bClearCreateTableAuth{(uClearFlag == asfCreateTableAuth)};
    bool const bSetIssueCoinsAuth{(uSetFlag == asfIssueCoinsAuth)};
    bool const bClearIssueCoinsAuth{(uClearFlag == asfIssueCoinsAuth)};
    bool const bSetAdminAuth{(uSetFlag == asfAdminAuth)};
    bool const bClearAdminAuth{(uClearFlag == asfAdminAuth)};

    if (bSetPaymentAuth && !(uFlagsIn & lsfPaymentAuth))
    {
        JLOG(j_.trace()) << "Set RequireAuth.";
        uFlagsOut |= lsfPaymentAuth;
    }else if (bClearPaymentAuth && (uFlagsIn & lsfPaymentAuth))
    {
        JLOG(j_.trace()) << "Clear RequireAuth.";
        uFlagsOut &= ~lsfPaymentAuth;
    }

    if (bSetDeployContractAuth && !(uFlagsIn & lsfDeployContractAuth))
    {
        JLOG(j_.trace()) << "Set RequireAuth.";
        uFlagsOut |= lsfDeployContractAuth;
    }else if (bClearDeployContractAuth && (uFlagsIn & lsfDeployContractAuth))
    {
        JLOG(j_.trace()) << "Clear RequireAuth.";
        uFlagsOut &= ~lsfDeployContractAuth;
    }

    if (bSetCreateTableAuth && !(uFlagsIn & lsfCreateTableAuth))
    {
        JLOG(j_.trace()) << "Set RequireAuth.";
        uFlagsOut |= lsfCreateTableAuth;
    }else if (bClearCreateTableAuth && (uFlagsIn & lsfCreateTableAuth))
    {
        JLOG(j_.trace()) << "Clear RequireAuth.";
        uFlagsOut &= ~lsfCreateTableAuth;
    }

    if (bSetIssueCoinsAuth && !(uFlagsIn & lsfIssueCoinsAuth))
    {
        JLOG(j_.trace()) << "Set RequireAuth.";
        uFlagsOut |= lsfIssueCoinsAuth;
    }else if (bClearIssueCoinsAuth && (uFlagsIn & lsfIssueCoinsAuth))
    {
        JLOG(j_.trace()) << "Clear RequireAuth.";
        uFlagsOut &= ~lsfIssueCoinsAuth;
    }

    if (bSetAdminAuth && !(uFlagsIn & lsfAdminAuth))
    {
        JLOG(j_.trace()) << "Set RequireAuth.";
        uFlagsOut |= lsfPaymentAuth | lsfDeployContractAuth |lsfCreateTableAuth
                | lsfIssueCoinsAuth | lsfAdminAuth;
    }else if (bClearAdminAuth && (uFlagsIn & lsfAdminAuth))
    {
        JLOG(j_.trace()) << "Clear RequireAuth.";
        uFlagsOut &= ~lsfPaymentAuth & ~lsfDeployContractAuth & ~lsfCreateTableAuth
            & ~lsfIssueCoinsAuth & ~lsfAdminAuth;
    }
    if (uFlagsIn != uFlagsOut)
        sle->setFieldU32(sfFlags, uFlagsOut);
    ctx_.view().update(sle);
    return tesSUCCESS;
}

} // ripple