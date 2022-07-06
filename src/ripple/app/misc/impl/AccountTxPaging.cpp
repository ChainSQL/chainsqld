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
#include <ripple/app/ledger/LedgerToJson.h>
#include <ripple/app/main/Application.h>
#include <ripple/app/misc/Transaction.h>
#include <ripple/app/misc/impl/AccountTxPaging.h>
#include <ripple/protocol/Serializer.h>
#include <ripple/protocol/UintTypes.h>
#include <peersafe/schema/Schema.h>
#include <boost/format.hpp>
#include <memory>

namespace ripple {

bool getRawMeta(Ledger const& ledger,
    uint256 const& transID, Blob& raw, Blob& meta);

void
convertBlobsToTxResult(
    NetworkOPs::AccountTxs& to,
    std::uint32_t ledger_index,
    Blob const& rawTxn,
    Blob const& rawMeta,
    Schema& app)
{
    SerialIter it(makeSlice(rawTxn));
    auto txn = std::make_shared<STTx const>(it);
    std::string reason;

    auto tr = std::make_shared<Transaction>(txn, reason, app);

    tr->setLedger(ledger_index);

    auto metaset = std::make_shared<TxMeta> (
        tr->getID (), tr->getLedger (), rawMeta);

    to.emplace_back(std::move(tr), metaset);
};

void
saveLedgerAsync (Schema& app, std::uint32_t seq)
{
    if (auto l = app.getLedgerMaster().getLedgerBySeq(seq))
        pendSaveValidated(app, l, false, false);
}


void 
processTransRes(Schema& app,DatabaseCon& connection,std::function<
        void(std::uint32_t, Blob&&, Blob&&)> const&
        onTransaction, std::optional<NetworkOPs::AccountTxMarker>& marker,
    std::string sql, std::uint32_t numberOfResults)
{
    bool lookingForMarker = marker.has_value();
    std::uint32_t findLedger = 0, findSeq = 0;

    if (lookingForMarker)
    {
        findLedger = marker->ledgerSeq;
        findSeq = marker->txnSeq;
    }

    // marker is also an output parameter, so need to reset
    marker.reset();

    auto db(connection.checkoutDb());

    boost::optional<std::uint64_t> ledgerSeq;
    boost::optional<std::uint64_t> txnSeq;
    boost::optional<std::string> transID;

    soci::statement st = (db->prepare << sql,
        soci::into (ledgerSeq),
        soci::into (txnSeq),
        soci::into (transID));

    st.execute();

    std::map<uint32_t, std::shared_ptr<const ripple::Ledger>> ledgerCache;
    while (st.fetch())
    {
        if (lookingForMarker)
        {
            if (findLedger == ledgerSeq.value_or(0) &&
                (uint64_t(findLedger) * 100000 + findSeq) == txnSeq.value_or(0))
            {
                lookingForMarker = false;
            }
        }
        else if (numberOfResults == 0)
        {
            marker = {
                rangeCheckedCast<std::uint32_t>(ledgerSeq.value_or(0)),
                rangeCheckedCast<std::uint32_t>(txnSeq.value_or(0) % 100000)};
            break;
        }

        if (!lookingForMarker)
        {
            auto const seq =
                rangeCheckedCast<std::uint32_t>(ledgerSeq.value_or(0));
            auto const txID = from_hex_text<uint256>(transID.value());

            std::shared_ptr<const ripple::Ledger> lgr = nullptr;

            if (ledgerCache.count(seq))
            {
                lgr = ledgerCache[seq];
            }
            else if (
                lgr =
                    app.getLedgerMaster().getLedgerBySeq(ledgerSeq.value_or(0)))
            {
                ledgerCache.emplace(seq, lgr);
            }

            Blob txRaw, txMeta;
            if (lgr && getRawMeta(*lgr, txID, txRaw, txMeta))
            {
                onTransaction(seq, std::move(txRaw), std::move(txMeta));
            }
            --numberOfResults;
        }
    }
}

void
accountTxPage(
    Schema& app,
    DatabaseCon& connection,
    AccountIDCache const& idCache,
    std::function<void(std::uint32_t)> const& onUnsavedLedger,
    std::function<
        void(std::uint32_t, Blob&&, Blob&&)> const&
        onTransaction,
    AccountID const& account,
    std::int32_t minLedger,
    std::int32_t maxLedger,
    bool forward,
    std::optional<NetworkOPs::AccountTxMarker>& marker,
    int limit,
    bool bAdmin,
    std::uint32_t page_length)
{
    bool lookingForMarker = marker.has_value();

    std::uint32_t numberOfResults;

    if (limit <= 0 || (limit > page_length && !bAdmin))
        numberOfResults = page_length;
    else
        numberOfResults = limit;

    // As an account can have many thousands of transactions, there is a limit
    // placed on the amount of transactions returned. If the limit is reached
    // before the result set has been exhausted (we always query for one more
    // than the limit), then we return an opaque marker that can be supplied in
    // a subsequent query.
    std::uint32_t queryLimit = numberOfResults + 1;
    std::uint32_t findLedger = 0, findSeq = 0;

    if (lookingForMarker)
    {
        findLedger = marker->ledgerSeq;
        findSeq = marker->txnSeq;
    }

    // marker is also an output parameter, so need to reset
    //marker.reset();

    static std::string const prefix(
        R"(SELECT AccountTransactions.LedgerSeq,AccountTransactions.TxnSeq,
          AccountTransactions.TransID 
          FROM AccountTransactions WHERE )");

    std::string sql;

    // SQL's BETWEEN uses a closed interval ([a,b])

    if (forward && (findLedger == 0))
    {
        sql = boost::str(
            boost::format(
                prefix + (R"(AccountTransactions.Account = '%s' AND AccountTransactions.LedgerSeq BETWEEN '%u' AND '%u'
             ORDER BY AccountTransactions.LedgerSeq ASC,
             AccountTransactions.TxnSeq ASC
             LIMIT %u;)")) %
            idCache.toBase58(account) % minLedger % maxLedger % queryLimit);
    }
    else if (forward && (findLedger != 0))
    {
        auto b58acct = idCache.toBase58(account);
        sql = boost::str(
            boost::format(
                prefix + (R"((AccountTransactions.Account = '%s' AND
            AccountTransactions.LedgerSeq BETWEEN '%u' AND '%u')
            OR 
            (AccountTransactions.Account = '%s' AND
            AccountTransactions.LedgerSeq = '%u' AND
            AccountTransactions.TxnSeq >= '%u')
            ORDER BY AccountTransactions.LedgerSeq ASC,
            AccountTransactions.TxnSeq ASC
            LIMIT %u;
            )")) %
            b58acct % (findLedger + 1) % maxLedger %
            b58acct % findLedger % findSeq %
            queryLimit);
    }
    else if (!forward && (findLedger == 0))
    {
        sql = boost::str(
            boost::format(
                prefix + (R"(AccountTransactions.Account = '%s' AND AccountTransactions.LedgerSeq BETWEEN '%u' AND '%u'
             ORDER BY AccountTransactions.LedgerSeq DESC,
             AccountTransactions.TxnSeq DESC
             LIMIT %u;)")) %
            idCache.toBase58(account) % minLedger % maxLedger % queryLimit);
    }
    else if (!forward && (findLedger != 0))
    {
        auto b58acct = idCache.toBase58(account);
        sql = boost::str(
            boost::format(
                prefix + (R"((AccountTransactions.Account = '%s' AND
            AccountTransactions.LedgerSeq BETWEEN '%u' AND '%u')
            OR
            (AccountTransactions.Account = '%s' AND
            AccountTransactions.LedgerSeq = '%u' AND
            AccountTransactions.TxnSeq <= '%u')
            ORDER BY AccountTransactions.LedgerSeq DESC,
            AccountTransactions.TxnSeq DESC
            LIMIT %u;
            )")) %
            b58acct % minLedger % (findLedger - 1) %
            b58acct % findLedger % findSeq %
            queryLimit);
    }
    else
    {
        assert(false);
        // sql is empty
        return;
    }

    processTransRes(app, connection, onTransaction, marker, sql, numberOfResults);

    return;
}

void
accountTxPageSQL(
    Schema& app,
    DatabaseCon& connection,
    AccountIDCache const& idCache,
    std::function<void(std::uint32_t)> const& onUnsavedLedger,
    std::function<
        void(std::uint32_t, Blob&&, Blob&&)> const&
        onTransaction,
    AccountID const& account,
    std::int32_t minLedger,
    std::int32_t maxLedger,
    bool forward,
    std::optional<NetworkOPs::AccountTxMarker>& marker,
    int limit,
    bool bAdmin,
    std::uint32_t page_length)
{
	bool lookingForMarker = marker.has_value();

	std::uint32_t numberOfResults;

	if (limit <= 0 || (limit > page_length && !bAdmin))
		numberOfResults = page_length;
	else
		numberOfResults = limit;

	// As an account can have many thousands of transactions, there is a limit
	// placed on the amount of transactions returned. If the limit is reached
	// before the result set has been exhausted (we always query for one more
	// than the limit), then we return an opaque marker that can be supplied in
	// a subsequent query.
	std::uint32_t queryLimit = numberOfResults + 1;
	std::uint32_t findLedger = 0, findSeq = 0;

	if (lookingForMarker)
    {
        findLedger = marker->ledgerSeq;
        findSeq = marker->txnSeq;
    }

	static std::string const prefix(
		R"(SELECT AccountTransactions.LedgerSeq,AccountTransactions.TxnSeq,
          Status,RawTxn,TxnMeta
          FROM AccountTransactions INNER JOIN Transactions
          ON Transactions.TransID = AccountTransactions.TransID
          AND AccountTransactions.Account = '%s' WHERE
          )");

	std::string sql;

	// SQL's BETWEEN uses a closed interval ([a,b])

	if (forward && (findLedger == 0))
	{
		sql = boost::str(boost::format(
			prefix +
			(R"(AccountTransactions.LedgerSeq BETWEEN '%u' AND '%u'
             ORDER BY AccountTransactions.LedgerSeq ASC,
             AccountTransactions.TxnSeq ASC
             LIMIT %u;)"))
			% idCache.toBase58(account)
			% minLedger
			% maxLedger
			% queryLimit);
	}
	else if (forward && (findLedger != 0))
	{
		sql = boost::str(boost::format(
			prefix +
			(R"(
            AccountTransactions.LedgerSeq BETWEEN '%u' AND '%u' OR
            ( AccountTransactions.LedgerSeq = '%u' AND
              AccountTransactions.TxnSeq >= '%u' )
            ORDER BY AccountTransactions.LedgerSeq ASC,
            AccountTransactions.TxnSeq ASC
            LIMIT %u;
            )"))
			% idCache.toBase58(account)
			% (findLedger + 1)
			% maxLedger
			% findLedger
			% findSeq
			% queryLimit);
	}
	else if (!forward && (findLedger == 0))
	{
		sql = boost::str(boost::format(
			prefix +
			(R"(AccountTransactions.LedgerSeq BETWEEN '%u' AND '%u'
             ORDER BY AccountTransactions.LedgerSeq DESC,
             AccountTransactions.TxnSeq DESC
             LIMIT %u;)"))
			% idCache.toBase58(account)
			% minLedger
			% maxLedger
			% queryLimit);
	}
	else if (!forward && (findLedger != 0))
	{
		sql = boost::str(boost::format(
			prefix +
			(R"(AccountTransactions.LedgerSeq BETWEEN '%u' AND '%u' OR
             (AccountTransactions.LedgerSeq = '%u' AND
              AccountTransactions.TxnSeq <= '%u')
             ORDER BY AccountTransactions.LedgerSeq DESC,
             AccountTransactions.TxnSeq DESC
             LIMIT %u;)"))
			% idCache.toBase58(account)
			% minLedger
			% (findLedger - 1)
			% findLedger
			% findSeq
			% queryLimit);
	}
	else
	{
		assert(false);
		// sql is empty
		return;
	}

	{
		auto db(connection.checkoutDb());

		Blob rawData;
		Blob rawMeta;

		boost::optional<std::uint64_t> ledgerSeq;
		boost::optional<std::uint32_t> txnSeq;
		boost::optional<std::string> status;
		soci::blob txnData(*db);
		soci::blob txnMeta(*db);
		soci::indicator dataPresent, metaPresent;

		soci::statement st = (db->prepare << sql,
			soci::into(ledgerSeq),
			soci::into(txnSeq),
			soci::into(status),
			soci::into(txnData, dataPresent),
			soci::into(txnMeta, metaPresent));

		st.execute();

		while (st.fetch())
		{
			if (lookingForMarker)
			{
				if (findLedger == ledgerSeq.value_or(0) &&
					findSeq == txnSeq.value_or(0))
				{
					lookingForMarker = false;
				}
			}
			else if (numberOfResults == 0)
			{
				marker = {
                    rangeCheckedCast<std::uint32_t>(ledgerSeq.value_or(0)),
                    txnSeq.value_or(0)};
                break;
			}

			if (!lookingForMarker)
			{
				if (dataPresent == soci::i_ok)
					convert(txnData, rawData);
				else
					rawData.clear();

				if (metaPresent == soci::i_ok)
					convert(txnMeta, rawMeta);
				else
					rawMeta.clear();

				// Work around a bug that could leave the metadata missing
				if (rawMeta.size() == 0)
					onUnsavedLedger(ledgerSeq.value_or(0));

				onTransaction(rangeCheckedCast<std::uint32_t>(ledgerSeq.value_or(0)),
					std::move(rawData), std::move(rawMeta));
				--numberOfResults;
			}
		}
	}

	return;
}

void
contractTxPage(
    Schema& app,
    DatabaseCon& connection,
    AccountIDCache const& idCache,
    std::function<
        void(std::uint32_t, Blob&&, Blob&&)> const&
        onTransaction,
    AccountID const& account,
    std::int32_t minLedger,
    std::int32_t maxLedger,
    std::optional<NetworkOPs::AccountTxMarker>& marker,
    int limit,
    bool bAdmin,
    std::uint32_t page_length)
{
    bool lookingForMarker = marker.has_value();

    std::uint32_t numberOfResults;

    if (limit <= 0 || (limit > page_length && !bAdmin))
        numberOfResults = page_length;
    else
        numberOfResults = limit;

    // As an account can have many thousands of transactions, there is a limit
    // placed on the amount of transactions returned. If the limit is reached
    // before the result set has been exhausted (we always query for one more
    // than the limit), then we return an opaque marker that can be supplied in
    // a subsequent query.
    std::uint32_t queryLimit = numberOfResults + 1;
    std::uint32_t findLedger = 0, findSeq = 0;

    if (lookingForMarker)
    {
        findLedger = marker->ledgerSeq;
        findSeq = marker->txnSeq;
    }

    // marker is also an output parameter, so need to reset
    //marker.reset();

    static std::string const prefix(
        R"(SELECT LedgerSeq,TxSeq,TransID FROM TraceTransactions WHERE (Owner = '%s'
            AND TransType = 'Contract' AND LedgerSeq BETWEEN '%u' AND '%u')
          )");

    std::string sql;

    // SQL's BETWEEN uses a closed interval ([a,b])

    if (findLedger == 0)
    {
        sql = boost::str(
            boost::format(
                prefix + (R"(ORDER BY LedgerSeq DESC,
             TxSeq DESC
             LIMIT %u;)")) %
            idCache.toBase58(account) % minLedger % maxLedger % queryLimit);
    }
    else if (findLedger != 0)
    {
        auto b58acct = idCache.toBase58(account);
        sql = boost::str(
            boost::format(
                prefix + (R"( OR (Owner = '%s'
            AND TransType = 'Contract' AND
            LedgerSeq = '%u' AND TxSeq <= '%u')
            ORDER BY LedgerSeq DESC,TxSeq DESC
            LIMIT %u;
            )")) %
            b58acct % minLedger % (findLedger - 1) % b58acct % findLedger %
            (findLedger * 100000 + findSeq) % queryLimit);
    }
    else
    {
        assert(false);
        // sql is empty
        return;
    }
    processTransRes(app, connection, onTransaction, marker, sql, numberOfResults);

    return;
}

}
