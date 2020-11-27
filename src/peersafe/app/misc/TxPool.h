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


#ifndef CHAINSQL_APP_MISC_TXPOOL_H_INCLUDED
#define CHAINSQL_APP_MISC_TXPOOL_H_INCLUDED


#include <set>
#include <mutex>
#include <memory>
#include <functional>
#include <unordered_map>
#include <ripple/core/ConfigSections.h>
#include <ripple/beast/core/LexicalCast.h>
#include <ripple/basics/base_uint.h>
#include <ripple/app/main/Application.h>
#include <ripple/app/misc/Transaction.h>
#include <ripple/beast/utility/Journal.h>
#include <ripple/app/consensus/RCLCxTx.h>
#include <ripple/protocol/TER.h>
#include <peersafe/app/util/Common.h>
#include <peersafe/app/consensus/PConsensusParams.h>


namespace ripple {

class STTx;
class RCLTxSet;


struct transactionCompare
{
	bool operator()(std::shared_ptr<Transaction> const& first, std::shared_ptr<Transaction> const& second) const
	{
		if (first->getSTransaction()->getAccountID(sfAccount) == second->getSTransaction()->getAccountID(sfAccount))
		{
			return first->getSTransaction()->getFieldU32(sfSequence) < second->getSTransaction()->getFieldU32(sfSequence);
		}
		else
		{
			return first->getSTransaction()->getAccountID(sfAccount) < second->getSTransaction()->getAccountID(sfAccount);
		}
		//return first->getTime() <= second->getTime();
	}
};

struct sync_status
{
	int pool_start_seq;
	int max_advance_seq;
	uint256 prevHash;
	int prevSeq;
	std::map<int, uint256> mapSynced;


	sync_status()
	{
		init();
	}

	void init()
	{
		pool_start_seq = 0;
		max_advance_seq = 0;
		prevHash = beast::zero;
		prevSeq = 0;
		mapSynced.clear();
	}
};

class TxPool
{
public:
    TxPool(Application& app, beast::Journal j);
	virtual ~TxPool() {}

    inline bool txExists(uint256 hash) { return mTxsHash.count(hash); }
    inline bool isEmpty() { return mTxsSet.size() == 0; }
    inline std::size_t getTxCountInPool() { return mTxsSet.size(); }

    // Set/Get pool limit.
    inline void setTxLimitInPool(std::size_t const& maxTxs) { mMaxTxsInPool = maxTxs; }
    inline std::size_t const& getTxLimitInPool() { return mMaxTxsInPool; }


    // Get at most specified counts of Tx from TxPool.
	h256Vector topTransactions(uint64_t const& limit);

    // Insert a new Tx, return true if success else false.
	TER insertTx(std::shared_ptr<Transaction> transaction,int ledgerSeq);

    // When block validated, remove Txs from pool and avoid set.
	void removeTxs(SHAMap const& cSet,int const ledgerSeq,uint256 const& prevHash);
    void removeTxs(std::vector<TxID> const& txHashes, int const ledgerSeq, uint256 const& prevHash);
	void removeTx(uint256 const& hash);

    // Update avoid set when receiving a Tx set from peers.
    void updateAvoid(RCLTxSet const& cSet);
	void clearAvoid();

	bool isAvailable();

	void timerEntry();

	void checkSyncStatus(int const ledgerSeq, uint256 const& prevHash);

private:
	Application& app_;

    std::mutex mutexTxPoll_;

    std::size_t mMaxTxsInPool;

	using TransactionSet = std::set<std::shared_ptr<Transaction>, transactionCompare>;

	TransactionSet mTxsSet;
    std::unordered_map<uint256, TransactionSet::iterator> mTxsHash;

    h256Set mAvoid;

	sync_status mSyncStatus;

    beast::Journal j_;
};

}
#endif