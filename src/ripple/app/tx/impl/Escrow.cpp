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

#include <ripple/app/tx/impl/Escrow.h>

#include <ripple/app/misc/HashRouter.h>
#include <ripple/basics/Log.h>
#include <ripple/basics/ZXCAmount.h>
#include <ripple/basics/chrono.h>
#include <ripple/basics/safe_cast.h>
#include <ripple/conditions/Condition.h>
#include <ripple/conditions/Fulfillment.h>
#include <ripple/ledger/ApplyView.h>
#include <ripple/ledger/View.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/TxFlags.h>
#include <ripple/app/paths/RippleState.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/protocol/Quality.h>
#include <ripple/protocol/digest.h>
#include <ripple/protocol/st.h>

// During an EscrowFinish, the transaction must specify both
// a condition and a fulfillment. We track whether that
// fulfillment matches and validates the condition.
#define SF_CF_INVALID SF_PRIVATE5
#define SF_CF_VALID SF_PRIVATE6

namespace ripple {

/*
    Escrow
    ======

    Escrow is a feature of the ZXC Ledger that allows you to send conditional
    ZXC payments. These conditional payments, called escrows, set aside ZXC and
    deliver it later when certain conditions are met. Conditions to successfully
    finish an escrow include time-based unlocks and crypto-conditions. Escrows
    can also be set to expire if not finished in time.

    The ZXC set aside in an escrow is locked up. No one can use or destroy the
    ZXC until the escrow has been successfully finished or canceled. Before the
    expiration time, only the intended receiver can get the ZXC. After the
    expiration time, the ZXC can only be returned to the sender.

    For more details on escrow, including examples, diagrams and more please
    visit https://ripple.com/build/escrow/#escrow

    For details on specific transactions, including fields and validation rules
    please see:

    `EscrowCreate`
    --------------
        See: https://ripple.com/build/transactions/#escrowcreate

    `EscrowFinish`
    --------------
        See: https://ripple.com/build/transactions/#escrowfinish

    `EscrowCancel`
    --------------
        See: https://ripple.com/build/transactions/#escrowcancel
*/

//------------------------------------------------------------------------------

/** Has the specified time passed?

    @param now  the current time
    @param mark the cutoff point
    @return true if \a now refers to a time strictly after \a mark, else false.
*/
static inline bool
after(NetClock::time_point now, std::uint32_t mark)
{
    return now.time_since_epoch().count() > mark;
}

ZXCAmount
EscrowCreate::calculateMaxSpend(STTx const& tx)
{
    return tx[sfAmount].zxc();
}

NotTEC
EscrowCreate::preflight(PreflightContext const& ctx)
{
    if (ctx.rules.enabled(fix1543) && ctx.tx.getFlags() & tfUniversalMask)
        return temINVALID_FLAG;

    auto const ret = preflight1(ctx);
    if (!isTesSuccess(ret))
        return ret;

    //if (!isZXC(ctx.tx[sfAmount]))
    //    return temBAD_AMOUNT;

    if (ctx.tx[sfAmount] <= beast::zero)
        return temBAD_AMOUNT;

    // We must specify at least one timeout value
    if (!ctx.tx[~sfCancelAfter] && !ctx.tx[~sfFinishAfter])
        return temBAD_EXPIRATION;

    // If both finish and cancel times are specified then the cancel time must
    // be strictly after the finish time.
    if (ctx.tx[~sfCancelAfter] && ctx.tx[~sfFinishAfter] &&
        ctx.tx[sfCancelAfter] <= ctx.tx[sfFinishAfter])
        return temBAD_EXPIRATION;

    if (ctx.rules.enabled(fix1571))
    {
        // In the absence of a FinishAfter, the escrow can be finished
        // immediately, which can be confusing. When creating an escrow,
        // we want to ensure that either a FinishAfter time is explicitly
        // specified or a completion condition is attached.
        if (!ctx.tx[~sfFinishAfter] && !ctx.tx[~sfCondition])
            return temMALFORMED;
    }

    if (auto const cb = ctx.tx[~sfCondition])
    {
        using namespace ripple::cryptoconditions;

        std::error_code ec;

        auto condition = Condition::deserialize(*cb, ec);
        if (!condition)
        {
            JLOG(ctx.j.debug())
                << "Malformed condition during escrow creation: "
                << ec.message();
            return temMALFORMED;
        }

        // Conditions other than PrefixSha256 require the
        // "CryptoConditionsSuite" amendment:
        if (condition->type != Type::preimageSha256 &&
            !ctx.rules.enabled(featureCryptoConditionsSuite))
            return temDISABLED;
    }

    return preflight2(ctx);
}
TER
EscrowCreate::preclaim(PreclaimContext const& ctx)
{
    auto const& dest = ctx.tx[sfDestination];
    return checkAuthority(
        ctx, ctx.tx.getAccountID(sfAccount), lsfPaymentAuth, dest);
}
TER
EscrowCreate::doApply()
{
    //auto const closeTime = ctx_.view().info().parentCloseTime;

    // Prior to fix1571, the cancel and finish times could be greater
    // than or equal to the parent ledgers' close time.
    //
    // With fix1571, we require that they both be strictly greater
    // than the parent ledgers' close time.
    //if (ctx_.view().rules().enabled(fix1571))
    //{
    //    if (ctx_.tx[~sfCancelAfter] && after(closeTime, ctx_.tx[sfCancelAfter]))
    //        return tecNO_PERMISSION;

    //    if (ctx_.tx[~sfFinishAfter] && after(closeTime, ctx_.tx[sfFinishAfter]))
    //        return tecNO_PERMISSION;
    //}
    //else
    //{
        if (ctx_.tx[~sfCancelAfter])
        {
            auto const cancelAfter = ctx_.tx[sfCancelAfter];

            if (ctx_.app.getLedgerMaster().getLastConsensusTime() >= cancelAfter)
                return tecNO_PERMISSION;
        }

        if (ctx_.tx[~sfFinishAfter])
        {
            auto const finishAfter = ctx_.tx[sfFinishAfter];

            if (ctx_.app.getLedgerMaster().getLastConsensusTime() >= finishAfter)
                return tecNO_PERMISSION;
        }
    //}

    auto const account = ctx_.tx[sfAccount];
    auto const sle = ctx_.view().peek(keylet::account(account));
    if (!sle)
    {
        return tefINTERNAL;
    }


	auto const& amount = ctx_.tx[sfAmount];
	bool isZxc = isZXC(amount);
    // Check reserve and funds availability
    {
		auto const balance = STAmount((*sle)[sfBalance]).zxc();
		auto const reserve = ctx_.view().fees().accountReserve(
			(*sle)[sfOwnerCount] + 1);

		if (balance < reserve)
			return tecINSUFFICIENT_RESERVE;

		if (isZxc)
		{
			if (balance < reserve + STAmount(ctx_.tx[sfAmount]).zxc())
				return tecUNFUNDED;
		}// if src is not issuer
		else if(account_ != amount.getIssuer())
		{
			SLE::pointer sleRippleStateSrc = view().peek(
				keylet::line(account_, amount.getIssuer(), amount.getCurrency()));
			auto rs = RippleState::makeItem(account_, sleRippleStateSrc);

			if (!rs)
				return tecNO_LINE;
			
			auto balance = rs->getBalance();
			if (balance.negative())
			{
				STAmount amountTmp = amount;
				amountTmp.negate();
				if (balance > amountTmp)
					return tecUNFUNDED_ESCROW;
			}
			else
			{
				if (balance < amount)
					return tecUNFUNDED_ESCROW;
			}
		}
    }

    // Check destination account
    {
        auto const& dest = ctx_.tx[sfDestination];
        auto const sled =
            ctx_.view().read(keylet::account(dest));
        if (!sled)
            return tecNO_DST;
        if (((*sled)[sfFlags] & lsfRequireDestTag) &&
            !ctx_.tx[~sfDestinationTag])
            return tecDST_TAG_NEEDED;
		if (isZxc)
		{
			if (! ctx_.view().rules().enabled(featureDepositAuth) &&
				((*sled)[sfFlags] & lsfDisallowZXC))
				return tecNO_TARGET;
		}
		else
		{
			SLE::pointer sleRippleStateDst = view().peek(
				keylet::line(dest, amount.getIssuer(), amount.getCurrency()));
			if (!sleRippleStateDst)
				return tecNO_LINE;

			bool const bHigh = dest > amount.getIssuer();
			auto limit = sleRippleStateDst->getFieldAmount(!bHigh ? sfLowLimit : sfHighLimit);

            auto balance = sleRippleStateDst->getFieldAmount(sfBalance);
            auto furBalance = balance;
            if (furBalance.negative())
                furBalance.negate();

            forEachItem(ctx_.view(), amount.getIssuer(),
                [&](std::shared_ptr<SLE const> const& sle)
            {
                //for escrow
                if (sle->getType() == ltESCROW)
                {
                    auto amount1 = (*sle)[sfAmount];
                    if (amount1.getIssuer() == amount.getIssuer() && AccountID((*sle)[sfDestination]) == dest)
                    {
                        furBalance += amount1;
                    }
                    return;
                }
            });

            if (limit < amount + furBalance)
            {
                return temBAD_PATH;
            }
		}
    }

    // Create escrow in ledger
    auto const slep =
        std::make_shared<SLE>(keylet::escrow(account, (*sle)[sfSequence] - 1));
    (*slep)[sfAmount] = ctx_.tx[sfAmount];
    (*slep)[sfAccount] = account;
    (*slep)[~sfCondition] = ctx_.tx[~sfCondition];
    (*slep)[~sfSourceTag] = ctx_.tx[~sfSourceTag];
    (*slep)[sfDestination] = ctx_.tx[sfDestination];
    (*slep)[~sfCancelAfter] = ctx_.tx[~sfCancelAfter];
    (*slep)[~sfFinishAfter] = ctx_.tx[~sfFinishAfter];
    (*slep)[~sfDestinationTag] = ctx_.tx[~sfDestinationTag];

    ctx_.view().insert(slep);

    // Add escrow to sender's owner directory
    {
        auto page = dirAdd(
            ctx_.view(),
            keylet::ownerDir(account),
            slep->key(),
            false,
            describeOwnerDir(account),
            ctx_.app.journal("View"));
        if (!page)
            return tecDIR_FULL;
        (*slep)[sfOwnerNode] = *page;
    }

    // If it's not a self-send, add escrow to recipient's owner directory.
    if (auto const dest = ctx_.tx[sfDestination]; dest != ctx_.tx[sfAccount])
    {
        auto page = dirAdd(
            ctx_.view(),
            keylet::ownerDir(dest),
            slep->key(),
            false,
            describeOwnerDir(dest),
            ctx_.app.journal("View"));
        if (!page)
            return tecDIR_FULL;
        (*slep)[sfDestinationNode] = *page;
    }

	// add to issuer's owner directory
	if (amount.getIssuer() != account && amount.getIssuer() != ctx_.tx[sfDestination])
	{
		auto page = dirAdd(ctx_.view(), keylet::ownerDir(amount.getIssuer()), slep->key(),
			false, describeOwnerDir(amount.getIssuer()), ctx_.app.journal("View"));
		if (!page)
			return tecDIR_FULL;
		(*slep)[sfIssuerNode] = *page;
	}

    // Deduct owner's balance, increment owner count
	if (isZxc)
	{
		(*sle)[sfBalance] = (*sle)[sfBalance] - ctx_.tx[sfAmount];
	}		
	else if(account_ != amount.getIssuer())
	{
		SLE::pointer sleSrc = view().peek(
			keylet::line(account_, amount.getIssuer(), amount.getCurrency()));
		STAmount const& balance = (*sleSrc)[sfBalance];
		if(balance.negative())
			(*sleSrc)[sfBalance] = (*sleSrc)[sfBalance] + ctx_.tx[sfAmount];
		else
			(*sleSrc)[sfBalance] = (*sleSrc)[sfBalance] - ctx_.tx[sfAmount];
		
		ctx_.view().update(sleSrc);
	}

    adjustOwnerCount(ctx_.view(), sle, 1, ctx_.journal);
    ctx_.view().update(sle);

    return tesSUCCESS;
}

//------------------------------------------------------------------------------

static bool
checkCondition(Slice f, Slice c)
{
    using namespace ripple::cryptoconditions;

    std::error_code ec;

    auto condition = Condition::deserialize(c, ec);
    if (!condition)
        return false;

    auto fulfillment = Fulfillment::deserialize(f, ec);
    if (!fulfillment)
        return false;

    return validate(*fulfillment, *condition);
}

NotTEC
EscrowFinish::preflight(PreflightContext const& ctx)
{
    if (ctx.rules.enabled(fix1543) && ctx.tx.getFlags() & tfUniversalMask)
        return temINVALID_FLAG;

    if (ctx.rules.enabled(fix1543) && ctx.tx.getFlags() & tfUniversalMask)
        return temINVALID_FLAG;

    {
        auto const ret = preflight1(ctx);
        if (!isTesSuccess(ret))
            return ret;
    }

    auto const cb = ctx.tx[~sfCondition];
    auto const fb = ctx.tx[~sfFulfillment];

    // If you specify a condition, then you must also specify
    // a fulfillment.
    if (static_cast<bool>(cb) != static_cast<bool>(fb))
        return temMALFORMED;

    // Verify the transaction signature. If it doesn't work
    // then don't do any more work.
    {
        auto const ret = preflight2(ctx);
        if (!isTesSuccess(ret))
            return ret;
    }

    if (cb && fb)
    {
        auto& router = ctx.app.getHashRouter();

        auto const id = ctx.tx.getTransactionID();
        auto const flags = router.getFlags(id);

        // If we haven't checked the condition, check it
        // now. Whether it passes or not isn't important
        // in preflight.
        if (!(flags & (SF_CF_INVALID | SF_CF_VALID)))
        {
            if (checkCondition(*fb, *cb))
                router.setFlags(id, SF_CF_VALID);
            else
                router.setFlags(id, SF_CF_INVALID);
        }
    }

    return tesSUCCESS;
}

TER
EscrowFinish::preclaim(PreclaimContext const& ctx)
{
    auto const k = keylet::escrow(ctx.tx[sfOwner], ctx.tx[sfOfferSequence]);
    auto const slep = ctx.view.read(k);
    if (!slep)
    {
        return tecNO_TARGET;
    }

    AccountID const account = (*slep)[sfAccount];
    AccountID const& dest = (*slep)[sfDestination];
    return checkAuthority(ctx, account, lsfPaymentAuth, dest);
}

FeeUnit64
EscrowFinish::calculateBaseFee(ReadView const& view, STTx const& tx)
{
    FeeUnit64 extraFee{0};

    if (auto const fb = tx[~sfFulfillment])
    {
        extraFee +=
            safe_cast<FeeUnit64>(view.fees().units) * (32 + (fb->size() / 16));
    }

    return Transactor::calculateBaseFee(view, tx) + extraFee;
}

TER
EscrowFinish::doApply()
{
    auto const k = keylet::escrow(ctx_.tx[sfOwner], ctx_.tx[sfOfferSequence]);
    auto const slep = ctx_.view().peek(k);
	if (!slep)
	{
		return tecNO_TARGET;
	}

	AccountID const account = (*slep)[sfAccount];
	STAmount const& amount = (*slep)[sfAmount];
	AccountID const& dest = (*slep)[sfDestination];

	bool isZxc = isZXC(amount);

    //// If a cancel time is present, a finish operation should only succeed prior
    //// to that time. fix1571 corrects a logic error in the check that would make
    //// a finish only succeed strictly after the cancel time.
    //if (ctx_.view ().rules().enabled(fix1571))
    //{
    //    auto const now = ctx_.view().info().parentCloseTime;

    //    // Too soon: can't execute before the finish time
    //    if ((*slep)[~sfFinishAfter] && ! after(now, (*slep)[sfFinishAfter]))
    //        return tecNO_PERMISSION;

    //    // Too late: can't execute after the cancel time
    //    if ((*slep)[~sfCancelAfter] && after(now, (*slep)[sfCancelAfter]))
    //        return tecNO_PERMISSION;
    //}
    //else
    //{
		uint32_t lastConsensusTime = ctx_.app.getLedgerMaster().getLastConsensusTime();
        // Too soon?
        if ((*slep)[~sfFinishAfter] &&
			lastConsensusTime <=
            (*slep)[sfFinishAfter])
            return tecNO_PERMISSION;

        // Too late?
        if ((*slep)[~sfCancelAfter] &&
            (*slep)[sfCancelAfter] <=
			lastConsensusTime)
            return tecNO_PERMISSION;
    //}

    // Check cryptocondition fulfillment
    {
        auto const id = ctx_.tx.getTransactionID();
        auto flags = ctx_.app.getHashRouter().getFlags(id);

        auto const cb = ctx_.tx[~sfCondition];

        // It's unlikely that the results of the check will
        // expire from the hash router, but if it happens,
        // simply re-run the check.
        if (cb && !(flags & (SF_CF_INVALID | SF_CF_VALID)))
        {
            auto const fb = ctx_.tx[~sfFulfillment];

            if (!fb)
                return tecINTERNAL;

            if (checkCondition(*fb, *cb))
                flags = SF_CF_VALID;
            else
                flags = SF_CF_INVALID;

            ctx_.app.getHashRouter().setFlags(id, flags);
        }

        // If the check failed, then simply return an error
        // and don't look at anything else.
        if (flags & SF_CF_INVALID)
            return tecCRYPTOCONDITION_ERROR;

        // Check against condition in the ledger entry:
        auto const cond = (*slep)[~sfCondition];

        // If a condition wasn't specified during creation,
        // one shouldn't be included now.
        if (!cond && cb)
            return tecCRYPTOCONDITION_ERROR;

        // If a condition was specified during creation of
        // the suspended payment, the identical condition
        // must be presented again. We don't check if the
        // fulfillment matches the condition since we did
        // that in preflight.
        if (cond && (cond != cb))
            return tecCRYPTOCONDITION_ERROR;
    }

    // NOTE: Escrow payments cannot be used to fund accounts.
    AccountID const destID = (*slep)[sfDestination];
    auto const sled = ctx_.view().peek(keylet::account(destID));
    if (! sled)
        return tecNO_DST;

    if (ctx_.view().rules().enabled(featureDepositAuth))
    {
        // Is EscrowFinished authorized?
        if (sled->getFlags() & lsfDepositAuth)
        {
            // A destination account that requires authorization has two
            // ways to get an EscrowFinished into the account:
            //  1. If Account == Destination, or
            //  2. If Account is deposit preauthorized by destination.
            if (account_ != destID)
            {
                if (! view().exists (keylet::depositPreauth (destID, account_)))
                    return tecNO_PERMISSION;
            }
        }
    }

	// Remove escrow from owner directory
	{
		auto const page = (*slep)[sfOwnerNode];
		if (!ctx_.view().dirRemove(
			keylet::ownerDir(account), page, k.key, true))
		{
			return tefBAD_LEDGER;
		}
	}

	// Remove escrow from recipient's owner directory, if present.
	if (auto const optPage = (*slep)[~sfDestinationNode])
	{
		if (!ctx_.view().dirRemove(
			keylet::ownerDir(destID), *optPage, k.key, true))
		{
			return tefBAD_LEDGER;
		}
	}

	//Remove escrow from issuer's owner directory
	if (!isZxc)
	{
		AccountID const destination = (*slep)[sfDestination];
		if (amount.getIssuer() != account && amount.getIssuer() != destination)
		{
			auto const page = (*slep)[sfIssuerNode];
			if (!ctx_.view().dirRemove(keylet::ownerDir(amount.getIssuer()), page, k.key, true))
			{
				return tefBAD_LEDGER;
			}
		}
	}	

	// Fetch Destination SLE,transfer amount to destination
	if (isZxc)
	{
	    // Transfer amount to destination
	    (*sled)[sfBalance] = (*sled)[sfBalance] + (*slep)[sfAmount];

		ctx_.view().update(sled);
	}
	else
	{

		SLE::pointer sledl = ctx_.view().peek(
			keylet::line((*slep)[sfDestination], amount.getIssuer(), amount.getCurrency()));
		if (!sledl)
			return tecNO_LINE;

		bool const bHigh = dest > amount.getIssuer();
		auto limit = sledl->getFieldAmount(!bHigh ? sfLowLimit : sfHighLimit);
		auto balance = sledl->getFieldAmount(sfBalance);
		auto dstBalance = balance;
		if (dstBalance.negative())
			dstBalance.negate();
		if (limit < amount + dstBalance)
			return tecPATH_DRY;

        SLE::pointer sled = ctx_.view().peek(
            keylet::line((*slep)[sfDestination], amount.getIssuer(), amount.getCurrency()));
        if (!sled)
            return tecNO_LINE;

		// If the gateway has a transfer rate, accommodate that.
		Rate gatewayXferRate{ QUALITY_ONE };
		STAmount const& sendMax = amount;
		STAmount amountSend = amount;
		if (!sendMax.native() && (account_ != sendMax.getIssuer()))
		{
			gatewayXferRate = transferRate(ctx_.view(), sendMax.getIssuer());
			if (gatewayXferRate.value != QUALITY_ONE)
			{
				amountSend = divideRound(sendMax,
					gatewayXferRate, true);
			}
		}
		// adjust feeMin,feeMax
		std::string saFeeMax = transferFeeMax(ctx_.view(), amount.getIssuer());
		std::string saFeeMin = transferFeeMin(ctx_.view(), amount.getIssuer());
		STAmount feeMin = amountFromString(amount.issue(), saFeeMin);
		STAmount feeMax = amountFromString(amount.issue(), saFeeMax);
		STAmount fee = sendMax - amountSend;
		STAmount feeAct = std::min(feeMax, std::max(fee, feeMin));
		if (feeAct != fee)
			amountSend = sendMax - feeAct;
		
		//transfer amount to destination
		if(bHigh)
			(*sled)[sfBalance] = (*sled)[sfBalance] - amountSend;
		else
			(*sled)[sfBalance] = (*sled)[sfBalance] + amountSend;

		ctx_.view().update(sled);
	}

    // Adjust source owner count
    auto const sle = ctx_.view().peek(
        keylet::account(account));
    adjustOwnerCount(ctx_.view(), sle, -1, ctx_.journal);
    ctx_.view().update(sle);

    // Remove escrow from ledger
    ctx_.view().erase(slep);

    return tesSUCCESS;
}

//------------------------------------------------------------------------------

NotTEC
EscrowCancel::preflight(PreflightContext const& ctx)
{
    if (ctx.rules.enabled(fix1543) && ctx.tx.getFlags() & tfUniversalMask)
        return temINVALID_FLAG;

    auto const ret = preflight1(ctx);
    if (!isTesSuccess(ret))
        return ret;

    return preflight2(ctx);
}

TER
EscrowCancel::doApply()
{
    auto const k = keylet::escrow(ctx_.tx[sfOwner], ctx_.tx[sfOfferSequence]);
    auto const slep = ctx_.view().peek(k);
    if (!slep)
        return tecNO_TARGET;

    //if (ctx_.view().rules().enabled(fix1571))
    //{
    //    auto const now = ctx_.view().info().parentCloseTime;

    //    // No cancel time specified: can't execute at all.
    //    if (!(*slep)[~sfCancelAfter])
    //        return tecNO_PERMISSION;

    //    // Too soon: can't execute before the cancel time.
    //    if (!after(now, (*slep)[sfCancelAfter]))
    //        return tecNO_PERMISSION;
    //}
    //else
    //{
        // Too soon?
        if (!(*slep)[~sfCancelAfter] ||
			ctx_.app.getLedgerMaster().getLastConsensusTime() <=
                (*slep)[sfCancelAfter])
            return tecNO_PERMISSION;
    //}

    AccountID const account = (*slep)[sfAccount];
	STAmount const& amount = (*slep)[sfAmount]; 
	bool isZxc = isZXC(amount);

    // Remove escrow from owner directory
    {
        auto const page = (*slep)[sfOwnerNode];
        if (!ctx_.view().dirRemove(
                keylet::ownerDir(account), page, k.key, true))
        {
            return tefBAD_LEDGER;
        }
    }

    // Remove escrow from recipient's owner directory, if present.
    if (auto const optPage = (*slep)[~sfDestinationNode]; optPage)
    {
        if (!ctx_.view().dirRemove(
                keylet::ownerDir((*slep)[sfDestination]),
                *optPage,
                k.key,
                true))
        {
            return tefBAD_LEDGER;
        }
    }


	if (!isZxc)
	{
		//Remove escrow from issuer's owner directory
		AccountID const destination = (*slep)[sfDestination];
		if (amount.getIssuer() != account && amount.getIssuer() != destination)
		{
			auto const page = (*slep)[sfIssuerNode];
			if (!ctx_.view().dirRemove(keylet::ownerDir(amount.getIssuer()), page, k.key, true))
			{
				return tefBAD_LEDGER;
			}
		}
	}
	
    // Transfer amount back to owner, decrement owner count
	// Fetch Destination SLE,transfer amount to src
	if (isZxc)
	{
		SLE::pointer sled = ctx_.view().peek(
			keylet::account(account));
		(*sled)[sfBalance] = (*sled)[sfBalance] + (*slep)[sfAmount];
		ctx_.view().update(sled);
	}
	else if(account != amount.getIssuer())
	{
		SLE::pointer sled = view().peek(
			keylet::line(account, amount.getIssuer(), amount.getCurrency()));

		STAmount const& balance = (*sled)[sfBalance];
		bool const bHigh = account > amount.getIssuer();
		auto limit = sled->getFieldAmount(!bHigh ? sfLowLimit : sfHighLimit);
		if (bHigh)
		{
			limit.negate();
			if (limit > balance - amount)
				return tecPATH_DRY;
		}
		else
		{
			if (limit < balance + amount)
				return tecPATH_DRY;
		}

		if(bHigh)
			(*sled)[sfBalance] = (*sled)[sfBalance] - (*slep)[sfAmount];
		else
			(*sled)[sfBalance] = (*sled)[sfBalance] + (*slep)[sfAmount];
		ctx_.view().update(sled);
	}

	// Decrement owner count
	auto const sle = ctx_.view().peek(
		keylet::account(account));
    adjustOwnerCount(ctx_.view(), sle, -1, ctx_.journal);
    ctx_.view().update(sle);

    // Remove escrow from ledger
    ctx_.view().erase(slep);

    return tesSUCCESS;
}

}  // namespace ripple
