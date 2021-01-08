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


#ifndef CHAINSQL_APP_MISC_TXPOOL_H_INCLUDED
#define CHAINSQL_APP_MISC_TXPOOL_H_INCLUDED


#include <ripple/basics/base_uint.h>
#include <peersafe/schema/Schema.h>
#include <ripple/app/misc/Transaction.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/app/consensus/RCLCxTx.h>
#include <ripple/beast/utility/Journal.h>
#include <ripple/protocol/TER.h>
#include <ripple/protocol/Protocol.h>
#include <peersafe/app/util/Common.h>
#include <peersafe/schema/Schema.h>
#include <set>
#include <map>
#include <mutex>
#include <memory>
#include <functional>
#include <unordered_map>


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
    LedgerIndex pool_start_seq;
    LedgerIndex max_advance_seq;
    uint256 prevHash;
    LedgerIndex prevSeq;
    std::map<LedgerIndex, uint256> mapSynced;

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
    TxPool(Schema& app, beast::Journal j)
        : app_(app)
        , mMaxTxsInPool(app.getOPs().getConsensusParms().txPOOL_CAPACITY)
        , j_(j)
    {
    }

	virtual ~TxPool() {}

    inline bool txExists(uint256 hash) const { return mTxsHash.count(hash); }
    inline std::size_t const& getTxLimitInPool() const { return mMaxTxsInPool; }
    inline bool isEmpty() const { return mTxsSet.size() == 0; }
    inline std::size_t getTxCountInPool() const { return mTxsSet.size(); }
    inline std::size_t getQueuedTxCountInPool() const { return mTxsSet.size() - mAvoidByHash.size(); }

    // Get at most specified counts of Tx fron TxPool.
    uint64_t topTransactions(uint64_t limit, LedgerIndex seq, H256Set &set);

    // Insert a new Tx, return true if success else false.
	TER insertTx(std::shared_ptr<Transaction> transaction, LedgerIndex ledgerSeq);

    // When block validated, remove Txs from pool and avoid set.
	void removeTxs(SHAMap const& cSet, LedgerIndex ledgerSeq, uint256 const& prevHash);
	void removeTx(uint256 hash);

    // Update avoid set when receiving a Tx set from peers.
    void updateAvoid(SHAMap const& map, LedgerIndex seq);
	void clearAvoid(LedgerIndex seq);

	bool isAvailable();

	void timerEntry();

	void checkSyncStatus(LedgerIndex ledgerSeq, uint256 const& prevHash);

private:
	Schema& app_;

    std::mutex mutexTxPoll_;

    std::size_t mMaxTxsInPool;

	using TransactionSet = std::set<std::shared_ptr<Transaction>, transactionCompare>;

	TransactionSet mTxsSet;
    std::unordered_map<uint256, TransactionSet::iterator> mTxsHash;

    std::map<LedgerIndex, H256Set> mAvoidBySeq;
    std::unordered_map<uint256, LedgerIndex> mAvoidByHash;

	sync_status mSyncStatus;

    beast::Journal j_;
};

}
#endif