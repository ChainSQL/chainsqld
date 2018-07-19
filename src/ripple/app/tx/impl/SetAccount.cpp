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

#include <BeastConfig.h>
#include <ripple/app/tx/impl/SetAccount.h>
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

bool
SetAccount::affectsSubsequentTransactionAuth(STTx const& tx)
{
    auto const uTxFlags = tx.getFlags();
    if(uTxFlags & (tfRequireAuth | tfOptionalAuth))
        return true;

    auto const uSetFlag = tx[~sfSetFlag];
    if(uSetFlag && (*uSetFlag == asfRequireAuth ||
        *uSetFlag == asfDisableMaster ||
            *uSetFlag == asfAccountTxnID))
                return true;

    auto const uClearFlag = tx[~sfClearFlag];
    return uClearFlag && (*uClearFlag == asfRequireAuth ||
        *uClearFlag == asfDisableMaster ||
            *uClearFlag == asfAccountTxnID);
}

TER
SetAccount::preflight (PreflightContext const& ctx)
{
    auto const ret = preflight1 (ctx);
    if (!isTesSuccess (ret))
        return ret;

    auto& tx = ctx.tx;
    auto& j = ctx.j;

    std::uint32_t const uTxFlags = tx.getFlags ();

    if (uTxFlags & tfAccountSetMask)
    {
        JLOG(j.trace()) << "Malformed transaction: Invalid flags set.";
        return temINVALID_FLAG;
    }

    std::uint32_t const uSetFlag = tx.getFieldU32 (sfSetFlag);
    std::uint32_t const uClearFlag = tx.getFieldU32 (sfClearFlag);

    if ((uSetFlag != 0) && (uSetFlag == uClearFlag))
    {
        JLOG(j.trace()) << "Malformed transaction: Set and clear same flag.";
        return temINVALID_FLAG;
    }

    //
    // RequireAuth
    //
    bool bSetRequireAuth   = (uTxFlags & tfRequireAuth) || (uSetFlag == asfRequireAuth);
    bool bClearRequireAuth = (uTxFlags & tfOptionalAuth) || (uClearFlag == asfRequireAuth);

    if (bSetRequireAuth && bClearRequireAuth)
    {
        JLOG(j.trace()) << "Malformed transaction: Contradictory flags set.";
        return temINVALID_FLAG;
    }

    //
    // RequireDestTag
    //
    bool bSetRequireDest   = (uTxFlags & TxFlag::requireDestTag) || (uSetFlag == asfRequireDest);
    bool bClearRequireDest = (uTxFlags & tfOptionalDestTag) || (uClearFlag == asfRequireDest);

    if (bSetRequireDest && bClearRequireDest)
    {
        JLOG(j.trace()) << "Malformed transaction: Contradictory flags set.";
        return temINVALID_FLAG;
    }

    //
    // DisallowZXC
    //
    bool bSetDisallowZXC   = (uTxFlags & tfDisallowZXC) || (uSetFlag == asfDisallowZXC);
    bool bClearDisallowZXC = (uTxFlags & tfAllowZXC) || (uClearFlag == asfDisallowZXC);

    if (bSetDisallowZXC && bClearDisallowZXC)
    {
        JLOG(j.trace()) << "Malformed transaction: Contradictory flags set.";
        return temINVALID_FLAG;
    }

    // TransferRate
    if (tx.isFieldPresent (sfTransferRate))
    {
        std::uint32_t uRate = tx.getFieldU32 (sfTransferRate);

        if (uRate && (uRate < QUALITY_ONE))
        {
            JLOG(j.trace()) << "Malformed transaction: Transfer rate too small.";
            return temBAD_TRANSFER_RATE;
        }

        if (ctx.rules.enabled(fix1201) && (uRate > 2 * QUALITY_ONE))
        {
            JLOG(j.trace()) << "Malformed transaction: Transfer rate too large.";
            return temBAD_TRANSFER_RATE;
        }
    }

	//TransferFee
	if (tx.isFieldPresent(sfTransferFeeMin) && tx.isFieldPresent(sfTransferFeeMax))
	{
		std::string feeMin = strCopy(tx.getFieldVL(sfTransferFeeMin));
		std::string feeMax = strCopy(tx.getFieldVL(sfTransferFeeMax));

		//RR-726
		if ( (!IsNumerialStr_Decimal(feeMin)) || (!IsNumerialStr_Decimal(feeMax)) )
		{
			JLOG(j.trace()) << "Malformed transaction: TransferFeeMin or TransferFeeMax invalid.";
			return temBAD_TRANSFERFEE;
		}
        float fMax = atof(feeMax.c_str());
        float fMin = atof(feeMin.c_str());
		if ((fMax != 0 && fMin > fMax) || fMin<0 || fMax < 0)
		{
			JLOG(j.trace()) << "Malformed transaction: TransferFeeMin can not be greater than TransferFeeMax.";
			return temBAD_TRANSFERFEE;
		}		
	}
	else if (tx.isFieldPresent(sfTransferFeeMin) || tx.isFieldPresent(sfTransferFeeMax))
	{
	JLOG(j.trace()) << "Malformed transaction: TransferFeeMin and TransferFeeMax can not be set individually.";
	return temBAD_TRANSFERFEE_BOTH;
	}

	// TickSize
	if (tx.isFieldPresent(sfTickSize))
	{
		if (!ctx.rules.enabled(featureTickSize))
			return temDISABLED;

		auto uTickSize = tx[sfTickSize];
		if (uTickSize &&
			((uTickSize < Quality::minTickSize) ||
			(uTickSize > Quality::maxTickSize)))
		{
			JLOG(j.trace()) << "Malformed transaction: Bad tick size.";
			return temBAD_TICK_SIZE;
		}
	}

	if (auto const mk = tx[~sfMessageKey])
	{
		if (mk->size() && !publicKeyType({ mk->data(), mk->size() }))
		{
			JLOG(j.trace()) << "Invalid message key specified.";
			return telBAD_PUBLIC_KEY;
		}
	}

	auto const domain = tx[~sfDomain];
	if (domain && domain->size() > DOMAIN_BYTES_MAX)
	{
		JLOG(j.trace()) << "domain too long";
		return telBAD_DOMAIN;
	}

	return preflight2(ctx);
}

TER
SetAccount::preclaim(PreclaimContext const& ctx)
{
	auto const id = ctx.tx[sfAccount];

	std::uint32_t const uTxFlags = ctx.tx.getFlags();

	auto const sle = ctx.view.read(
		keylet::account(id));

	std::uint32_t const uFlagsIn = sle->getFieldU32(sfFlags);

	std::uint32_t const uSetFlag = ctx.tx.getFieldU32(sfSetFlag);

	// legacy AccountSet flags
	bool bSetRequireAuth = (uTxFlags & tfRequireAuth) || (uSetFlag == asfRequireAuth);

	//
	// RequireAuth
	//
	if (bSetRequireAuth && !(uFlagsIn & lsfRequireAuth))
	{
		if (!dirIsEmpty(ctx.view,
			keylet::ownerDir(id)))
		{
			JLOG(ctx.j.trace()) << "Retry: Owner directory not empty.";
			return (ctx.flags & tapRETRY) ? terOWNERS : tecOWNERS;
		}
	}

	if (ctx.tx.isFieldPresent(sfTransferFeeMin) && ctx.tx.isFieldPresent(sfTransferFeeMax))
	{
		std::string feeMin = strCopy(ctx.tx.getFieldVL(sfTransferFeeMin));
		std::string feeMax = strCopy(ctx.tx.getFieldVL(sfTransferFeeMax));

		float fMax = atof(feeMax.c_str());
		float fMin = atof(feeMin.c_str());

		if ((fMin == 0 && fMax != 0) || (fMin != 0 && fMax == 0) || (fMin > 0 && fMax > 0 && fMin < fMax))
		{
			if (ctx.tx.isFieldPresent(sfTransferRate))
			{
				if (ctx.tx.getFieldU32(sfTransferRate) == QUALITY_ONE || ctx.tx.getFieldU32(sfTransferRate) == 0)
				{
					return temBAD_FEE_MISMATCH_TRANSFER_RATE;
				}
			}
			else if (sle->isFieldPresent(sfTransferRate))
			{
				if (sle->getFieldU32(sfTransferRate) == QUALITY_ONE || sle->getFieldU32(sfTransferRate) == 0)
				{
					return temBAD_FEE_MISMATCH_TRANSFER_RATE;
				}
			}
			else {
				return temBAD_FEE_MISMATCH_TRANSFER_RATE;
			}
		}
		else if (fMin > 0 && fMin == fMax)
		{
			if (ctx.tx.isFieldPresent(sfTransferRate))
			{
				if (ctx.tx.getFieldU32(sfTransferRate) > QUALITY_ONE)
				{
					return temBAD_FEE_MISMATCH_TRANSFER_RATE;
				}
			}
			else if(sle->isFieldPresent(sfTransferRate))
			{
				if (sle->getFieldU32(sfTransferRate) > QUALITY_ONE)
				{
					return temBAD_FEE_MISMATCH_TRANSFER_RATE;
				}
			}
        }
	}
	else if (ctx.tx.isFieldPresent(sfTransferRate) && ctx.tx.getFieldU32(sfTransferRate) > QUALITY_ONE)
	{
		if (sle->isFieldPresent(sfTransferFeeMin) && sle->isFieldPresent(sfTransferFeeMax))
		{
			std::string feeMin = strCopy(sle->getFieldVL(sfTransferFeeMin));
			std::string feeMax = strCopy(sle->getFieldVL(sfTransferFeeMax));
			if (feeMin == feeMax)
				return temBAD_FEE_MISMATCH_TRANSFER_RATE;
		}
	}
    return tesSUCCESS;
}

TER
SetAccount::doApply ()
{
    std::uint32_t const uTxFlags = ctx_.tx.getFlags ();

    auto const sle = view().peek(
        keylet::account(account_));

    std::uint32_t const uFlagsIn = sle->getFieldU32 (sfFlags);
    std::uint32_t uFlagsOut = uFlagsIn;

    std::uint32_t const uSetFlag = ctx_.tx.getFieldU32 (sfSetFlag);
    std::uint32_t const uClearFlag = ctx_.tx.getFieldU32 (sfClearFlag);

    // legacy AccountSet flags
    bool bSetRequireDest   = (uTxFlags & TxFlag::requireDestTag) || (uSetFlag == asfRequireDest);
    bool bClearRequireDest = (uTxFlags & tfOptionalDestTag) || (uClearFlag == asfRequireDest);
    bool bSetRequireAuth   = (uTxFlags & tfRequireAuth) || (uSetFlag == asfRequireAuth);
    bool bClearRequireAuth = (uTxFlags & tfOptionalAuth) || (uClearFlag == asfRequireAuth);
    bool bSetDisallowZXC   = (uTxFlags & tfDisallowZXC) || (uSetFlag == asfDisallowZXC);
    bool bClearDisallowZXC = (uTxFlags & tfAllowZXC) || (uClearFlag == asfDisallowZXC);

    bool sigWithMaster = false;

    {
        auto const spk = ctx_.tx.getSigningPubKey();

        if (publicKeyType (makeSlice (spk)))
        {
            PublicKey const signingPubKey (makeSlice (spk));

            if (calcAccountID(signingPubKey) == account_)
                sigWithMaster = true;
        }
    }

    //
    // RequireAuth
    //
    if (bSetRequireAuth && !(uFlagsIn & lsfRequireAuth))
    {
        JLOG(j_.trace()) << "Set RequireAuth.";
        uFlagsOut |= lsfRequireAuth;
    }

    if (bClearRequireAuth && (uFlagsIn & lsfRequireAuth))
    {
        JLOG(j_.trace()) << "Clear RequireAuth.";
        uFlagsOut &= ~lsfRequireAuth;
    }

    //
    // RequireDestTag
    //
    if (bSetRequireDest && !(uFlagsIn & lsfRequireDestTag))
    {
        JLOG(j_.trace()) << "Set lsfRequireDestTag.";
        uFlagsOut |= lsfRequireDestTag;
    }

    if (bClearRequireDest && (uFlagsIn & lsfRequireDestTag))
    {
        JLOG(j_.trace()) << "Clear lsfRequireDestTag.";
        uFlagsOut &= ~lsfRequireDestTag;
    }

    //
    // DisallowZXC
    //
    if (bSetDisallowZXC && !(uFlagsIn & lsfDisallowZXC))
    {
        JLOG(j_.trace()) << "Set lsfDisallowZXC.";
        uFlagsOut |= lsfDisallowZXC;
    }

    if (bClearDisallowZXC && (uFlagsIn & lsfDisallowZXC))
    {
        JLOG(j_.trace()) << "Clear lsfDisallowZXC.";
        uFlagsOut &= ~lsfDisallowZXC;
    }

    //
    // DisableMaster
    //
    if ((uSetFlag == asfDisableMaster) && !(uFlagsIn & lsfDisableMaster))
    {
        if (!sigWithMaster)
        {
            JLOG(j_.trace()) << "Must use master key to disable master key.";
            return tecNEED_MASTER_KEY;
        }

        if ((!sle->isFieldPresent (sfRegularKey)) &&
            (!view().peek (keylet::signers (account_))))
        {
            // Account has no regular key or multi-signer signer list.

            // Prevent transaction changes until we're ready.
            if (view().rules().enabled(featureMultiSign))
                return tecNO_ALTERNATIVE_KEY;

            return tecNO_REGULAR_KEY;
        }

        JLOG(j_.trace()) << "Set lsfDisableMaster.";
        uFlagsOut |= lsfDisableMaster;
    }

    if ((uClearFlag == asfDisableMaster) && (uFlagsIn & lsfDisableMaster))
    {
        JLOG(j_.trace()) << "Clear lsfDisableMaster.";
        uFlagsOut &= ~lsfDisableMaster;
    }

    //
    // DefaultRipple
    //
    if (uSetFlag == asfDefaultRipple)
    {
        uFlagsOut   |= lsfDefaultRipple;
    }
    else if (uClearFlag == asfDefaultRipple)
    {
        uFlagsOut   &= ~lsfDefaultRipple;
    }

    //
    // NoFreeze
    //
    if (uSetFlag == asfNoFreeze)
    {
        if (!sigWithMaster && !(uFlagsIn & lsfDisableMaster))
        {
            JLOG(j_.trace()) << "Can't use regular key to set NoFreeze.";
            return tecNEED_MASTER_KEY;
        }

        JLOG(j_.trace()) << "Set NoFreeze flag";
        uFlagsOut |= lsfNoFreeze;
    }

    // Anyone may set global freeze
    if (uSetFlag == asfGlobalFreeze)
    {
        JLOG(j_.trace()) << "Set GlobalFreeze flag";
        uFlagsOut |= lsfGlobalFreeze;
    }

    // If you have set NoFreeze, you may not clear GlobalFreeze
    // This prevents those who have set NoFreeze from using
    // GlobalFreeze strategically.
    if ((uSetFlag != asfGlobalFreeze) && (uClearFlag == asfGlobalFreeze) &&
        ((uFlagsOut & lsfNoFreeze) == 0))
    {
        JLOG(j_.trace()) << "Clear GlobalFreeze flag";
        uFlagsOut &= ~lsfGlobalFreeze;
    }

    //
    // Track transaction IDs signed by this account in its root
    //
    if ((uSetFlag == asfAccountTxnID) && !sle->isFieldPresent (sfAccountTxnID))
    {
        JLOG(j_.trace()) << "Set AccountTxnID";
        sle->makeFieldPresent (sfAccountTxnID);
        }

    if ((uClearFlag == asfAccountTxnID) && sle->isFieldPresent (sfAccountTxnID))
    {
        JLOG(j_.trace()) << "Clear AccountTxnID";
        sle->makeFieldAbsent (sfAccountTxnID);
    }

    //
    // EmailHash
    //
    if (ctx_.tx.isFieldPresent (sfEmailHash))
    {
        uint128 const uHash = ctx_.tx.getFieldH128 (sfEmailHash);

        if (!uHash)
        {
            JLOG(j_.trace()) << "unset email hash";
            sle->makeFieldAbsent (sfEmailHash);
        }
        else
        {
            JLOG(j_.trace()) << "set email hash";
            sle->setFieldH128 (sfEmailHash, uHash);
        }
    }
    //
    // Memos
    //
    if (ctx_.tx.isFieldPresent (sfMemos))
    {
        STArray const& memos = ctx_.tx.getFieldArray(sfMemos);
        sle->setFieldArray(sfMemos, memos);
    }
    
    //
    // WalletLocator
    //
    if (ctx_.tx.isFieldPresent (sfWalletLocator))
    {
        uint256 const uHash = ctx_.tx.getFieldH256 (sfWalletLocator);

        if (!uHash)
        {
            JLOG(j_.trace()) << "unset wallet locator";
            sle->makeFieldAbsent (sfWalletLocator);
        }
        else
        {
            JLOG(j_.trace()) << "set wallet locator";
            sle->setFieldH256 (sfWalletLocator, uHash);
        }
    }

    //
    // MessageKey
    //
    if (ctx_.tx.isFieldPresent (sfMessageKey))
    {
        Blob const messageKey = ctx_.tx.getFieldVL (sfMessageKey);

        if (messageKey.empty ())
        {
            JLOG(j_.debug()) << "set message key";
            sle->makeFieldAbsent (sfMessageKey);
        }
        else
        {
            JLOG(j_.debug()) << "set message key";
            sle->setFieldVL (sfMessageKey, messageKey);
        }
    }

    //
    // Domain
    //
    if (ctx_.tx.isFieldPresent (sfDomain))
    {
        Blob const domain = ctx_.tx.getFieldVL (sfDomain);

        if (domain.empty ())
        {
            JLOG(j_.trace()) << "unset domain";
            sle->makeFieldAbsent (sfDomain);
        }
        else
        {
            JLOG(j_.trace()) << "set domain";
            sle->setFieldVL (sfDomain, domain);
        }
    }

    //
    // TransferRate
    //
    if (ctx_.tx.isFieldPresent (sfTransferRate))
    {
        std::uint32_t uRate = ctx_.tx.getFieldU32 (sfTransferRate);

        if (uRate == 0 || uRate == QUALITY_ONE)
        {
            JLOG(j_.trace()) << "unset transfer rate";
            sle->makeFieldAbsent (sfTransferRate);
        }
        else
        {
            JLOG(j_.trace()) << "set transfer rate";
            sle->setFieldU32 (sfTransferRate, uRate);
        }
    }

	//
	// TransferFee
	//
	if (ctx_.tx.isFieldPresent(sfTransferFeeMin) && ctx_.tx.isFieldPresent(sfTransferFeeMax))
	{		
		// if you want to unset transferfee-min just set it to 0
		// if you want to unset transferfee-max just set it to 0
        std::string feeMin = strCopy(ctx_.tx.getFieldVL(sfTransferFeeMin));
		std::string feeMax = strCopy(ctx_.tx.getFieldVL(sfTransferFeeMax));

        float fMax = atof(feeMax.c_str());
        float fMin = atof(feeMin.c_str());
        if(fMin == 0){
            sle->makeFieldAbsent(sfTransferFeeMin);
        }else{
            sle->setFieldVL(sfTransferFeeMin, ctx_.tx.getFieldVL(sfTransferFeeMin));
        }
        
		if(fMax == 0){
            sle->makeFieldAbsent(sfTransferFeeMax);
        }else{
            sle->setFieldVL(sfTransferFeeMax, ctx_.tx.getFieldVL(sfTransferFeeMax));
        }
	}

    //
    // TickSize
    //
    if (ctx_.tx.isFieldPresent (sfTickSize))
    {
        auto uTickSize = ctx_.tx[sfTickSize];
        if ((uTickSize == 0) || (uTickSize == Quality::maxTickSize))
        {
            JLOG(j_.trace()) << "unset tick size";
            sle->makeFieldAbsent (sfTickSize);
        }
        else
        {
            JLOG(j_.trace()) << "set tick size";
            sle->setFieldU8 (sfTickSize, uTickSize);
        }
    }

    if (uFlagsIn != uFlagsOut)
        sle->setFieldU32 (sfFlags, uFlagsOut);

    return tesSUCCESS;
}

}
