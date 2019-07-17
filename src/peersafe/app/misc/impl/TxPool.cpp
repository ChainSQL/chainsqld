
#include <peersafe/app/misc/TxPool.h>

namespace ripple {

    h256Set TxPool::topTransactions(uint64_t const& limit)
    {
        h256Set ret;
        int txCnt = 0;

        std::lock_guard<std::mutex> lock(mutexTxPoll_);

        JLOG(j_.info()) << "Currently mTxsSet size: " << mTxsSet.size() 
            << ", mAvoid size: " << mAvoid.size();

        for (auto iter = mTxsSet.begin(); txCnt < limit && iter != mTxsSet.end(); ++iter)
        {
            if (!mAvoid.count((*iter)->getID()))
            {
                ret.insert((*iter)->getID());
                txCnt++;

                // update avoid set
                mAvoid.insert((*iter)->getID());
            }
        }

        return ret;
    }

    bool TxPool::insertTx(std::shared_ptr<Transaction> transaction)
    {
        std::lock_guard<std::mutex> lock(mutexTxPoll_);

        if (mTxsSet.size() >= mMaxTxsInPool)
        {
            JLOG(j_.warn()) << "Txs pool is full, insert failed, Tx hash: " 
                << transaction->getID();
            return false;
        }

        bool rc = false;
        auto result = mTxsSet.insert(transaction);

        JLOG(j_.info()) << "Inserting a " << (result.second ? "new" : "exist")
            << " Tx: " << transaction->getID();

        if (result.second)
        {
            auto ret = mTxsHash.emplace(make_pair(transaction->getID(), result.first));
            rc = ret.second;
        }

        return rc;
        //return true;
    }

    bool TxPool::removeTxs(RCLTxSet const& cSet)
    {
        TransactionSet::iterator iterSet;

        std::lock_guard<std::mutex> lock(mutexTxPoll_);

        for (auto const& item : *(cSet.map_))
        {
            try
            {
                // If not exist, throw std::out_of_range exception.
                iterSet = mTxsHash.at(item.key());

                // remove from Tx pool.
                mTxsHash.erase(item.key());
                mTxsSet.erase(iterSet);

                // remove from avoid set.
                mAvoid.erase(item.key());
            }
            catch (std::exception const&)
            {
                JLOG(j_.warn()) << "Tx: " << item.key() << " throws, not in pool";

                return false;
            }
        }

        return true;
    }

    void TxPool::updateAvoid(RCLTxSet const& cSet)
    {
        // If the Tx set had be added into avoid set recently, don't add it again.
        // TODO
        if (0)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(mutexTxPoll_);

        for (auto const& item : *(cSet.map_))
        {
            mAvoid.insert(item.key());
        }
    }
}
