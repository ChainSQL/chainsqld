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

#include <BeastConfig.h>
#include <ripple/ledger/OpenView.h>
#include <ripple/basics/contract.h>
#include <ripple/core/Config.h>
#include <peersafe/app/shard/MicroLedger.h>
#include <peersafe/app/shard/FinalLedger.h>
#include <peersafe/app/shard/ShardManager.h>
#include <ripple/app/misc/AmendmentTable.h>
#include <ripple/app/misc/NetworkOPs.h>


namespace ripple {

open_ledger_t const open_ledger {};

class OpenView::txs_iter_impl
    : public txs_type::iter_base
{
private:
    bool metadata_;
    txs_map::const_iterator iter_;

public:
    explicit
    txs_iter_impl (bool metadata,
            txs_map::const_iterator iter)
        : metadata_(metadata)
        , iter_(iter)
    {
    }

    std::unique_ptr<base_type>
    copy() const override
    {
        return std::make_unique<
            txs_iter_impl>(
                metadata_, iter_);
    }

    bool
    equal (base_type const& impl) const override
    {
        auto const& other = dynamic_cast<
            txs_iter_impl const&>(impl);
        return iter_ == other.iter_;
    }

    void
    increment() override
    {
        ++iter_;
    }

    value_type
    dereference() const override
    {
        value_type result;
        {
            SerialIter sit(
                iter_->second.first->slice());
            result.first = std::make_shared<
                STTx const>(sit);
        }
        if (metadata_)
        {
            SerialIter sit(
                iter_->second.second->slice());
            result.second = std::make_shared<
                STObject const>(sit, sfMetadata);
        }
        return result;
    }
};

//------------------------------------------------------------------------------

OpenView::OpenView (open_ledger_t,
    ReadView const* base, Rules const& rules,
        std::shared_ptr<void const> hold)
    : rules_ (rules)
    , info_ (base->info())
    , base_ (base)
    , hold_ (std::move(hold))
{
    info_.validated = false;
    info_.accepted = false;
    info_.seq = base_->info().seq + 1;
    info_.parentCloseTime = base_->info().closeTime;
    info_.parentHash = base_->info().hash;
}

OpenView::OpenView (ReadView const* base,
        std::shared_ptr<void const> hold)
    : rules_ (base->rules())
    , info_ (base->info())
    , base_ (base)
    , hold_ (std::move(hold))
    , open_ (base->open())
{
}

std::size_t
OpenView::txCount() const
{
    return txs_.size();
}

void
OpenView::apply (TxsRawView& to) const
{
    items_.apply(to);
    for (auto const& item : txs_)
        to.rawTxInsert (item.first,
            item.second.first,
                item.second.second);
}

void
OpenView::apply (MicroLedger& to, std::shared_ptr<CanonicalTXSet const> txSet) const
{
    to.setDropsDestroyed(items_.dropsDestroyed().drops());

    if (txSet)
    {
        // committee microledger transactions
        for (auto const& item : *txSet)
        {
            auto const& txID = item.second->getTransactionID();
            auto iter = txs_.find(txID);
            if (iter != txs_.end())
            {
                if (to.rawTxInsert(iter->first,
                    iter->second.first,
                    iter->second.second))
                {
                    to.addTxID(txID);
                }
            }
        }
    }
    else
    {
        // shard

        for (auto const& item : txs_)
        {
            if (to.rawTxInsert(item.first,
                item.second.first,
                item.second.second))
            {
                to.addTxID(item.first);
            }
        }

        for (auto const& item : items_.items())
        {
            to.addStateDelta(*base_, item.first, item.second.first, item.second.second);
        }
    }
}

//---

LedgerInfo const&
OpenView::info() const
{
    return info_;
}

Fees const&
OpenView::fees() const
{
    return base_->fees();
}

Rules const&
OpenView::rules() const
{
    return rules_;
}

bool
OpenView::exists (Keylet const& k) const
{
    return items_.exists(*base_, k);
}

auto
OpenView::succ (key_type const& key,
    boost::optional<key_type> const& last) const ->
        boost::optional<key_type>
{
    return items_.succ(*base_, key, last);
}

std::shared_ptr<SLE const>
OpenView::read (Keylet const& k) const
{
    return items_.read(*base_, k);
}

auto
OpenView::slesBegin() const ->
    std::unique_ptr<sles_type::iter_base>
{
    return items_.slesBegin(*base_);
}

auto
OpenView::slesEnd() const ->
    std::unique_ptr<sles_type::iter_base>
{
    return items_.slesEnd(*base_);
}

auto
OpenView::slesUpperBound(uint256 const& key) const ->
    std::unique_ptr<sles_type::iter_base>
{
    return items_.slesUpperBound(*base_, key);
}

auto
OpenView::txsBegin() const ->
    std::unique_ptr<txs_type::iter_base>
{
    return std::make_unique<txs_iter_impl>(
        !open(), txs_.cbegin());
}

auto
OpenView::txsEnd() const ->
    std::unique_ptr<txs_type::iter_base>
{
    return std::make_unique<txs_iter_impl>(
        !open(), txs_.cend());
}

bool
OpenView::txExists (key_type const& key) const
{
    return txs_.find(key) != txs_.end();
}

auto
OpenView::txRead (key_type const& key) const ->
    tx_type
{
    auto const iter = txs_.find(key);
    if (iter == txs_.end())
        return base_->txRead(key);
    auto const& item = iter->second;
    auto stx = std::make_shared<STTx const
        >(SerialIter{ item.first->slice() });
    decltype(tx_type::second) sto;
    if (item.second)
        sto = std::make_shared<STObject const>(
                SerialIter{ item.second->slice() },
                    sfMetadata);
    else
        sto = nullptr;
    return { std::move(stx), std::move(sto) };
}

//---

void
OpenView::rawErase(
    std::shared_ptr<SLE> const& sle)
{
    items_.erase(sle);
}

void
OpenView::rawInsert(
    std::shared_ptr<SLE> const& sle)
{
    items_.insert(sle);
}

void
OpenView::rawReplace(
    std::shared_ptr<SLE> const& sle)
{
    items_.replace(sle);
}

void
OpenView::rawDestroyZXC(
    ZXCAmount const& fee)
{
    items_.destroyZXC(fee);
    // VFALCO Deduct from info_.totalDrops ?
    //        What about child views?
}

//---

void
OpenView::rawTxInsert (key_type const& key,
    std::shared_ptr<Serializer const>
        const& txn, std::shared_ptr<
            Serializer const>
                const& metaData)
{
    auto const result = txs_.emplace (key,
        std::make_pair(txn, metaData));
    if (! result.second)
        LogicError("rawTxInsert: duplicate TX id" +
            to_string(key));
}

void OpenView::initFeeShardVoting(Application& app)
{
    feeShardVoting_ = std::make_shared<FeeShardVoting>(
        base_->fees(),
        setup_FeeVote(app.config().section("voting")));
}

void OpenView::initAmendmentSet()
{
    amendmentSet_ = std::make_shared<AmendmentSet>();
}

void OpenView::finalVote(MicroLedger const& microLedger0, Application& app)
{
    assert(feeShardVoting_);
    assert(amendmentSet_);

    auto j = app.journal("finalVote");

    // Voting fee settings
    auto const k1 = keylet::fees();
    auto iter1 = items_.items().find(k1.key);
    assert(iter1 != items_.items().end());
    if (iter1 != items_.items().end())
    {
        SLE::pointer feeObject = iter1->second.second;
        microLedger0.applyFeeSetting(*this, feeObject, j);

        feeObject->setFieldU64(sfBaseFee, feeShardVoting_->baseFeeVote.getVotes());
        feeObject->setFieldU32(sfReserveBase, feeShardVoting_->baseReserveVote.getVotes());
        feeObject->setFieldU32(sfReserveIncrement, feeShardVoting_->incReserveVote.getVotes());
        feeObject->setFieldU64(sfDropsPerByte, feeShardVoting_->dropsPerByteVote.getVotes());

        rawReplace(feeObject);
    }

    // Voting amendments
    auto const k2 = keylet::amendments();

    auto iter2 = items_.items().find(k2.key);
    if (iter2 != items_.items().end())
    {
        SLE::pointer amendmentObject = iter2->second.second;
        microLedger0.applyAmendments(*this, amendmentObject, j);
    }

    amendmentSet_->mTrustedValidations = app.getShardManager().shardCount();
    amendmentSet_->mThreshold = std::max(1,
        (amendmentSet_->mTrustedValidations * app.getAmendmentTable().majorityFraction()) / 256);

    JLOG(j.info()) << "Final voting amendment, shardCount is: "
        << amendmentSet_->mTrustedValidations <<
        " , threshold is: " << amendmentSet_->mThreshold;

    std::set<uint256> oldAmendments;
    majorityAmendments_t oldMajorities;

    SLE::pointer sle = std::const_pointer_cast<SLE>(base().read(k2));
    if (!sle)
    {
        sle = std::make_shared<SLE>(k2);
    }

    STVector256 newAmendments = sle->getFieldV256(sfAmendments);
    oldAmendments.insert(newAmendments.begin(), newAmendments.end());

    STArray newMajorities = sle->getFieldArray(sfMajorities);
    {
        using tp = NetClock::time_point;
        using d = tp::duration;

        for (auto const& m : newMajorities)
        {
            oldMajorities[m.getFieldH256(sfAmendment)] = tp(d(m.getFieldU32(sfCloseTime)));
        }
    }

    for (auto const& entry : amendmentSet_->votes())
    {
        NetClock::time_point majorityTime = {};

        bool const hasValMajority = (amendmentSet_->votes(entry.first) >= amendmentSet_->mThreshold);

        {
            auto const it = oldMajorities.find(entry.first);
            if (it != oldMajorities.end())
                majorityTime = it->second;
        }

        if (oldAmendments.count(entry.first) != 0)
        {
            JLOG(j.debug()) << entry.first << ": amendment already enabled";
        }
        else if (hasValMajority && (majorityTime == NetClock::time_point{}))
        {
            // Ledger says no majority, validators say yes
            JLOG(j.debug()) << entry.first << ": amendment got majority";
            // This amendment now has a majority
            newMajorities.push_back(STObject(sfMajority));
            auto& object = newMajorities.back();
            object.emplace_back(STHash256(sfAmendment, entry.first));
            object.emplace_back(STUInt32(sfCloseTime, base().parentCloseTime().time_since_epoch().count()));
            if (!app.getAmendmentTable().isSupported(entry.first))
            {
                JLOG(j.warn()) << "Unsupported amendment " << entry.first << " received a majority.";
            }
        }
        else if (!hasValMajority &&
            (majorityTime != NetClock::time_point{}))
        {
            // Ledger says majority, validators say no
            JLOG(j.debug()) << entry.first << ": amendment lost majority";
            for (auto it = newMajorities.begin(); it != newMajorities.end(); it++)
            {
                if (it->getFieldH256(sfAmendment) == entry.first)
                {
                    newMajorities.erase(it);
                    break;
                }
            }
        }
        else if ((majorityTime != NetClock::time_point{}) &&
            ((majorityTime + app.getAmendmentTable().majorityTime()) <= base().parentCloseTime()))
        {
            // Enable amendment
            newAmendments.push_back(entry.first);

            app.getAmendmentTable().enable(entry.first);

            if (!app.getAmendmentTable().isSupported(entry.first))
            {
                JLOG (j.error()) << "Unsupported amendment " << entry.first << " activated: server blocked.";
                app.getOPs().setAmendmentBlocked();
            }
        }
    }

    if (newAmendments.empty())
    {
        sle->makeFieldAbsent(sfAmendments);
    }
    else
    {
        sle->setFieldV256(sfAmendments, newAmendments);
    }

    if (newMajorities.empty())
    {
        sle->makeFieldAbsent(sfMajorities);
    }
    else
    {
        sle->setFieldArray(sfMajorities, newMajorities);
    }

    if (base().exists(k2))
    {
        rawReplace(sle);
    }
    else
    {
        rawInsert(sle);
    }
}

} // ripple
