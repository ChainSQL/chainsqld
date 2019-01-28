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

#include <peersafe/app/table/TableTxAccumulator.h>
#include <ripple/protocol/STTx.h>
#include <ripple/app/ledger/TransactionMaster.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/main/Application.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <peersafe/core/Tuning.h>

namespace ripple {

	TableTxAccumulator::TableTxAccumulator(Application& app)
		:app_(app)
	{
		sweepingThread_ = false;
	}

	//ignore the case that tx swept and come again
	void TableTxAccumulator::onSubtxResponse(STTx const& tx, AccountID const& owner, std::string tableName, int subTxCount,std::pair<bool, std::string> result)
	{
		if (!result.first)
		{
			pubTableTxError(tx, owner,tableName, result.second);
			return;
		}

		//Unit chainsql-tx,send db_success immediately
		if (tx.getTxnType() != ttCONTRACT && tx.getTxnType() != ttSQLTRANSACTION)
		{
			pubTableTxSuccess(tx,owner,tableName);
			return;
		}

		if (mapTxAccumulator_.find(tx.getTransactionID()) == mapTxAccumulator_.end())
		{
			auto txsAll = app_.getMasterTransaction().getTxs(tx);
			SubTxInfo info;
			info.numSuccess = subTxCount;
			info.numSubTxs = txsAll.size();
			info.setTables.emplace(std::make_pair(owner,tableName));
			info.startLedgerSeq = app_.getLedgerMaster().getValidLedgerIndex();

			std::lock_guard<std::mutex> lock(mutexTxCache_);
			mapTxAccumulator_[tx.getTransactionID()] = info;
		}
		else
		{
			auto& info = mapTxAccumulator_[tx.getTransactionID()];
			info.setTables.emplace(std::make_pair(owner, tableName));
			info.numSuccess += subTxCount;
		}
		checkTxResult(tx);
	}

	void TableTxAccumulator::checkTxResult(STTx const& tx)
	{
		//check to send db_success.
		auto& info = mapTxAccumulator_[tx.getTransactionID()];
		if (info.numSuccess == info.numSubTxs)
		{
			auto& tmpSet = mapTxAccumulator_[tx.getTransactionID()].setTables;
			for (auto iter = tmpSet.begin(); iter != tmpSet.end(); iter++)
			{
				pubTableTxSuccess(tx,iter->first,iter->second);
			}		

			clearCache(tx.getTransactionID());
		}
	}

	void TableTxAccumulator::trySweepCache()
	{
		if (!sweepingThread_ && (mapTxAccumulator_.size() > 0))
		{
			sweepingThread_ = true;
			app_.getJobQueue().addJob(jtCheckSubTx, "TableTxAccumulator.sweepCache",
				[this](Job&) { sweepCache(); });
		}
	}

	void TableTxAccumulator::sweepCache()
	{
		std::lock_guard<std::mutex> lock(mutexTxCache_);
		auto iter = mapTxAccumulator_.begin(); 
		while (iter != mapTxAccumulator_.end())
		{
			if (app_.getLedgerMaster().getValidLedgerIndex() > iter->second.startLedgerSeq + LAST_LEDGERSEQ_PASS)
			{
				iter = mapTxAccumulator_.erase(iter);
			}
			else
			{
				iter++;
			}
		}
		
		sweepingThread_ = false;
	}

	void TableTxAccumulator::pubTableTxSuccess(STTx const& tx, AccountID const& owner, std::string tableName)
	{
		std::pair <std::string, std::string> result = std::make_pair("db_success", "");	
		app_.getOPs().pubTableTxs(owner, tableName, tx, result, false);
	}

	void TableTxAccumulator::pubTableTxError(STTx const& tx, AccountID const& owner,
		std::string tableName, std::string err_msg)
	{
		std::pair <std::string, std::string> result = std::make_pair("db_error", err_msg);
		app_.getOPs().pubTableTxs(owner, tableName, tx, result, false);
		clearCache(tx.getTransactionID());
	}

	void TableTxAccumulator::clearCache(uint256 txId)
	{
		std::lock_guard<std::mutex> lock(mutexTxCache_);
		mapTxAccumulator_.erase(txId);
	}
}
