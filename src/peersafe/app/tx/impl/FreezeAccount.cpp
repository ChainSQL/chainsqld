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

#include <peersafe/app/tx/FreezeAccount.h>
#include <ripple/basics/Log.h>
#include <ripple/core/Config.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/PublicKey.h>
#include <ripple/protocol/Quality.h>
#include <ripple/protocol/st.h>
#include <ripple/ledger/View.h>
#include <ripple/basics/StringUtilities.h>

namespace ripple {

NotTEC
FreezeAccount::preflight(PreflightContext const& ctx)
{
	return tesSUCCESS;
}

TER
FreezeAccount::preclaim(PreclaimContext const& ctx)
{
    if (!ctx.app.config().ADMIN)
        return tefNO_ADMIN_CONFIGURED;

    AccountID const uSrcAccountID(ctx.tx.getAccountID(sfAccount));
    if (uSrcAccountID != *ctx.app.config().ADMIN)
        return tecNO_PERMISSION;

    AccountID const uDstAccountID(ctx.tx.getAccountID(sfDestination));
    auto const k = keylet::account(uDstAccountID);
    auto const sleDst = ctx.view.read(k);
    if (!sleDst)
        return tecNO_DST;

    if (uSrcAccountID == uDstAccountID)
        return temDST_IS_SRC;

    if (!(ctx.tx.getFlags() & tfFreezeAccount) && !(ctx.tx.getFlags() & tfUnFreezeAccount))
        return temINVALID_FLAG;

    return tesSUCCESS;
}

TER
FreezeAccount::doApply()
{
    std::shared_ptr<SLE> sleFrozen;
    STObject objFrozen(sfFrozen);
    STArray frozenAccounts;
    bool isExist = true;
    sleFrozen = ctx_.view().peek(keylet::frozen());

    if (!sleFrozen)
    {
        sleFrozen = std::make_shared<SLE>(keylet::frozen());
        isExist = false;
    }
    else
    {
        objFrozen = sleFrozen->getFieldObject(sfFrozen);
        frozenAccounts = objFrozen.peekFieldArray(sfFrozenAccounts);
    }
    

    AccountID const uDstAccountID(ctx_.tx.getAccountID(sfDestination));

    auto iter(frozenAccounts.end());
    iter = std::find_if(frozenAccounts.begin(), frozenAccounts.end(),
        [uDstAccountID](STObject const &item) {
        if (!item.isFieldPresent(sfAccount))
            return false;
        return item.getAccountID(sfAccount) == uDstAccountID;
    });

    if (ctx_.tx.getFlags() & tfUnFreezeAccount)
    {
        // unFreeze
        if (iter == frozenAccounts.end())
            return tefACCOUNT_NOT_FROZEN;

        frozenAccounts.erase(iter);
    }
    else if (ctx_.tx.getFlags() & tfFreezeAccount)
    {
        // Freeze
        if (iter != frozenAccounts.end())
            return tefACCOUNT_FROZEN;

        STObject newAccount(sfAccount);
        newAccount.setAccountID(sfAccount, uDstAccountID);
        frozenAccounts.push_back(newAccount);
    }

    objFrozen.setFieldArray(sfFrozenAccounts, frozenAccounts);
    sleFrozen->setFieldObject(sfFrozen, objFrozen);

    if (isExist)
        ctx_.view().update(sleFrozen);
    else
        ctx_.view().insert(sleFrozen);

    return tesSUCCESS;
}

} // ripple