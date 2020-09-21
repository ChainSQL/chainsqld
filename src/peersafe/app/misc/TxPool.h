#ifndef CHAINSQL_APP_MISC_TXPOOL_H_INCLUDED
#define CHAINSQL_APP_MISC_TXPOOL_H_INCLUDED


#include <ripple/basics/base_uint.h>
#include <ripple/app/main/Application.h>
#include <ripple/app/misc/Transaction.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/app/consensus/RCLCxTx.h>
#include <ripple/beast/utility/Journal.h>
#include <ripple/protocol/TER.h>
#include <ripple/protocol/Protocol.h>
#include <peersafe/app/util/Common.h>
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
    TxPool(Application& app, beast::Journal j)
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

    // Get at most specified counts of Tx fron TxPool.
	h256Set topTransactions(uint64_t const& limit, LedgerIndex seq);

    // Insert a new Tx, return true if success else false.
	TER insertTx(std::shared_ptr<Transaction> transaction,int ledgerSeq);

    // When block validated, remove Txs from pool and avoid set.
	void removeTxs(SHAMap const& cSet,int const ledgerSeq,uint256 const& prevHash);
	void removeTx(uint256 hash);

    // Update avoid set when receiving a Tx set from peers.
    void updateAvoid(RCLTxSet const& cSet, LedgerIndex seq);
	void clearAvoid(LedgerIndex seq);

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

    std::map<LedgerIndex, h256Set> mAvoidBySeq;
    std::unordered_map<uint256, LedgerIndex> mAvoidByHash;

	sync_status mSyncStatus;

    beast::Journal j_;
};

}
#endif