//------------------------------------------------------------------------------
/*
 This file is part of chainsqld: https://github.com/chainsql/chainsqld
 Copyright (c) 2016-2018 Peersafe Technology Co., Ltd.
 
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

#ifndef RIPPLE_TX_SQLSTATEMENTTXS_H_INCLUDED
#define RIPPLE_TX_SQLSTATEMENTTXS_H_INCLUDED

#include <ripple/app/paths/RippleCalc.h>
#include <ripple/app/tx/impl/Transactor.h>
#include <ripple/basics/Log.h>
#include <ripple/protocol/TxFlags.h>
#include <peersafe/app/tx/ChainSqlTx.h>

namespace ripple {


class SqlTransaction
    : public ChainSqlTx
{
private:
    ripple::TER handleEachTx(ApplyContext& ctx);
    std::pair<TER, std::string> transactionImpl(ApplyContext& ctx_,ripple::TxStoreDBConn &txStoreDBConn, ripple::TxStore& txStore, beast::Journal journal, const STTx &tx);
	std::pair<TER, bool> transactionImpl_FstStorage(ApplyContext& ctx_, ripple::TxStore& txStore, TxID txID, beast::Journal journal, const std::vector<STTx> &txs);

public:
    SqlTransaction(ApplyContext& ctx)
        : ChainSqlTx(ctx)
    {
    }

    static
    ZXCAmount
    calculateMaxSpend(STTx const& tx);

    static
    TER
    preflight (PreflightContext const& ctx);

    static
    TER
    preclaim(PreclaimContext const& ctx);

    TER doApply () override;

	std::pair<TER, std::string> dispose(TxStore& txStore, const STTx& tx);
};

} // ripple

#endif
