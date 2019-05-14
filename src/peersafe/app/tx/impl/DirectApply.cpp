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
#include <peersafe/app/tx/DirectApply.h>
#include <ripple/app/tx/impl/ApplyContext.h>
#include <ripple/app/tx/impl/CancelOffer.h>
#include <ripple/app/tx/impl/CancelTicket.h>
#include <ripple/app/tx/impl/Change.h>
#include <ripple/app/tx/impl/CreateOffer.h>
#include <ripple/app/tx/impl/CreateTicket.h>
#include <ripple/app/tx/impl/Escrow.h>
#include <ripple/app/tx/impl/Payment.h>
#include <ripple/app/tx/impl/SetAccount.h>
#include <ripple/app/tx/impl/SetRegularKey.h>
#include <ripple/app/tx/impl/SetSignerList.h>
#include <ripple/app/tx/impl/SetTrust.h>
#include <ripple/app/tx/impl/PayChan.h>
#include <peersafe/app/tx/TableListSet.h>
#include <peersafe/app/tx/SqlStatement.h>
#include <peersafe/app/tx/SqlTransaction.h>
#include <peersafe/app/tx/SmartContract.h>

namespace ripple {
	template<class T>
	static
		std::pair<TER, std::uint64_t>
		invoke_preclaim_direct(PreclaimContext const& ctx)
	{
		// If the transactor requires a valid account and the transaction doesn't
		// list one, preflight will have already a flagged a failure.
		auto const baseFee = T::calculateBaseFee(ctx);

		return{ T::preclaim(ctx), baseFee };
	}

	static
		std::pair<TER, std::uint64_t>
		invoke_preclaim_direct(PreclaimContext const& ctx)
	{
		switch (ctx.tx.getTxnType())
		{
		case ttPAYMENT:         return invoke_preclaim_direct<Payment>(ctx);
		case ttTABLELISTSET:	return invoke_preclaim_direct<TableListSet>(ctx);
		case ttSQLSTATEMENT:	return invoke_preclaim_direct<SqlStatement>(ctx);
		case ttSQLTRANSACTION:  return invoke_preclaim_direct<SqlTransaction>(ctx);
		case ttACCOUNT_SET:     return invoke_preclaim_direct<SetAccount>(ctx);
		case ttTRUST_SET:		return invoke_preclaim_direct<SetTrust>(ctx);
		default:
			assert(false);
			return{ temUNKNOWN, 0 };
		}
	}
	PreclaimResult
		preclaimDirect(PreflightResult const& preflightResult,
			Application& app, OpenView const& view)
	{
		boost::optional<PreclaimContext const> ctx;
		if (preflightResult.rules != view.rules())
		{
			auto secondFlight = preflight(app, view.rules(),
				preflightResult.tx, preflightResult.flags,
				preflightResult.j);
			ctx.emplace(app, view, secondFlight.ter, secondFlight.tx,
				secondFlight.flags, secondFlight.j);
		}
		else
		{
			ctx.emplace(
				app, view, preflightResult.ter, preflightResult.tx,
				preflightResult.flags, preflightResult.j);
		}
		try
		{
			if (ctx->preflightResult != tesSUCCESS)
				return{ *ctx, ctx->preflightResult, 0 };
			return{ *ctx, invoke_preclaim_direct(*ctx) };
		}
		catch (std::exception const& e)
		{
			JLOG(ctx->j.fatal()) <<
				"apply: " << e.what();
			return{ *ctx, tefEXCEPTION, 0 };
		}
	}

	static
		TER
		invoke_apply_direct(ApplyContext& ctx)
	{
		switch (ctx.tx.getTxnType())
		{
		case ttPAYMENT:			{ Payment        p(ctx); return p.applyDirect(); }
		case ttTABLELISTSET:	{ TableListSet   p(ctx); return p.applyDirect(); }
		case ttSQLSTATEMENT:	{ SqlStatement   p(ctx); return p.applyDirect(); }
		case ttSQLTRANSACTION:	{ SqlTransaction p(ctx); return p.applyDirect(); }
		case ttACCOUNT_SET:     { SetAccount     p(ctx); return p.applyDirect(); }
		case ttTRUST_SET:       { SetTrust       p(ctx); return p.applyDirect(); }
					
		default:
			assert(false);
			return temUNKNOWN;
		}
	}

	TER
		doApplyDirect(PreclaimResult const& preclaimResult,
			Application& app, ApplyView& view)
	{
		detail::ApplyStateTable table;
		if (preclaimResult.view.seq() != view.seq())
		{
			// Logic error from the caller. Don't have enough
			// info to recover.
			return tefEXCEPTION;
		}

		if (preclaimResult.ter != tesSUCCESS)
		{
			return preclaimResult.ter;
		}
			
		try
		{
			if (!preclaimResult.likelyToClaimFee)
				return preclaimResult.ter;
			ApplyContext ctx(app, view.openView(),
				preclaimResult.tx, preclaimResult.ter,
				preclaimResult.baseFee, preclaimResult.flags,
				preclaimResult.j);
			ApplyViewImpl& viewImpl = (ApplyViewImpl&)view;
			ApplyViewImpl& applyView = (ApplyViewImpl&)(ctx.view());
			
			//first fetch changes from outside view
			viewImpl.items().apply(applyView);

			//then apply tx directly
			TER ter = invoke_apply_direct(ctx);

			//finally apply changes to outside view
			viewImpl.items().clear();
			applyView.items().apply(viewImpl);
			return ter;
		}
		catch (std::exception const& e)
		{
			JLOG(preclaimResult.j.fatal()) <<
				"apply: " << e.what();
			return tefEXCEPTION;
		}
	}

	TER
		applyDirect(Application& app, ApplyView& view, STTx const& tx, beast::Journal j)
	{
		auto pfresult = preflight(app, view.openView().rules(), tx, view.flags()| tapNO_CHECK_SIGN, j);
		auto pcresult = preclaimDirect(pfresult, app, view.openView());
		auto ret = doApplyDirect(pcresult, app, view);
		return ret;
		//return doApplyDirect(pcresult, app, view);
	}
}