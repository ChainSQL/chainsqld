#include <peersafe/app/tx/SchemaTx.h>
#include <ripple/protocol/STTx.h>
#include <ripple/ledger/View.h>
#include <ripple/protocol/st.h>

namespace ripple {
	NotTEC SchemaCreate::preflight(PreflightContext const& ctx)
	{
		auto const ret = preflight1(ctx);
		if (!isTesSuccess(ret))
			return ret;

		if( !ctx.tx.isFieldPresent(sfSchemaName) ||
			!ctx.tx.isFieldPresent(sfSchemaStrategy) ||
			!ctx.tx.isFieldPresent(sfValidators) ||
			!ctx.tx.isFieldPresent(sfPeerList))
			return temMALFORMED;

		return preflight2(ctx);
	}

	TER SchemaCreate::preclaim(PreclaimContext const& ctx)
	{
		return tesSUCCESS;
	}

	TER SchemaCreate::doApply()
	{
		auto const account = ctx_.tx[sfAccount];
		auto const sle = ctx_.view().peek(
			keylet::account(account));
		// Create escrow in ledger
		auto const slep = std::make_shared<SLE>(
			keylet::schema(account, (*sle)[sfSequence] - 1,ctx_.view().info().parentHash));
		(*slep)[sfAccount] = account;
		(*slep)[sfSchemaName] = ctx_.tx[sfSchemaName];
		(*slep)[sfSchemaStrategy] = ctx_.tx[sfSchemaStrategy];
		(*slep)[~sfSchemaAdmin] = ctx_.tx[~sfSchemaAdmin];
		(*slep)[~sfAnchorLedgerHash] = ctx_.tx[~sfAnchorLedgerHash];

		STArray const& validators = ctx_.tx.getFieldArray(sfValidators);
		slep->setFieldArray(sfValidators, validators);
		STArray const& peerList = ctx_.tx.getFieldArray(sfPeerList);
		slep->setFieldArray(sfPeerList, peerList);

		// Multi-Sign
		if (ctx_.tx.getSigningPubKey().empty())
		{
			// ref:Transactor::checkMultiSign
			
		}

		ctx_.view().insert(slep);

		// Add schema to sender's owner directory
		{
			auto page = dirAdd(ctx_.view(), keylet::ownerDir(account), slep->key(),
				false, describeOwnerDir(account), ctx_.app.journal("View"));
			if (!page)
				return tecDIR_FULL;
			(*slep)[sfOwnerNode] = *page;
		}
		adjustOwnerCount(ctx_.view(), sle, 1, ctx_.journal);

		return tesSUCCESS;
	}

	//------------------------------------------------------------------------------

	NotTEC SchemaModify::preflight(PreflightContext const& ctx)
	{
		return tesSUCCESS;
	}

	TER SchemaModify::preclaim(PreclaimContext const& ctx)
	{
		return tesSUCCESS;
	}

	TER SchemaModify::doApply()
	{

		return tesSUCCESS;
	}
}
