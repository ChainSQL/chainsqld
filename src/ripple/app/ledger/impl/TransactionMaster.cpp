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

#include <ripple/app/ledger/TransactionMaster.h>
#include <ripple/app/main/Application.h>
#include <ripple/app/ledger/Ledger.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/basics/Log.h>
#include <ripple/basics/chrono.h>
#include <peersafe/schema/Schema.h>
#include <ripple/protocol/STTx.h>
#include <peersafe/app/sql/TxnDBConn.h>

namespace ripple {

TransactionMaster::TransactionMaster(Schema& app)
    : mApp(app)
    , mCache(
          "TransactionCache",
          65536,
          std::chrono::minutes{30},
          stopwatch(),
          mApp.journal("TaggedCache"))
    , m_pClientTxStoreDBConn(std::make_unique<TxStoreDBConn>(mApp.config()))
    , m_pClientTxStoreDB(std::make_unique<TxStore>(m_pClientTxStoreDBConn->GetDBConn(), mApp.config(), mApp.journal("TxStore")))
    , m_pConsensusTxStoreDBConn(std::make_unique<TxStoreDBConn>(mApp.config()))
    , m_pConsensusTxStoreDB(std::make_unique<TxStore>(m_pConsensusTxStoreDBConn->GetDBConn(), mApp.config(), mApp.journal("TxStore")))
{
}

bool
TransactionMaster::inLedger(uint256 const& hash, std::uint32_t ledger)
{
    auto txn = mCache.fetch(hash);

    if (!txn)
        return false;

    txn->setStatus(COMMITTED, ledger);
    return true;
}

int
TransactionMaster::getTxCount(bool chainsql, int ledgerIndex)
{
	std::string sql = "SELECT COUNT(*) FROM Transactions ";
	if (chainsql)
	{
		sql += "WHERE (TransType = 'SQLTransaction' or TransType = 'TableListSet' or TransType = 'SQLStatement')";
        if (ledgerIndex > 0)
        {
            sql += " and LedgerSeq <=";
            sql += std::to_string(ledgerIndex);
        }
	}
    else if (ledgerIndex > 0)
    {
        sql += "WHERE LedgerSeq <=";
        sql += std::to_string(ledgerIndex);
    }

	sql += ";";

	boost::optional<int> txCount;
	{
		auto db = mApp.getTxnDB().checkoutDbRead();
		*db << sql,
			soci::into(txCount);

		if (!db->got_data() || !txCount)
			return 0;
	}

	return *txCount;
}

std::vector<STTx> TransactionMaster::getTxs(STTx const& stTx, std::string sTableNameInDB /* = "" */,
	std::shared_ptr<ReadView const> ledger /* = nullptr */,int ledgerSeq /* = 0 */,bool includeAssert /* = true*/)
{
	std::vector<STTx> vecTxs;
	if (stTx.getTxnType() == ttCONTRACT)
	{
		if (ledger == nullptr)
		{
			if (ledgerSeq == 0)
			{
				auto txn = fetch(stTx.getTransactionID());
				if (txn)
					ledgerSeq = txn->getLedger();
			}
			if(ledgerSeq != 0)
			{
				ledger = mApp.getLedgerMaster().getLedgerBySeq(ledgerSeq);
			}
		}
		if (ledger != nullptr)
		{
			auto rawMeta = ledger->txRead(stTx.getTransactionID()).second;
			vecTxs = STTx::getTxs(stTx, sTableNameInDB, rawMeta,includeAssert);
		}
	}
	else
	{
		vecTxs = STTx::getTxs(stTx, sTableNameInDB,NULL,includeAssert);
	}
	return vecTxs;
}
std::shared_ptr<Transaction>
TransactionMaster::fetch_from_cache(uint256 const& txnID)
{
    return mCache.fetch(txnID);
}

std::shared_ptr<Transaction>
TransactionMaster::fetch(uint256 const& txnID)
{
    auto txn = fetch_from_cache(txnID);

    if (txn)
        return txn;

    txn = Transaction::load(txnID, mApp);

    if (!txn)
        return txn;

    mCache.canonicalize_replace_client(txnID, txn);

    return txn;
}

boost::variant<Transaction::pointer, bool>
TransactionMaster::fetch(
    uint256 const& txnID,
    ClosedInterval<uint32_t> const& range)
{
    using pointer = Transaction::pointer;

    auto txn = mCache.fetch(txnID);

    if (txn)
        return txn;

    boost::variant<Transaction::pointer, bool> v =
        Transaction::load(txnID, mApp, range);

    if (v.which() == 0 && boost::get<pointer>(v))
        mCache.canonicalize_replace_client(txnID, boost::get<pointer>(v));

    return v;
}

std::shared_ptr<STTx const>
TransactionMaster::fetch(
    std::shared_ptr<SHAMapItem> const& item,
    SHAMapTreeNode::TNType type,
    std::uint32_t uCommitLedger)
{
    std::shared_ptr<STTx const> txn;
    auto iTx = fetch_from_cache(item->key());

    if (!iTx)
    {
        if (type == SHAMapTreeNode::tnTRANSACTION_NM)
        {
            SerialIter sit(item->slice());
            txn = std::make_shared<STTx const>(std::ref(sit));
        }
        else if (type == SHAMapTreeNode::tnTRANSACTION_MD)
        {
            auto blob = SerialIter{item->data(), item->size()}.getVL();
            txn = std::make_shared<STTx const>(
                SerialIter{blob.data(), blob.size()});
        }
    }
    else
    {
        if (uCommitLedger)
            iTx->setStatus(COMMITTED, uCommitLedger);

        txn = iTx->getSTransaction();
    }

    return txn;
}

TxStoreDBConn& TransactionMaster::getClientTxStoreDBConn()
{
    return *m_pClientTxStoreDBConn;
}

TxStore& TransactionMaster::getClientTxStore()
{
    return *m_pClientTxStoreDB;
}

TxStoreDBConn& TransactionMaster::getConsensusTxStoreDBConn()
{
    return *m_pConsensusTxStoreDBConn;
}

TxStore& TransactionMaster::getConsensusTxStore()
{
    return *m_pConsensusTxStoreDB;
}

void
TransactionMaster::canonicalize(std::shared_ptr<Transaction>* pTransaction,bool bReplace /* = false */)
{
    uint256 const tid = (*pTransaction)->getID();
    if (tid != beast::zero)
    {
        auto txn = *pTransaction;
        // VFALCO NOTE canonicalize can change the value of txn!
        if (!bReplace)
            mCache.canonicalize_replace_client(tid, txn);
        else
            mCache.canonicalize_replace_cache(tid, txn);
        *pTransaction = txn;
    }
}

void TransactionMaster::tune(int size, int age)
{
    mCache.setTargetSize(size);
    mCache.setTargetAge(std::chrono::seconds(age));
}

void TransactionMaster::sweep (void)
{
    mCache.sweep();
}

TaggedCache<uint256, Transaction>&
TransactionMaster::getCache()
{
    return mCache;
}

}  // namespace ripple
