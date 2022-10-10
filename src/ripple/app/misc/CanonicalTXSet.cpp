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

#include <ripple/app/misc/CanonicalTXSet.h>
#include <boost/range/adaptor/transformed.hpp>
#include <peersafe/app/tx/impl/Tuning.h>

namespace ripple {


uint256
CanonicalTXSet::accountKey(AccountID const& account)
{
    uint256 ret = beast::zero;
    memcpy(ret.begin(), account.begin(), account.size());
    ret ^= salt_;
    return ret;
}

bool
CanonicalTXSet::Key::operator<(Key const& rhs) const
{
    if (mAccount < rhs.mAccount)
        return true;

    if (mAccount > rhs.mAccount)
        return false;

    if (mSeq < rhs.mSeq)
        return true;

    if (mSeq > rhs.mSeq)
        return false;

    return mTXid < rhs.mTXid;
}

bool
CanonicalTXSet::Key::operator>(Key const& rhs) const
{
    if (mAccount > rhs.mAccount)
        return true;

    if (mAccount < rhs.mAccount)
        return false;

    if (mSeq > rhs.mSeq)
        return true;

    if (mSeq < rhs.mSeq)
        return false;

    return mTXid > rhs.mTXid;
}

bool
CanonicalTXSet::Key::operator<=(Key const& rhs) const
{
    if (mAccount < rhs.mAccount)
        return true;

    if (mAccount > rhs.mAccount)
        return false;

    if (mSeq < rhs.mSeq)
        return true;

    if (mSeq > rhs.mSeq)
        return false;

    return mTXid <= rhs.mTXid;
}

bool
CanonicalTXSet::Key::operator>=(Key const& rhs) const
{
    if (mAccount > rhs.mAccount)
        return true;

    if (mAccount < rhs.mAccount)
        return false;

    if (mSeq > rhs.mSeq)
        return true;

    if (mSeq < rhs.mSeq)
        return false;

    return mTXid >= rhs.mTXid;
}

bool
CanonicalTXSet::insert(std::shared_ptr<STTx const> const& txn,bool bJudgeLimit /* = false */)
{
    auto accId = txn->getAccountID(sfAccount);
    auto ret = map_.insert(std::make_pair(
        Key(accountKey(accId),
            txn->getSequence(),
            txn->getTransactionID()),
        txn));

    return ret.second;
}

std::vector<std::shared_ptr<STTx const>>
CanonicalTXSet::prune(AccountID const& account, std::uint32_t const seq)
{
    auto effectiveAccount = accountKey(account);

    Key keyLow(effectiveAccount, seq, beast::zero);
    Key keyHigh(effectiveAccount, seq + 1, beast::zero);

    auto range = boost::make_iterator_range(
        map_.lower_bound(keyLow), map_.lower_bound(keyHigh));
    auto txRange = boost::adaptors::transform(
        range, [](auto const& p) { return p.second; });

    std::vector<std::shared_ptr<STTx const>> result(
        txRange.begin(), txRange.end());

    map_.erase(range.begin(), range.end());

    return result;
}

//-------------------------------------------------------------------------------------------
uint256
CanonicalTXSetHeld::accountKey(AccountID const& account)
{
    uint256 ret = beast::zero;
    memcpy(ret.begin(), account.begin(), account.size());
    return ret;
}

bool
CanonicalTXSetHeld::Key::operator<(Key const& rhs) const
{
    if (mAccount < rhs.mAccount)
        return true;

    if (mAccount > rhs.mAccount)
        return false;

    return mSeq < rhs.mSeq;
}

bool
CanonicalTXSetHeld::Key::operator>(Key const& rhs) const
{
    if (mAccount > rhs.mAccount)
        return true;

    if (mAccount < rhs.mAccount)
        return false;

    return mSeq > rhs.mSeq;
}

bool
CanonicalTXSetHeld::Key::operator<=(Key const& rhs) const
{
    if (mAccount < rhs.mAccount)
        return true;

    if (mAccount > rhs.mAccount)
        return false;

    return mSeq <= rhs.mSeq;
}

bool
CanonicalTXSetHeld::Key::operator>=(Key const& rhs) const
{
    if (mAccount > rhs.mAccount)
        return true;

    if (mAccount < rhs.mAccount)
        return false;

    return mSeq >= rhs.mSeq;
}

bool
CanonicalTXSetHeld::insert(
    std::shared_ptr<Transaction> const& tx,bool bForceAdd)
{
    auto txn = tx->getSTransaction();
    auto accId = txn->getAccountID(sfAccount);

    if (!bForceAdd && accountTxsize_[accId] >= MAX_ACCOUNT_HELD_COUNT)
        return false;
    if (map_.size() >= MAX_HELD_COUNT)
        return false;

    auto ret = map_.insert(std::make_pair(
        Key(accountKey(accId), txn->getSequence()),
        tx));

    if (ret.second)
        accountTxsize_[accId]++;

    return ret.second;
}

std::vector<std::shared_ptr<Transaction>>
CanonicalTXSetHeld::prune(AccountID const& account, std::uint32_t const seq)
{
    auto effectiveAccount = accountKey(account);

    Key keyLow(effectiveAccount, seq);
    Key keyHigh(effectiveAccount, seq + 1);

    auto range = boost::make_iterator_range(
        map_.lower_bound(keyLow), map_.lower_bound(keyHigh));
    auto txRange = boost::adaptors::transform(
        range, [](auto const& p) { return p.second; });

    std::vector<std::shared_ptr<Transaction>> result(
        txRange.begin(), txRange.end());

    map_.erase(range.begin(), range.end());
    if (!result.empty() &&
        accountTxsize_.find(account) != accountTxsize_.end() &&
        accountTxsize_[account] > 0)
    {
        accountTxsize_[account]--;
        if (accountTxsize_[account] <= 0)
        {
            accountTxsize_.erase(account);
        }
    }

    return result;
}

}  // namespace ripple
