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


#include <peersafe/app/tx/SmartContract.h>
#include <ripple/basics/Log.h>
#include <ripple/core/Config.h>
#include <ripple/protocol/st.h>
#include <ripple/protocol/TxFlags.h>
#include <ripple/protocol/jss.h>
#include <peersafe/app/misc/Executive.h>
#include <peersafe/core/Tuning.h>
#include <peersafe/protocol/ContractDefines.h>
#include <ripple/app/misc/LoadFeeTrack.h>
#include <peersafe/app/ledger/LedgerAdjust.h>

namespace ripple {

	NotTEC SmartContract::preflight(PreflightContext const& ctx)
	{
		auto const ret = preflight1(ctx);
		if (!isTesSuccess(ret))
			return ret;
		auto& tx = ctx.tx;

		if (!tx.isFieldPresent(sfContractData))
		{
			return temMALFORMED;
		}

		if (tx.isFieldPresent(sfContractValue) && tx.getFieldAmount(sfContractValue).zxc().drops() < 0)
		{
			return temBAD_AMOUNT;
		}

		return preflight2(ctx);
	}

	TER SmartContract::preclaim(PreclaimContext const& ctx)
	{
        auto& tx = ctx.tx;
        AccountID srcAcc = tx.getAccountID(sfAccount);
        auto const k = keylet::account(srcAcc);
        auto const sle = ctx.view.read(k);
        if (tx.getFieldU16(sfContractOpType) == ContractCreation)
        {
            if (auto res =
                    checkAuthority(ctx, srcAcc, lsfDeployContractAuth);
                res != tesSUCCESS)
                return res;
        }

        if (tx.isFieldPresent(sfContractValue) &&
            tx.getFieldAmount(sfContractValue).zxc().drops() > 0)
        {
            if (auto res = checkAuthority(ctx, srcAcc, lsfPaymentAuth);
                res != tesSUCCESS)
                return res;
        }

        if (!isContractTypeValid((ContractOpType)tx.getFieldU16(sfContractOpType)))
            return temBAD_OPTYPE;
		if (tx.getFieldVL(sfContractData).size() == 0)
		{
			//empty contract_data not valid if deploy or pay to contract 0
			if (tx.getFieldU16(sfContractOpType) == 1 ||
				(tx.getFieldU16(sfContractOpType) != 1 && (!tx.isFieldPresent(sfContractValue) ||
				(tx.isFieldPresent(sfContractValue) && tx.getFieldAmount(sfContractValue).zxc().drops() == 0))))
			{
				return temMALFORMED;
			}
		}			

		
		// Avoid unaffordable transactions.
        int64_t gas_price = ctx.view.fees().gas_price;
		int64_t gas = tx.getFieldU32(sfGas);
        int64_t gasCost = int64_t(gas * gas_price);
		int64_t value = tx.getFieldAmount(sfContractValue).zxc().drops();
		int64_t totalCost = value + gasCost;
		
		if (gasCost < 0 || totalCost < 0)
		{
			return temMALFORMED;
		}

		auto balance = sle->getFieldAmount(sfBalance).zxc().drops();
		if (balance < totalCost)
		{
            JLOG(ctx.j.trace())
                << "Not enough zxc: Require >" << totalCost << "="
                << gas << "*" << gas_price << "+" << value << " Got"
                << balance
                << "for sender: " << tx.getAccountID(sfAccount);
            return tecINSUFF_FEE;
		}
		return tesSUCCESS;
	}

	TER SmartContract::doApply()
	{
		SleOps ops(ctx_);
		auto pInfo = std::make_shared<EnvInfoImpl>(ctx_.view().info().seq, 210000, 
                ctx_.view().fees().drops_per_byte, ctx_.app.getPreContractFace());
		Executive e(ops, *pInfo, INITIAL_DEPTH);
		e.initialize();
		if (!e.execute())
		{
			e.go();
			if (e.getException() != tesSUCCESS)
			{
				std::string errMsg = e.takeOutput().toString();
				JLOG(ctx_.journal.warn()) << "SmartContract exception:"<< errMsg;
				setExtraMsg(errMsg);
			}	
			if(ctx_.tx.getFieldU16(sfContractOpType) != ContractCreation)
				LedgerAdjust::updateContractCount(ctx_.app, ctx_.view(),CONTRACT_CALL);
			return e.finalize();
		}			
		else
			return e.getException();
	}
}