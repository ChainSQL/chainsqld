//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012-2017 Ripple Labs Inc.

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

#include <ripple/app/consensus/RCLValidations.h>
#include <ripple/app/ledger/InboundLedger.h>
#include <ripple/app/ledger/InboundLedgers.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/app/misc/ValidatorList.h>
#include <ripple/basics/Log.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/basics/chrono.h>
#include <ripple/core/DatabaseCon.h>
#include <ripple/core/JobQueue.h>
#include <ripple/core/TimeKeeper.h>
#include <peersafe/schema/Schema.h>
#include <peersafe/consensus/LedgerTiming.h>
#include <memory>
#include <mutex>
#include <thread>

namespace ripple {

RCLValidatedLedger::RCLValidatedLedger(MakeGenesis)
    : ledgerID_{0}, ledgerSeq_{0}, j_{beast::Journal::getNullSink()}
{
}

RCLValidatedLedger::RCLValidatedLedger(
    std::shared_ptr<Ledger const> const& ledger,
    beast::Journal j)
    : ledgerID_{ledger->info().hash}, ledgerSeq_{ledger->seq()}, j_{j}
{
    if (ledger->seq() > 1)
    {
        auto const hashIndex = ledger->read(keylet::skip());
        if (hashIndex)
        {
            assert(hashIndex->getFieldU32(sfLastLedgerSequence) == (seq() - 1));
            ancestors_ = hashIndex->getFieldV256(sfHashes).value();
        }
        else
            JLOG(j_.warn()) << "Ledger " << ledgerSeq_ << ":" << ledgerID_
                            << " missing recent ancestor hashes";
    }
}

auto
RCLValidatedLedger::minSeq() const -> Seq
{
    return seq() - std::min(seq(), static_cast<Seq>(ancestors_.size()));
}

auto
RCLValidatedLedger::seq() const -> Seq
{
    return ledgerSeq_;
}
auto
RCLValidatedLedger::id() const -> ID
{
    return ledgerID_;
}

auto RCLValidatedLedger::operator[](Seq const& s) const -> ID
{
    if (s >= minSeq() && s <= seq())
    {
        if (s == seq())
            return ledgerID_;
        Seq const diff = seq() - s;
        return ancestors_[ancestors_.size() - diff];
    }
    if (s != 0)
    {
        JLOG(j_.warn()) << "Unable to determine hash of ancestor seq=" << s
                        << " from ledger hash=" << ledgerID_
                        << " seq=" << ledgerSeq_;
    }

    // Default ID that is less than all others
    return ID{0};
}

// Return the sequence number of the earliest possible mismatching ancestor
RCLValidatedLedger::Seq
mismatch(RCLValidatedLedger const& a, RCLValidatedLedger const& b)
{
    using Seq = RCLValidatedLedger::Seq;

    // Find overlapping interval for known sequence for the ledgers
    Seq const lower = std::max(a.minSeq(), b.minSeq());
    Seq const upper = std::min(a.seq(), b.seq());

    Seq curr = upper;
    while (curr != Seq{0} && a[curr] != b[curr] && curr >= lower)
        --curr;

    // If the searchable interval mismatches entirely, then we have to
    // assume the ledgers mismatch starting post genesis ledger
    return (curr < lower) ? Seq{1} : (curr + Seq{1});
}

RCLValidationsAdaptor::RCLValidationsAdaptor(Schema& app, beast::Journal j)
    : app_(app), j_(j)
{
}

NetClock::time_point
RCLValidationsAdaptor::now() const
{
    return app_.timeKeeper().closeTime();
}

boost::optional<RCLValidatedLedger>
RCLValidationsAdaptor::acquire(LedgerHash const& hash)
{
    auto ledger = app_.getLedgerMaster().getLedgerByHash(hash);
    if (!ledger)
    {
        JLOG(j_.debug())
            << "Need validated ledger for preferred ledger analysis " << hash;

        Schema* pApp = &app_;

        app_.getJobQueue().addJob(
            jtADVANCE, "getConsensusLedger", [pApp, hash](Job&) {
                pApp->getInboundLedgers().acquire(
                    hash, 0, InboundLedger::Reason::CONSENSUS);
            });
        return boost::none;
    }

    assert(!ledger->open() && ledger->isImmutable());
    assert(ledger->info().hash == hash);

    return RCLValidatedLedger(std::move(ledger), j_);
}


}  // namespace ripple
