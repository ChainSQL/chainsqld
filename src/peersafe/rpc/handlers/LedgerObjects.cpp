//------------------------------------------------------------------------------
/*
	This file is part of rippled: https://github.com/ripple/rippled
	Copyright (c) 2012-2014 Ripple Labs Inc.

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

#include <ripple/app/ledger/LedgerToJson.h>
#include <ripple/ledger/ReadView.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/jss.h>
#include <ripple/protocol/LedgerFormats.h>
#include <ripple/rpc/impl/RPCHelpers.h>
#include <ripple/rpc/impl/Tuning.h>
#include <ripple/rpc/Context.h>
#include <ripple/rpc/Role.h>

namespace ripple {

	// Get all types of node count  from a ledger
	//   Inputs:
	//		ledger_hash :  <ledger>
	//		ledger_index : <ledger_index>
	//   Outputs:

	Json::Value doLedgerObjects(RPC::JsonContext& context)
	{
		std::shared_ptr<ReadView const> lpLedger;

		auto jvResult = RPC::lookupLedger(lpLedger, context);
		if (!lpLedger)
			return jvResult;

		jvResult[jss::ledger_hash] = to_string(lpLedger->info().hash);
		jvResult[jss::ledger_index] = lpLedger->info().seq;

		Json::Value& nodes = jvResult[jss::state];

		std::map< LedgerEntryType, uint32_t> mapCount;
		for (auto sle:lpLedger->sles)
		{
		//	auto sle = //lpLedger->read(keylet::unchecked((*i)->key()));
			mapCount[sle->getType()] ++;
		}

		nodes[jss::account] = mapCount[ltACCOUNT_ROOT];
		nodes[jss::amendments] = mapCount[ltAMENDMENTS];
		nodes[jss::directory] = mapCount[ltDIR_NODE];
		nodes[jss::fee] = mapCount[ltFEE_SETTINGS];
		nodes[jss::hashes] = mapCount[ltLEDGER_HASHES];
		nodes[jss::offer] = mapCount[ltOFFER];
		nodes[jss::signer_list] = mapCount[ltSIGNER_LIST];
		nodes[jss::state] = mapCount[ltRIPPLE_STATE];
		nodes[jss::escrow] = mapCount[ltESCROW];
		nodes[jss::ticket] = mapCount[ltTICKET];
		nodes[jss::payment_channel] = mapCount[ltPAYCHAN];
		nodes[jss::table] = mapCount[ltTABLE];
        nodes[jss::tablelist] = mapCount[ltTABLELIST];
		nodes[jss::schema] = mapCount[ltSCHEMA];
		nodes[jss::Statis] = mapCount[ltSTATIS];
        nodes[jss::TableGrant] = mapCount[ltTABLEGRANT];

		int txCount = 0;
		for (auto const& tx : lpLedger->txs)
		{
            (void)tx;
			txCount++;
		}
		jvResult[jss::tx] = txCount;

		//auto j = context.app.journal("RPCHandler");
		//JLOG(j.info())<<"ledger_objects:"<<jvResult.toStyledString();

		return jvResult;
	}

} // ripple
