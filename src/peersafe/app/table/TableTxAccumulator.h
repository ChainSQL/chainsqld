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

#ifndef CHAINSQL_APP_TABLE_SUBTX_ACCUM_H_INCLUDED
#define CHAINSQL_APP_TABLE_SUBTX_ACCUM_H_INCLUDED

#include <map>
#include <set>
#include <string>
#include <ripple/basics/base_uint.h>
#include <ripple/protocol/AccountID.h>

namespace ripple {
	class STTx;
	class Application;
	/*
	This class is used to statistic the db_success count of subTransaction of Contract/SqlTransaction tx .
	Will trigger db_success of Contract/SqlTransaction for all tables if all subTransaction return db_success.
	Will trigger db_error for the Contract/SqlTransaction if one subTransaction  return db_error.
	*/
	class TableTxAccumulator {
		struct SubTxInfo {
			SubTxInfo(){}

			std::set<std::pair<AccountID,std::string>>	setTables;	
			int						numSuccess;			//subtx count of db_success 
			int						numSubTxs;			//all subtx count
			int						startLedgerSeq;		//the ledger sequence when first subTx db_success triggered.
		};
	public:
		TableTxAccumulator(Application& app);
		void onSubtxResponse(STTx const& tx, AccountID const& owner, std::string tableName, int subTxCount,std::pair<bool, std::string> result);

		void pubTableTxSuccess(STTx const& tx, AccountID const& owner, std::string tableName);
		void pubTableTxError(STTx const& tx, AccountID const& owner,std::string tableName, std::string err_msg);

		void checkTxResult(STTx const& tx);
		void trySweepCache();
		void sweepCache();

		void clearCache(uint256 txId);
	private:
		std::mutex                              mutexTxCache_;
		bool									sweepingThread_;
		Application&							app_;
		std::map<uint256, SubTxInfo>			mapTxAccumulator_;	//key : transactionId
																	//value: first	total sub_tx count
																	//		 second current db_success_count
	};
}

#endif
