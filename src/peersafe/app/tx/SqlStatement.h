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

#ifndef RIPPLE_TX_SQLSTATEMENT_H_INCLUDED
#define RIPPLE_TX_SQLSTATEMENT_H_INCLUDED

#include <ripple/app/paths/RippleCalc.h>
#include <ripple/app/tx/impl/Transactor.h>
#include <ripple/basics/Log.h>
#include <ripple/protocol/TxFlags.h>
#include <peersafe/app/tx/ChainSqlTx.h>

namespace ripple {

// See https://ripple.com/wiki/Transaction_Format#Payment_.280.29

class SqlStatement
    : public ChainSqlTx
{
    /* The largest number of paths we allow */
    static std::size_t const MaxPathSize = 6;

    /* The longest path we allow */
    static std::size_t const MaxPathLength = 8;

public:
    SqlStatement(ApplyContext& ctx)
        : ChainSqlTx(ctx)
    {
    }

    static
    ZXCAmount
    calculateMaxSpend(STTx const& tx);

    static
    TER
    preflightHandler(const STTx & tx, Application& app);
    static
    TER
    preflight (PreflightContext const& ctx);

    static
    TER
    preclaimHandler(ReadView const& view, const STTx & tx, Application& app);
    static
    TER
    preclaim(PreclaimContext const& ctx);

	static
    TER
    applyHandler(ApplyView& view, const STTx & tx, Application& app);
    TER doApply () override;


	TER dealWithOperationRule(const STTx & tx,STEntry* pEntry);

	TER adjustInsertCount(const STTx & tx, DatabaseCon* pConn);

	std::string getOperationRule(ApplyView& view, const STTx& tx);
	
	TER preApply(const STTx & tx);

	std::pair<TER, std::string> dispose(TxStore& txStore, const STTx& tx);
};

} // ripple

#endif
