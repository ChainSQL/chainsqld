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

#ifndef RIPPLE_APP_MISC_IMPL_ACCOUNTTXPAGING_H_INCLUDED
#define RIPPLE_APP_MISC_IMPL_ACCOUNTTXPAGING_H_INCLUDED

#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/core/DatabaseCon.h>
#include <cstdint>
#include <string>
#include <utility>

//------------------------------------------------------------------------------

namespace ripple {

void
convertBlobsToTxResult(
    NetworkOPs::AccountTxs& to,
    std::uint32_t ledger_index,
    Blob const& rawTxn,
    Blob const& rawMeta,
    Schema& app);

void
saveLedgerAsync (Schema& app, std::uint32_t seq);

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
    std::uint32_t page_length);

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
    std::uint32_t page_length);

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
    std::uint32_t page_length);
void
processTransRes(
    Schema& app,
    DatabaseCon& connection,
    std::function<
        void(std::uint32_t, Blob&&, Blob&&)> const&
        onTransaction,
    std::optional<NetworkOPs::AccountTxMarker>& marker,
    std::string sql,
    std::uint32_t numberOfResults);

}  // namespace ripple

#endif
