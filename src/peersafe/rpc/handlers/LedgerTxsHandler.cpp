//------------------------------------------------------------------------------
/*
	This file is part of chainsqld: https://github.com/chainsql/chainsqld
	Copyright (c) 2019 Peersafe Technology Co., Ltd.

	chainsqld is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	chainsqld is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
//==============================================================================

#include <BeastConfig.h>
#include <ripple/rpc/handlers/LedgerHandler.h>
#include <ripple/app/ledger/LedgerToJson.h>
#include <ripple/app/main/Application.h>
#include <ripple/app/misc/LoadFeeTrack.h>
#include <ripple/json/Object.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/JsonFields.h>
#include <ripple/resource/Fees.h>
#include <ripple/rpc/impl/RPCHelpers.h>
#include <ripple/rpc/Role.h>

namespace ripple {


	Json::Value doLedgerTxs(RPC::Context& context)
	{

		Json::Value jvResult;

		auto const& params = context.params;
		bool needsLedger = params.isMember(jss::ledger) ||
			params.isMember(jss::ledger_hash) ||
			params.isMember(jss::ledger_index);
		if (!needsLedger)
			return RPC::missing_field_error(jss::ledger);

		std::shared_ptr<ReadView const> ledger;
		auto result = RPC::lookupLedger(ledger, context);

		if (!ledger)
			return result;

		bool const bSuccessDetail  = params[jss::include_success].asBool();
		bool const bFailtureDetail = params[jss::include_failure].asBool();

		int iSuccess = 0;
		int iFailure = 0;

		Json::Value arrFailureDetail(Json::arrayValue);
		Json::Value arrSuccessDetail(Json::arrayValue);

		beast::Journal  j_ = context.app.logs().journal("LedgerTxHandler");

		for (auto& item : ledger->txs){
		
			std::shared_ptr<TxMeta> meta = std::make_shared<TxMeta>(
				item.first->getTransactionID(), ledger->seq(), *(item.second), j_);

			TER result = meta->getResultTER();

			Json::Value txItem;
			txItem[jss::hash] = to_string(item.first->getTransactionID());

			std::string token, human;
			if (transResultInfo(result, token, human)) {
				txItem[jss::transaction_result] = token;
			}
			else {
				JLOG(j_.error())
					<< "Unknown result code in metadata: : " << result;
			}

			if (result == tesSUCCESS) {

				arrSuccessDetail.append(txItem);
				iSuccess++;
			}		
			else {

				arrFailureDetail.append(txItem);
				iFailure++;
			}
						
		}

		jvResult[jss::txn_success]   = Json::UInt(iSuccess);
		jvResult[jss::txn_failure]   = Json::UInt(iFailure);
		jvResult[jss::ledger_index]  = ledger->seq();
	
		if (bFailtureDetail) {
			jvResult[jss::txn_failure_detail] = arrFailureDetail;
		}

		if (bSuccessDetail) {
			jvResult[jss::txn_success_detail] = arrSuccessDetail;
		}

		return jvResult;
	}

} // ripple
