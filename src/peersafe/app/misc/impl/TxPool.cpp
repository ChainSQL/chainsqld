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

#include <ripple/app/ledger/LedgerMaster.h>
#include <peersafe/app/misc/TxPool.h>
#include <peersafe/app/misc/StateManager.h>

namespace ripple {

Json::Value
sync_status::getJson() const
{
    Json::Value ret(Json::objectValue);
    ret["pool_start_seq"] = pool_start_seq;
    ret["max_advance_seq"] = max_advance_seq;
    ret["prev_hash"] = to_string(prevHash);
    return ret;
}

uint64_t
TxPool::topTransactions(uint64_t limit, LedgerIndex seq, H256Set& set)
{
    uint64_t txCnt = 0;

    std::shared_lock<std::shared_mutex> read_lock_set{mutexSet_};
    std::shared_lock<std::shared_mutex> read_lock_avoid{mutexAvoid_};

    JLOG(j_.info()) << "Currently mTxsSet size: " << mTxsSet.size()
                    << ", mAvoid size: " << mAvoidByHash.size();

    for (auto iter = mTxsSet.begin(); txCnt < limit && iter != mTxsSet.end();
         ++iter)
    {
        if (!mAvoidByHash.count((*iter)->getID()))
        {
            set.insert((*iter)->getID());
            txCnt++;
        }
    }

    return txCnt;
}

TER
TxPool::insertTx(
    std::shared_ptr<Transaction> transaction,
    LedgerIndex ledgerSeq)
{
    std::unique_lock<std::shared_mutex> lock(mutexSet_);

    if (mTxsSet.size() >= mMaxTxsInPool)
    {
        JLOG(j_.warn()) << "Txs pool is full, insert failed, Tx hash: "
                        << transaction->getID();
        return telTX_POOL_FULL;
    }

    if (mInLedgerCache.count(transaction->getID()) > 0)
    {
        JLOG(j_.info()) << "Inserting a applied Tx: " << transaction->getID();
        return tesSUCCESS;
    }

    auto result = mTxsSet.insert(transaction);

    if (result.second)
    {
        JLOG(j_.trace()) << "Inserting a new Tx: " << transaction->getID();

        if (mTxsHash.emplace(make_pair(transaction->getID(), result.first))
                .second)
        {
            // Init sync_status
            if (mSyncStatus.pool_start_seq == 0)
            {
                mSyncStatus.pool_start_seq = ledgerSeq;
            }
            return tesSUCCESS;
        }
        else
        {
            JLOG(j_.error())
                << "mTxsHash.emplace failed, Tx: " << transaction->getID();
            mTxsSet.erase(transaction);
            return telLOCAL_ERROR;
        }
    }

    JLOG(j_.info()) << "Inserting an exist Tx: " << transaction->getID();

    return tefPAST_SEQ;
}

void
TxPool::removeTxs(
    SHAMap const& cSet,
    LedgerIndex ledgerSeq,
    uint256 const& prevHash)
{
    int count = 0;
    TransactionSet::iterator iterSet;
    try
    {
        for (auto const& item : cSet)
        {
            std::unique_lock<std::shared_mutex> lock(mutexSet_);
            if (mTxsHash.count(item.key()) <= 0)
            {
                mInLedgerCache.insert(item.key());
            }
            else
            {
                // If not exist, throw std::out_of_range exception.
                iterSet = mTxsHash.at(item.key());
                // remove from Tx pool.
                mTxsHash.erase(item.key());
                mTxsSet.erase(iterSet);
                count++;
            }
        }

        // remove avoid set.
        clearAvoid(ledgerSeq);
    }
    catch (std::exception const& e)
    {
        JLOG(j_.warn()) << "TxPool::removeTxs exception:" << e.what();
    }

    JLOG(j_.info()) << "Remove " << count << " txs for ledger " << ledgerSeq;

    checkSyncStatus(ledgerSeq, prevHash);
}

void
TxPool::checkSyncStatus(LedgerIndex ledgerSeq, uint256 const& prevHash)
{
    std::lock_guard lock(mutexMapSynced_);
    // update sync_status
    if (mTxsSet.size() == 0)
    {
        mSyncStatus.init();
        return;
    }

    // There are ledgers to be synced.
    if (mSyncStatus.pool_start_seq > 0)
    {
        if (mSyncStatus.max_advance_seq < ledgerSeq)
        {
            mSyncStatus.max_advance_seq = ledgerSeq;
        }

        mSyncStatus.mapSynced[ledgerSeq] = prevHash;

        // get next prevHash to sync
        {
            uint256 prevHashDue = beast::zero;
            int seqDue = 0;
            for (int i = mSyncStatus.max_advance_seq - 1;
                 i >= mSyncStatus.pool_start_seq;
                 i--)
            {
                if (mSyncStatus.mapSynced.find(i) ==
                    mSyncStatus.mapSynced.end())
                {
                    prevHashDue = mSyncStatus.mapSynced[i + 1];
                    seqDue = i;
                    break;
                }
            }
            if (prevHashDue == beast::zero)
            {
                mSyncStatus.pool_start_seq = mSyncStatus.max_advance_seq + 1;
                mSyncStatus.mapSynced.clear();
            }
            else
            {
                mSyncStatus.prevHash = prevHashDue;
                JLOG(j_.info())
                    << "start_seq:" << mSyncStatus.pool_start_seq
                    << ",advance seq=" << mSyncStatus.max_advance_seq
                    << ",ledger seq to acquire:" << seqDue;
            }
        }
    }
}

void
TxPool::updateAvoid(SHAMap const& map, LedgerIndex seq)
{
    std::unique_lock<std::shared_mutex> lock(mutexAvoid_);

    if (mAvoidBySeq.find(seq) != mAvoidBySeq.end() &&
        mAvoidBySeq[seq].size() > 0)
    {
        JLOG(j_.warn()) << "TxPool updateAvoid already "
                        << mAvoidBySeq[seq].size() << " txs for Seq:" << seq;
    }

    if (app_.getLedgerMaster().getValidLedgerIndex() >= seq)
    {
        return;
    }

    for (auto const& item : map)
    {
        // if (txExists(item.key()))
        //{
        mAvoidBySeq[seq].insert(item.key());
        mAvoidByHash.emplace(item.key(), seq);
        //}
    }
}

void
TxPool::clearAvoid(LedgerIndex seq)
{
    std::unique_lock<std::shared_mutex> lock(mutexAvoid_);

    for (auto const& hash : mAvoidBySeq[seq])
    {
        mAvoidByHash.erase(hash);
    }
    mAvoidBySeq.erase(seq);
}

void
TxPool::clearAvoid()
{
    std::unique_lock<std::shared_mutex> lock(mutexAvoid_);
    mAvoidByHash.clear();
    mAvoidBySeq.clear();
}

bool
TxPool::isAvailable()
{
    return mSyncStatus.max_advance_seq <= mSyncStatus.pool_start_seq;
}

void
TxPool::timerEntry(NetClock::time_point const& now)
{
    if (!isAvailable())
    {
        // we need to switch the ledger we're working from
        auto prevLedger =
            app_.getLedgerMaster().getLedgerByHash(mSyncStatus.prevHash);
        if (prevLedger)
        {
            JLOG(j_.info()) << "TxPool found ledger " << prevLedger->info().seq;
            removeTxs(
                prevLedger->txMap(),
                prevLedger->info().seq,
                prevLedger->info().parentHash);
        }
    }

    if (now - mDeleteTime >= inLedgerCacheDeleteInterval)
    {
        {
            std::unique_lock<std::shared_mutex> lock(mutexSet_);
            beast::expire(mInLedgerCache, inLedgerCacheLiveTime);
        }

        mDeleteTime = now;
    }
}

void
TxPool::removeTx(uint256 hash)
{
    {
        std::unique_lock<std::shared_mutex> lock_set(mutexSet_);
        auto iter = mTxsHash.find(hash);
        if ( iter != mTxsHash.end())
        {
            // remove from Tx pool.
            mTxsHash.erase(hash);
            mTxsSet.erase(iter->second);
        }
    }
    
    // remove from avoid set.
    std::unique_lock<std::shared_mutex> lock_avoid(mutexAvoid_);
    if (mAvoidByHash.find(hash) != mAvoidByHash.end())
    {
        LedgerIndex seq = mAvoidByHash[hash];
        mAvoidBySeq[seq].erase(hash);
        if (mAvoidBySeq[seq].size() == 0)
        {
            mAvoidBySeq.erase(seq);
        }
        mAvoidByHash.erase(hash);
    }
}

Json::Value
TxPool::txInPool()
{
    Json::Value ret(Json::objectValue);
    {
        std::shared_lock<std::shared_mutex> read_lock_avoid{mutexAvoid_};

        for (auto iter = mAvoidByHash.begin(); iter != mAvoidByHash.end();
             ++iter)
        {
            ret["avoid"].append(
                to_string(iter->first) + ":" + std::to_string(iter->second));
        }

        ret["avoid_size"] = (uint32_t)mAvoidByHash.size();
    }

    {
        std::shared_lock<std::shared_mutex> read_lock_set{mutexSet_};
        for (auto it = mTxsHash.begin(); it != mTxsHash.end(); it++)
        {
            if (mAvoidByHash.find(it->first) == mAvoidByHash.end())
                ret["free"].append(to_string(it->first));
        }
    }

    return ret;
}

void
TxPool::sweep()
{
    removeExpired();
}

void
TxPool::removeExpired()
{
    // Remove txs the lastLedgerSequence reach.
    uint64_t txCnt = 0;
    auto seq = app_.getLedgerMaster().getValidLedgerIndex();

    std::unique_lock<std::shared_mutex> lock_set{mutexSet_};

    auto iter = mTxsSet.begin();
    std::set<AccountID> setAccounts;
    while (iter != mTxsSet.end())
    {
        auto pTx = *iter;
        if (pTx && pTx->getSTransaction() &&
            pTx->getSTransaction()->isFieldPresent(sfLastLedgerSequence))
        {
            auto seqTx = 
                pTx->getSTransaction()->getFieldU32(sfLastLedgerSequence);
            auto& hash = pTx->getID();
            if (seqTx < seq)
            {
                setAccounts.emplace(
                    pTx->getSTransaction()->getAccountID(sfAccount));
                iter = mTxsSet.erase(iter);
                mTxsHash.erase(hash);
                txCnt++;
                continue;
            }
        }
        iter++;
    }
    for (auto const& account : setAccounts)
    {
        app_.getStateManager().resetAccountSeq(account);
    }

    // sweep avoid
    std::unique_lock<std::shared_mutex> lock_avoid{mutexAvoid_};

    auto it = mAvoidByHash.begin();
    while (it != mAvoidByHash.end())
    {
        if (it->second < seq)
        {
            auto seqTmp = it->second;
            it = mAvoidByHash.erase(it);
            mAvoidBySeq.erase(seqTmp);
        }
        else
            it++;
    }
    if (txCnt > 0)
    {
        JLOG(j_.warn()) << "TxPool sweep removed " << txCnt << " txs.";
    }
}

}  // namespace ripple
