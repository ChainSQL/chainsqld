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
#include <ripple/basics/chrono.h>
#include <ripple/basics/Log.h>
#include <ripple/basics/safe_cast.h>
#include <ripple/conditions/Condition.h>
#include <ripple/conditions/Fulfillment.h>
#include <ripple/protocol/digest.h>
#include <ripple/protocol/st.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/TxFlags.h>
#include <ripple/protocol/ZXCAmount.h>
#include <ripple/ledger/View.h>
#include <ripple/app/paths/RippleState.h>
#include <ripple/protocol/Quality.h>

// During an EscrowFinish, the transaction must specify both
// a condition and a fulfillment. We track whether that
// fulfillment matches and validates the condition.
#define SF_CF_INVALID  SF_PRIVATE5
#define SF_CF_VALID    SF_PRIVATE6

namespace ripple {

/*
    Escrow allows an account holder to sequester any amount
    of ZXC in its own ledger entry, until the escrow process
    either finishes or is canceled.

    If the escrow process finishes successfully, then the
    destination account (which must exist) will receives the
    sequestered ZXC. If the escrow is, instead, canceled,
    the account which created the escrow will receive the
    sequestered ZXC back instead.

    EscrowCreate

        When an escrow is created, an optional condition may
        be attached. If present, that condition must be
        fulfilled for the escrow to successfully finish.

        At the time of creation, one or both of the fields
        sfCancelAfter and sfFinishAfter may be provided. If
        neither field is specified, the transaction is
        malformed.

        Since the escrow eventually becomes a payment, an
        optional DestinationTag and an optional SourceTag
        are supported in the EscrowCreate transaction.

        Validation rules:

            sfCondition
                If present, specifies a condition; the same
                condition along with its matching fulfillment
                are required during EscrowFinish.

            sfCancelAfter
                If present, escrow may be canceled after the
                specified time (seconds after the Ripple epoch).

            sfFinishAfter
                If present, must be prior to sfCancelAfter.
                A EscrowFinish succeeds only in ledgers after
                sfFinishAfter but before sfCancelAfter.

                If absent, same as parentCloseTime

            Malformed if both sfCancelAfter, sfFinishAfter
            are absent.

            Malformed if both sfFinishAfter, sfCancelAfter
            specified and sfCancelAfter <= sfFinishAfter

    EscrowFinish

        Any account may submit a EscrowFinish. If the escrow
        ledger entry specifies a condition, the EscrowFinish
        must provide the same condition and its associated
        fulfillment in the sfCondition and sfFulfillment
        fields, or else the EscrowFinish will fail.

        If the escrow ledger entry specifies sfFinishAfter, the
        transaction will fail if parentCloseTime <= sfFinishAfter.

        EscrowFinish transactions must be submitted before
        the escrow's sfCancelAfter if present.

        If the escrow ledger entry specifies sfCancelAfter, the
        transaction will fail if sfCancelAfter <= parentCloseTime.

        NOTE: The reason the condition must be specified again
              is because it must always be possible to verify
              the condition without retrieving the escrow
              ledger entry.

    EscrowCancel

        Any account may submit a EscrowCancel transaction.

        If the escrow ledger entry does not specify a
        sfCancelAfter, the cancel transaction will fail.

        If parentCloseTime <= sfCancelAfter, the transaction
        will fail.

        When a escrow is canceled, the funds are returned to
        the source account.

    By careful selection of fields in each transaction,
    these operations may be achieved:

        * Lock up ZXC for a time period
        * Execute a payment conditionally
*/

//------------------------------------------------------------------------------
/** Has the specified time passed?

	@param now  the current time
	@param mark the cutoff point
	@return true if \a now refers to a time strictly after \a mark, false otherwise.
*/
static inline bool after(NetClock::time_point now, std::uint32_t mark)
{
	return now.time_since_epoch().count() > mark;
}

ZXCAmount
EscrowCreate::calculateMaxSpend(STTx const& tx)
{
    return tx[sfAmount].zxc();
}

NotTEC
EscrowCreate::preflight (PreflightContext const& ctx)
{
    if (! ctx.rules.enabled(featureEscrow))
        return temDISABLED;

    if (ctx.rules.enabled(fix1543) && ctx.tx.getFlags() & tfUniversalMask)
        return temINVALID_FLAG;

    auto const ret = preflight1 (ctx);
    if (!isTesSuccess (ret))
        return ret;

    //if (! isZXC(ctx.tx[sfAmount]))
    //    return temBAD_AMOUNT;

    if (ctx.tx[sfAmount] <= beast::zero)
        return temBAD_AMOUNT;

    // We must specify at least one timeout value
    if (! ctx.tx[~sfCancelAfter] && ! ctx.tx[~sfFinishAfter])
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
        if (! ctx.tx[~sfFinishAfter] && ! ctx.tx[~sfCondition])
            return temMALFORMED;
    }

    if (auto const cb = ctx.tx[~sfCondition])
    {
        using namespace ripple::cryptoconditions;

        std::error_code ec;

        auto condition = Condition::deserialize(*cb, ec);
        if (!condition)
        {
            JLOG(ctx.j.debug()) <<
                "Malformed condition during escrow creation: " << ec.message();
            return temMALFORMED;
        }

        // Conditions other than PrefixSha256 require the
        // "CryptoConditionsSuite" amendment:
        if (condition->type != Type::preimageSha256 &&
                !ctx.rules.enabled(featureCryptoConditionsSuite))
            return temDISABLED;
    }

    return preflight2 (ctx);
}

TER
EscrowCreate::doApply()
{
    auto const closeTime = ctx_.view ().info ().parentCloseTime;

    // Prior to fix1571, the cancel and finish times could be greater
    // than or equal to the parent ledgers' close time.
    //
    // With fix1571, we require that they both be strictly greater
    // than the parent ledgers' close time.
    if (ctx_.view ().rules().enabled(fix1571))
    {
        if (ctx_.tx[~sfCancelAfter] && after(closeTime, ctx_.tx[sfCancelAfter]))
            return tecNO_PERMISSION;

        if (ctx_.tx[~sfFinishAfter] && after(closeTime, ctx_.tx[sfFinishAfter]))
            return tecNO_PERMISSION;
    }
    else
    {
        if (ctx_.tx[~sfCancelAfter])
        {
            auto const cancelAfter = ctx_.tx[sfCancelAfter];

            if (closeTime.time_since_epoch().count() >= cancelAfter)
                return tecNO_PERMISSION;
        }

        if (ctx_.tx[~sfFinishAfter])
        {
            auto const finishAfter = ctx_.tx[sfFinishAfter];

            if (closeTime.time_since_epoch().count() >= finishAfter)
                return tecNO_PERMISSION;
        }
    }

    auto const account = ctx_.tx[sfAccount];
    auto const sle = ctx_.view().peek(
        keylet::account(account));


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
        auto const sled = ctx_.view().read(
            keylet::account(dest));
        if (! sled)
            return tecNO_DST;
        if (((*sled)[sfFlags] & lsfRequireDestTag) &&
                ! ctx_.tx[~sfDestinationTag])
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
			if (limit < amount)
				return temBAD_PATH;
		}
    }

    // Create escrow in ledger
    auto const slep = std::make_shared<SLE>(
        keylet::escrow(account, (*sle)[sfSequence] - 1));
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
        auto page = dirAdd(ctx_.view(), keylet::ownerDir(account), slep->key(),
            false, describeOwnerDir(account), ctx_.app.journal ("View"));
        if (!page)
            return tecDIR_FULL;
        (*slep)[sfOwnerNode] = *page;
    }

    // If it's not a self-send, add escrow to recipient's owner directory.
    if (ctx_.view ().rules().enabled(fix1523))
    {
        auto const dest = ctx_.tx[sfDestination];

        if (dest != ctx_.tx[sfAccount])
        {
            auto page = dirAdd(ctx_.view(), keylet::ownerDir(dest), slep->key(),
                false, describeOwnerDir(dest), ctx_.app.journal ("View"));
            if (!page)
                return tecDIR_FULL;
            (*slep)[sfDestinationNode] = *page;
        }
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

static
bool
checkCondition (Slice f, Slice c)
{
    using namespace ripple::cryptoconditions;

    std::error_code ec;

    auto condition = Condition::deserialize(c, ec);
    if (!condition)
        return false;

    auto fulfillment = Fulfillment::deserialize(f, ec);
    if (!fulfillment)
        return false;

    return validate (*fulfillment, *condition);
}

NotTEC
EscrowFinish::preflight (PreflightContext const& ctx)
{
    if (! ctx.rules.enabled(featureEscrow))
        return temDISABLED;

    if (ctx.rules.enabled(fix1543) && ctx.tx.getFlags() & tfUniversalMask)
        return temINVALID_FLAG;

    {
        auto const ret = preflight1 (ctx);
        if (!isTesSuccess (ret))
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
        auto const ret = preflight2 (ctx);
        if (!isTesSuccess (ret))
            return ret;
    }

    if (cb && fb)
    {
        auto& router = ctx.app.getHashRouter();

        auto const id = ctx.tx.getTransactionID();
        auto const flags = router.getFlags (id);

        // If we haven't checked the condition, check it
        // now. Whether it passes or not isn't important
        // in preflight.
        if (!(flags & (SF_CF_INVALID | SF_CF_VALID)))
        {
            if (checkCondition (*fb, *cb))
                router.setFlags (id, SF_CF_VALID);
            else
                router.setFlags (id, SF_CF_INVALID);
        }
    }

    return tesSUCCESS;
}

std::uint64_t
EscrowFinish::calculateBaseFee (
    ReadView const& view,
    STTx const& tx)
{
    std::uint64_t extraFee = 0;

    if (auto const fb = tx[~sfFulfillment])
    {
        extraFee += view.fees().units *
            (32 + safe_cast<std::uint64_t> (fb->size() / 16));
    }

    return Transactor::calculateBaseFee (view, tx) + extraFee;
}

TER
EscrowFinish::doApply()
{
    auto const k = keylet::escrow(
        ctx_.tx[sfOwner], ctx_.tx[sfOfferSequence]);
    auto const slep = ctx_.view().peek(k);
    if (! slep)
        return tecNO_TARGET;

	AccountID const account = (*slep)[sfAccount];
	STAmount const& amount = (*slep)[sfAmount];

    // If a cancel time is present, a finish operation should only succeed prior
    // to that time. fix1571 corrects a logic error in the check that would make
    // a finish only succeed strictly after the cancel time.
    if (ctx_.view ().rules().enabled(fix1571))
    {
        auto const now = ctx_.view().info().parentCloseTime;

        // Too soon: can't execute before the finish time
        if ((*slep)[~sfFinishAfter] && ! after(now, (*slep)[sfFinishAfter]))
            return tecNO_PERMISSION;

        // Too late: can't execute after the cancel time
        if ((*slep)[~sfCancelAfter] && after(now, (*slep)[sfCancelAfter]))
            return tecNO_PERMISSION;
    }
    else
    {
        // Too soon?
        if ((*slep)[~sfFinishAfter] &&
            ctx_.view().info().parentCloseTime.time_since_epoch().count() <=
            (*slep)[sfFinishAfter])
            return tecNO_PERMISSION;

        // Too late?
        if ((*slep)[~sfCancelAfter] &&
            ctx_.view().info().parentCloseTime.time_since_epoch().count() <=
            (*slep)[sfCancelAfter])
            return tecNO_PERMISSION;
    }

    // Check cryptocondition fulfillment
    {
        auto const id = ctx_.tx.getTransactionID();
        auto flags = ctx_.app.getHashRouter().getFlags (id);

        auto const cb = ctx_.tx[~sfCondition];

        // It's unlikely that the results of the check will
        // expire from the hash router, but if it happens,
        // simply re-run the check.
        if (cb && ! (flags & (SF_CF_INVALID | SF_CF_VALID)))
        {
            auto const fb = ctx_.tx[~sfFulfillment];

            if (!fb)
                return tecINTERNAL;

            if (checkCondition (*fb, *cb))
                flags = SF_CF_VALID;
            else
                flags = SF_CF_INVALID;

            ctx_.app.getHashRouter().setFlags (id, flags);
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

    // NOTE: These payments cannot be used to fund accounts
	bool isZxc = isZXC(amount);

	AccountID const& dest = (*slep)[sfDestination];
	// Fetch Destination SLE,transfer amount to destination
	if (isZxc)
	{
	    // Remove escrow from owner directory
	    {
	        auto const page = (*slep)[sfOwnerNode];
	        if (! ctx_.view().dirRemove(
	                keylet::ownerDir(account), page, k.key, true))
	        {
	            return tefBAD_LEDGER;
	        }
	    }

	    // Remove escrow from recipient's owner directory, if present.
	    if (ctx_.view ().rules().enabled(fix1523) && (*slep)[~sfDestinationNode])
	    {
	        auto const page = (*slep)[sfDestinationNode];
	        if (! ctx_.view().dirRemove(keylet::ownerDir(destID), page, k.key, true))
	        {
	            return tefBAD_LEDGER;
	        }
	    }

	    // Transfer amount to destination
	    (*sled)[sfBalance] = (*sled)[sfBalance] + (*slep)[sfAmount];

		ctx_.view().update(sled);
	}
	else
	{
		SLE::pointer sled = view().peek(
			keylet::line((*slep)[sfDestination], amount.getIssuer(), amount.getCurrency()));
		if (!sled)
			return tecNO_LINE;

		bool const bHigh = dest > amount.getIssuer();
		auto limit = sled->getFieldAmount(!bHigh ? sfLowLimit : sfHighLimit);
		if (limit < amount)
			return tecPATH_DRY;

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
EscrowCancel::preflight (PreflightContext const& ctx)
{
    if (! ctx.rules.enabled(featureEscrow))
        return temDISABLED;

    if (ctx.rules.enabled(fix1543) && ctx.tx.getFlags() & tfUniversalMask)
        return temINVALID_FLAG;

    auto const ret = preflight1 (ctx);
    if (!isTesSuccess (ret))
        return ret;

    return preflight2 (ctx);
}

TER
EscrowCancel::doApply()
{
    auto const k = keylet::escrow(ctx_.tx[sfOwner], ctx_.tx[sfOfferSequence]);
    auto const slep = ctx_.view().peek(k);
    if (! slep)
        return tecNO_TARGET;

    if (ctx_.view ().rules().enabled(fix1571))
    {
        auto const now = ctx_.view().info().parentCloseTime;

        // No cancel time specified: can't execute at all.
        if (! (*slep)[~sfCancelAfter])
            return tecNO_PERMISSION;

        // Too soon: can't execute before the cancel time.
        if (! after(now, (*slep)[sfCancelAfter]))
            return tecNO_PERMISSION;
    }
    else
    {
        // Too soon?
        if (!(*slep)[~sfCancelAfter] ||
            ctx_.view().info().parentCloseTime.time_since_epoch().count() <=
            (*slep)[sfCancelAfter])
            return tecNO_PERMISSION;
    }

    AccountID const account = (*slep)[sfAccount];
	STAmount const& amount = (*slep)[sfAmount];

    // Remove escrow from owner directory
    {
        auto const page = (*slep)[sfOwnerNode];
        if (! ctx_.view().dirRemove(
                keylet::ownerDir(account), page, k.key, true))
        {
            return tefBAD_LEDGER;
        }
    }

    // Remove escrow from recipient's owner directory, if present.
    if (ctx_.view ().rules().enabled(fix1523) && (*slep)[~sfDestinationNode])
    {
        auto const page = (*slep)[sfDestinationNode];
        if (! ctx_.view().dirRemove(
                keylet::ownerDir((*slep)[sfDestination]), page, k.key, true))
        {
            return tefBAD_LEDGER;
        }
    }
	
    // Transfer amount back to owner, decrement owner count
	bool isZxc = isZXC(amount);
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

} // ripple

