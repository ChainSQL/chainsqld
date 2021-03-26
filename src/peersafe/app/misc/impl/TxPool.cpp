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

namespace ripple {

uint64_t
TxPool::topTransactions(uint64_t limit, LedgerIndex seq, H256Set& set)
{
    uint64_t txCnt = 0;

    std::shared_lock read_lock_set{mutexSet_};
    std::shared_lock read_lock_avoid{mutexAvoid_};

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
    if (mTxsSet.size() >= mMaxTxsInPool)
    {
        JLOG(j_.warn()) << "Txs pool is full, insert failed, Tx hash: "
                        << transaction->getID();
        return telTX_POOL_FULL;
    }

    TER ter = tefPAST_SEQ;
    std::unique_lock lock(mutexSet_);
    auto result = mTxsSet.insert(transaction);

    if (result.second)
    {
        JLOG(j_.trace()) << "Inserting a new Tx: " << transaction->getID();

        if (mTxsHash.emplace(make_pair(transaction->getID(), result.first))
                .second)
        {
            ter = tesSUCCESS;
            // Init sync_status
            if (mSyncStatus.pool_start_seq == 0)
            {
                mSyncStatus.pool_start_seq = ledgerSeq;
            }
        }
        else
        {
            JLOG(j_.error())
                << "mTxsHash.emplace failed, Tx: " << transaction->getID();
            mTxsSet.erase(transaction);
            ter = telLOCAL_ERROR;
        }
    }
    else
    {
        JLOG(j_.info()) << "Inserting a exist Tx: " << transaction->getID();
    }

    return ter;
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
            if (!txExists(item.key()))
                continue;

            {
                // If not exist, throw std::out_of_range exception.
                std::unique_lock<std::shared_mutex> lock(mutexSet_);
                iterSet = mTxsHash.at(item.key());
                // remove from Tx pool.
                mTxsHash.erase(item.key());
                mTxsSet.erase(iterSet);
            }

            count++;
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
    if (mAvoidBySeq.find(seq) != mAvoidBySeq.end() &&
        mAvoidBySeq[seq].size() > 0)
    {
        JLOG(j_.warn()) << "TxPool updateAvoid already "
                        << mAvoidBySeq[seq].size() << " txs for Seq:" << seq;
    }

    std::unique_lock lock(mutexAvoid_);

    for (auto const& item : map)
    {
        if (txExists(item.key()))
        {
            mAvoidBySeq[seq].insert(item.key());
            mAvoidByHash.emplace(item.key(), seq);
        }
    }
}

void
TxPool::clearAvoid(LedgerIndex seq)
{
    std::unique_lock lock(mutexAvoid_);

    for (auto const& hash : mAvoidBySeq[seq])
    {
        mAvoidByHash.erase(hash);
    }
    mAvoidBySeq.erase(seq);
}

bool
TxPool::isAvailable()
{
    return mSyncStatus.max_advance_seq <= mSyncStatus.pool_start_seq;
}

void
TxPool::timerEntry()
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
}

void
TxPool::removeTx(uint256 hash)
{
    if (mTxsHash.find(hash) != mTxsHash.end())
    {
        // remove from Tx pool.
        {
            std::unique_lock lock(mutexSet_);
            auto iter = mTxsHash.at(hash);
            mTxsHash.erase(hash);
            mTxsSet.erase(iter);
        }

        // remove from avoid set.
        std::unique_lock lock(mutexAvoid_);
        if (mAvoidByHash.find(hash) != mAvoidByHash.end())
        {
            LedgerIndex seq = mAvoidByHash[hash];
            mAvoidBySeq[seq].erase(hash);
            if (mAvoidBySeq[seq].size())
            {
                mAvoidBySeq.erase(seq);
            }
            mAvoidByHash.erase(hash);
        }
    }
}

}  // namespace ripple
