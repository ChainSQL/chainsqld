#ifndef CHAINSQL_APP_MISC_TXPOOL_H_INCLUDED
#define CHAINSQL_APP_MISC_TXPOOL_H_INCLUDED



#include <functional>
#include <set>
#include <unordered_map>
#include <memory>
#include <peersafe/app/util/Common.h>
#include <ripple/app/misc/Transaction.h>

namespace ripple {

class Application;
class STTx;
class RCLTxSet;


struct transactionCompare
{
	bool operator()(Transaction const& first, Transaction const& second) const
	{
		if (first.getSTransaction()->getAccountID(sfAccount) == second.getSTransaction()->getAccountID(sfAccount))
		{
			return first.getSTransaction()->getFieldU32(sfSequence) < second.getSTransaction()->getFieldU32(sfSequence);
		}
		return first.getTime() <= second.getTime();
	}
};

class TxPool
{
public:
	TxPool(Application& app);
	virtual ~TxPool() {};

	h256Set topTransactions(
		uint64_t const& limit, h256Set& avoid, bool updateAvoid = false);

	bool insertTx(std::shared_ptr<Transaction> transaction);
	bool removeTxs(RCLTxSet const& set);

	//set max transaction count in a ledger
	void setTxLimitInLedger(unsigned const& maxTxs);
	unsigned const& getTxLimitInLedger() {	return mMaxTxsInLedger;	}

	void setTxLimitInPool(unsigned const& maxTxs);
	unsigned const& getTxLimitInPool() { return mMaxTxsInPool; }

private:
	Application& mApp;
	unsigned mMaxTxsInLedger;
	unsigned mMaxTxsInPool;

	using TransactionSet = std::set<std::shared_ptr<Transaction>, transactionCompare>;
	TransactionSet m_set;
	std::unordered_map<uint256, TransactionSet::iterator> m_txsHash;
};

}
#endif