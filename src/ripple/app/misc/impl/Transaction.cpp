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
#include <ripple/app/main/Application.h>
#include <ripple/app/misc/HashRouter.h>
#include <ripple/app/misc/Transaction.h>
#include <ripple/app/tx/apply.h>
#include <ripple/basics/Log.h>
#include <ripple/basics/safe_cast.h>
#include <ripple/core/DatabaseCon.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/jss.h>
#include <boost/optional.hpp>
#include <peersafe/app/util/Common.h>
#include <peersafe/schema/Schema.h>
#include <peersafe/app/sql/TxnDBConn.h>

namespace ripple {

Transaction::Transaction(
    std::shared_ptr<STTx const> const& stx,
    std::string& reason,
    Schema& app) noexcept
    : mTransaction(stx), mApp(app), j_(app.journal("Ledger"))
{
    try
    {
        mTransactionID = mTransaction->getTransactionID();
    }
    catch (std::exception& e)
    {
        reason = e.what();
        return;
    }
	mTimeCreate = utcTime();
    mStatus = NEW;
}

//
// Misc.
//

void
Transaction::setStatus(TransStatus ts, std::uint32_t lseq)
{
    mStatus = ts;
    mInLedger = lseq;
}

TransStatus
Transaction::sqlTransactionStatus(boost::optional<std::string> const& status)
{
    char const c = (status) ? (*status)[0] : safe_cast<char>(txnSqlUnknown);

    switch (c)
    {
        case txnSqlNew:
            return NEW;
        case txnSqlConflict:
            return CONFLICTED;
        case txnSqlHeld:
            return HELD;
        case txnSqlValidated:
            return COMMITTED;
        case txnSqlIncluded:
            return INCLUDED;
    }

    assert(c == txnSqlUnknown);
    return INVALID;
}

Transaction::pointer
Transaction::transactionFromSQL(
    boost::optional<std::uint64_t> const& ledgerSeq,
    boost::optional<std::string> const& status,
    Blob const& rawTxn,
    Blob const& txnMeta,
	Schema& app)
{
    std::uint32_t const inLedger =
        rangeCheckedCast<std::uint32_t>(ledgerSeq.value_or(0));

    SerialIter it(makeSlice(rawTxn));
    auto txn = std::make_shared<STTx const>(it);
    std::string reason;
    auto tr = std::make_shared<Transaction> (
        txn, reason, app);
    tr->setMeta(txnMeta);
    
    tr->setStatus (sqlTransactionStatus (status));
    tr->setLedger (inLedger);
    return tr;
}

Transaction::pointer Transaction::transactionFromSQLValidated(
    boost::optional<std::uint64_t> const& ledgerSeq,
    boost::optional<std::string> const& status,
    Blob const& rawTxn,
    Blob const& txnMeta,
    Schema& app)
{
    auto ret = transactionFromSQL(ledgerSeq, status, rawTxn, txnMeta, app);

	auto retPair = checkValidity(app, app.getHashRouter(),
		*ret->getSTransaction(), app.
		getLedgerMaster().getValidatedRules(),
		app.config());
	if (retPair.first != Validity::Valid)
	{
		JLOG(app.journal("Transaction").error()) << "checkValidity for tx " 
			<< to_string(ret->getID()) << " failed,reason:" << retPair.second;
		return {};
	}

    return ret;
}

Transaction::pointer Transaction::transactionFromSHAMap(
    boost::optional<std::uint64_t> const& ledgerSeq,
    boost::optional<std::string> const& status,
    uint256 const& transID,
    Schema& app)
{
    std::uint32_t const inLedger =
        rangeCheckedCast<std::uint32_t>(ledgerSeq.value_or(0));

    if (auto lgr = app.getLedgerMaster().getLedgerBySeq(ledgerSeq.value_or(0)))
    {
        auto transaction = lgr->txRead(transID);
        auto txn = transaction.first;
        auto meta = transaction.second;
        Serializer s;
        meta->add(s);

        std::string reason;
        auto tr = std::make_shared<Transaction>(
            txn, reason, app);

        tr->setStatus(sqlTransactionStatus(status));
        tr->setLedger(inLedger);
        tr->setMeta(s.getData());
        return tr;
    }

    return {};
}

Transaction::pointer Transaction::transactionFromSHAMapValidated(
    boost::optional<std::uint64_t> const& ledgerSeq,
    boost::optional<std::string> const& status,
    uint256 const& transID,
    Schema& app)
{
    if (auto ret = transactionFromSHAMap(ledgerSeq, status, transID, app))
    {
        if (checkValidity(app, app.getHashRouter(),
            *ret->getSTransaction(), app.
            getLedgerMaster().getValidatedRules(),
            app.config()).first !=
            Validity::Valid)
        {
            return{};
        }
        else
        {
            return ret;
        }
    }

    return {};
}

Transaction::pointer
Transaction::load(uint256 const& id, Schema& app)
{
	auto v = load(id, app, boost::none);
	if (v.which() == 0)
		return boost::get<Transaction::pointer>(v);
    return nullptr;
}

boost::variant<Transaction::pointer, bool>
Transaction::load(
    uint256 const& id,
    Schema& app,
    ClosedInterval<uint32_t> const& range)
{
    using op = boost::optional<ClosedInterval<uint32_t>>;

    return load(id, app, op{range});
}

boost::variant<Transaction::pointer, bool>
Transaction::load(
    uint256 const& id,
    Schema& app,
    boost::optional<ClosedInterval<uint32_t>> const& range)
{
    boost::optional<std::uint64_t> ledgerSeq;
    boost::optional<std::string> status;
    if (!app.config().useTxTables())
        return {};
    if (app.config().SAVE_TX_RAW)
    {
		std::string sql = "SELECT LedgerSeq,Status,RawTxn,TxnMeta "
			"FROM Transactions WHERE TransID='";
		sql.append(to_string(id));
		sql.append("';");

		Blob rawTxn;
        Blob txnMeta;
		{
			auto db = app.getTxnDB().checkoutDbRead();
			soci::blob sociRawTxnBlob(*db);
            soci::blob sociTxnMetaBlob(*db);
			soci::indicator rti;
            soci::indicator mti;

			*db << sql, soci::into(ledgerSeq), soci::into(status),
				soci::into(sociRawTxnBlob, rti),soci::into(sociTxnMetaBlob, mti);
			if (!db->got_data() || rti != soci::i_ok)
				return {};

			convert(sociRawTxnBlob, rawTxn);
            convert(sociTxnMetaBlob, txnMeta);
		}

		return Transaction::transactionFromSQLValidated(
			ledgerSeq, status, rawTxn,txnMeta, app);
    }
    else 
    {
		std::string sql = "SELECT LedgerSeq,Status "
			"FROM Transactions WHERE TransID='";
		sql.append(to_string(id));
		sql.append("';");

		{
			auto db = app.getTxnDB().checkoutDbRead();

			*db << sql, soci::into(ledgerSeq), soci::into(status);
			if (!db->got_data())
				return {};
		}

		return Transaction::transactionFromSHAMapValidated(
			ledgerSeq, status, id, app);
    }
}

// options 1 to include the date of the transaction
Json::Value
Transaction::getJson(JsonOptions options, bool binary) const
{
    Json::Value ret(mTransaction->getJson(JsonOptions::none, binary));

    if (mInLedger)
    {
        ret[jss::inLedger] = mInLedger;  // Deprecated.
        ret[jss::ledger_index] = mInLedger;

        if (options == JsonOptions::include_date)
        {
            auto ct = mApp.getLedgerMaster().getCloseTimeBySeq(mInLedger);
            if (ct)
                ret[jss::date] = ct->time_since_epoch().count();
        }
    }

    return ret;
}

}  // namespace ripple
