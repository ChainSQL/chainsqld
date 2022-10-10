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

#ifndef RIPPLE_TX_TABLELISTSET_H_INCLUDED
#define RIPPLE_TX_TABLELISTSET_H_INCLUDED

#include <ripple/app/paths/RippleCalc.h>
#include <ripple/app/tx/impl/Transactor.h>
#include <ripple/basics/Log.h>
#include <ripple/protocol/TxFlags.h>
#include <peersafe/app/tx/ChainSqlTx.h>

namespace ripple {


class TableListSet
    : public ChainSqlTx
{
public:
    TableListSet(ApplyContext& ctx)
        : ChainSqlTx(ctx)
    {
    }

    static
	NotTEC
    preflightHandler(const STTx & tx, Schema& app);
    static
	NotTEC
    preflight (PreflightContext const& ctx);

    static
    TER
    preclaimHandler(ReadView const& view, const STTx & tx, Schema& app);
    static
    TER
    preclaim(PreclaimContext const& ctx);

	static
    TER
    applyHandler(ApplyView& view, const STTx & tx, Schema& app);
    TER doApply () override;

    static void
    prepareTableEntry(const STTx& tx, ApplyView& view, STObject* pEntry);

    static
    void UpdateTableSle(
        STObject *pEntry, 
        LedgerIndex createLgrSeq, 
        uint256 createdLedgerHash, 
        uint256 createdTxnHash,
        LedgerIndex previousTxnLgrSeq = 0,
        uint256 previousTxnLgrHash = uint256());

	std::pair<TER, std::string> dispose(TxStore& txStore, const STTx& tx) override;

private:
    static
    void updateOperationRules(
        STObject* pEntry, 
        const STTx& tx);
    static
    bool updateOperationRuleForInserting(
        const std::string& fieldName, 
        STObject* pEntry, 
        Json::Value& rule);
    static
    bool updateOperationRuleForUpdating(
        const std::string& fieldName, 
        STObject* pEntry, 
        Json::Value& rule);
    static
    bool updateOperationRuleForDeletingOrGet(
        const std::string& fieldName, 
        STObject* pEntry,
        int opType, 
        Json::Value& rule);
};

} // ripple

#endif
