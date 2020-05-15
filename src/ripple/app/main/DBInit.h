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

#ifndef RIPPLE_APP_DATA_DBINIT_H_INCLUDED
#define RIPPLE_APP_DATA_DBINIT_H_INCLUDED

namespace ripple {


	////////////////////////////////////////////////////////////////////////////////

// Ledger database holds ledgers and ledger confirmations
static constexpr auto LgrDBName{ "ledger.db" };

static constexpr
std::array<char const*, 3> LgrDBPragma{ {
	"PRAGMA synchronous=NORMAL;",
	"PRAGMA journal_mode=WAL;",
	"PRAGMA journal_size_limit=1582080;"
} };

static constexpr
std::array<char const*, 5> LgrDBInit{ {
	"BEGIN TRANSACTION;",

	"CREATE TABLE IF NOT EXISTS Ledgers (           \
        LedgerHash      CHARACTER(64) PRIMARY KEY,  \
        LedgerSeq       BIGINT UNSIGNED,            \
        PrevHash        CHARACTER(64),              \
        TotalCoins      BIGINT UNSIGNED,            \
        ClosingTime     BIGINT UNSIGNED,            \
        PrevClosingTime BIGINT UNSIGNED,            \
        CloseTimeRes    BIGINT UNSIGNED,            \
        CloseFlags      BIGINT UNSIGNED,            \
        AccountSetHash  CHARACTER(64),              \
        TransSetHash    CHARACTER(64)               \
    );",
	"CREATE INDEX IF NOT EXISTS SeqLedger ON Ledgers(LedgerSeq);",

	// Old table and indexes no longer needed
	"DROP TABLE IF EXISTS Validations;",

	"END TRANSACTION;"
} };

////////////////////////////////////////////////////////////////////////////////

// Transaction database holds transactions and public keys
static constexpr auto TxDBName{ "transaction.db" };

static constexpr
#if (ULONG_MAX > UINT_MAX) && !defined (NO_SQLITE_MMAP)
std::array<char const*, 6> TxDBPragma{ {
#else
	std::array<char const*, 5> TxDBPragma {{
#endif
	"PRAGMA page_size=4096;",
	"PRAGMA synchronous=NORMAL;",
	"PRAGMA journal_mode=WAL;",
	"PRAGMA journal_size_limit=1582080;",
	"PRAGMA max_page_count=2147483646;",
#if (ULONG_MAX > UINT_MAX) && !defined (NO_SQLITE_MMAP)
	"PRAGMA mmap_size=17179869184;"
#endif
}};

static constexpr
std::array<char const*, 8> TxDBInit {{
	"BEGIN TRANSACTION;",

	"CREATE TABLE IF NOT EXISTS Transactions (          \
        TransID     CHARACTER(64) PRIMARY KEY,          \
        TransType   CHARACTER(24),                      \
        FromAcct    CHARACTER(35),                      \
        FromSeq     BIGINT UNSIGNED,                    \
        LedgerSeq   BIGINT UNSIGNED,                    \
        Status      CHARACTER(1),                       \
        RawTxn      BLOB,                               \
        TxnMeta     BLOB                                \
    );",
	"CREATE INDEX IF NOT EXISTS TxLgrIndex ON           \
        Transactions(LedgerSeq);",

	"CREATE TABLE IF NOT EXISTS AccountTransactions (   \
        TransID     CHARACTER(64),                      \
        Account     CHARACTER(64),                      \
        LedgerSeq   BIGINT UNSIGNED,                    \
        TxnSeq      INTEGER                             \
    );",
	"CREATE INDEX IF NOT EXISTS AcctTxIDIndex ON        \
        AccountTransactions(TransID);",
	"CREATE INDEX IF NOT EXISTS AcctTxIndex ON          \
        AccountTransactions(Account, LedgerSeq, TxnSeq, TransID);",
	"CREATE INDEX IF NOT EXISTS AcctLgrIndex ON         \
        AccountTransactions(LedgerSeq, Account, TransID);",

	"END TRANSACTION;"
}};

////////////////////////////////////////////////////////////////////////////////

// Pragma for Ledger and Transaction databases with complete shards
static constexpr
std::array<char const*, 2> CompleteShardDBPragma {{
	"PRAGMA synchronous=OFF;",
	"PRAGMA journal_mode=OFF;"
}};


// VFALCO TODO Tidy these up into a class with functions and return types.
extern const char* TxnDBName;
extern const char* TxnDBInit[];
extern const char* LedgerDBInit[];
extern const char* WalletDBInit[];
extern const char* SyncTableStateDBInit[];
// VFALCO TODO Figure out what these counts are for
extern int TxnDBCount;
extern int LedgerDBCount;
extern int SyncTableStateDBCount;
extern int WalletDBCount;

} // ripple

#endif
