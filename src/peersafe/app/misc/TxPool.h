#ifndef CHAINSQL_APP_MISC_TXPOOL_H_INCLUDED
#define CHAINSQL_APP_MISC_TXPOOL_H_INCLUDED


#include <set>
#include <mutex>
#include <memory>
#include <functional>
#include <unordered_map>
#include <ripple/basics/base_uint.h>
#include <ripple/app/main/Application.h>
#include <ripple/app/misc/Transaction.h>
#include <ripple/beast/utility/Journal.h>
#include <ripple/app/consensus/RCLCxTx.h>
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
		return first->getTime() <= second->getTime();
	}
};

class TxPool
{
public:
    TxPool(Application& app, beast::Journal j)
        : app_(app)
        , j_(j)
    {
        mMaxTxsInPool = TxPoolCapacity;
    }

	virtual ~TxPool() {}

    // Get at most specified counts of Tx fron TxPool.
	h256Set topTransactions(uint64_t const& limit);

    // Insert a new Tx, return true if success else false.
	bool insertTx(std::shared_ptr<Transaction> transaction);

    // When block validated, remove Txs from pool and avoid set.
	bool removeTxs(SHAMap const& cSet);

    // Update avoid set when receiving a Tx set from peers.
    void updateAvoid(RCLTxSet const& cSet);

    inline bool txExists(uint256 hash) { return mTxsHash.count(hash); }

	// Set pool limit.
    void setTxLimitInPool(std::size_t const& maxTxs) { mMaxTxsInPool = maxTxs; }

    // Get pool limit.
    std::size_t const& getTxLimitInPool() { return mMaxTxsInPool; }

private:
	Application& app_;

    std::mutex mutexTxPoll_;

    std::size_t mMaxTxsInPool;

	using TransactionSet = std::set<std::shared_ptr<Transaction>, transactionCompare>;

	TransactionSet mTxsSet;
    std::unordered_map<uint256, TransactionSet::iterator> mTxsHash;

    h256Set mAvoid;

    beast::Journal j_;
};

}
#endif