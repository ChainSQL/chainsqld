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
#include <ripple/core/ConfigSections.h>
#include <ripple/core/DatabaseCon.h>
#include <ripple/core/JobQueue.h>
#include <ripple/core/TimeKeeper.h>
#include <memory>
#include <mutex>
#include <peersafe/consensus/LedgerTiming.h>
#include <peersafe/schema/Schema.h>
#include <thread>

namespace ripple {

RCLValidationsPolicy::RCLValidationsPolicy(Schema& app, beast::Journal j)
    : app_(app), j_(j)
{
    staleValidations_.reserve(512);
    if (app.config().exists(SECTION_CONSENSUS))
    {
        write_ = app.config().loadConfig(
            SECTION_CONSENSUS, "write_validation", write_);
    }
}

NetClock::time_point
RCLValidationsPolicy::now() const
{
    return app_.timeKeeper().closeTime();
}

void
RCLValidationsPolicy::onStale(RCLValidation&& v)
{
    // Store the newly stale validation; do not do significant work in this
    // function since this is a callback from Validations, which may be
    // doing other work.
    if (!write_)
        return;

    ScopedLockType sl(staleLock_);
    staleValidations_.emplace_back(std::move(v));
    if (staleWriting_)
        return;

    // addJob() may return false (Job not added) at shutdown.
    staleWriting_ = app_.getJobQueue().addJob(
        jtWRITE, "Validations::doStaleWrite", [this](Job&) {
            auto event =
                app_.getJobQueue().makeLoadEvent(jtDISK, "ValidationWrite");
            ScopedLockType sl(staleLock_);
            doStaleWrite(sl);
        });
}

void
RCLValidationsPolicy::flush(hash_map<PublicKey, RCLValidation>&& remaining)
{
    if (!write_)
        return;

    bool anyNew = false;
    {
        ScopedLockType sl(staleLock_);

        for (auto const& keyVal : remaining)
        {
            staleValidations_.emplace_back(std::move(keyVal.second));
            anyNew = true;
        }

        // If we have new validations to write and there isn't a write in
        // progress already, then write to the database synchronously.
        if (anyNew && !staleWriting_)
        {
            staleWriting_ = true;
            doStaleWrite(sl);
        }

        // In the case when a prior asynchronous doStaleWrite was scheduled,
        // this loop will block until all validations have been flushed.
        // This ensures that all validations are written upon return from
        // this function.

        while (staleWriting_)
        {
            ScopedUnlockType sul(staleLock_);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

// NOTE: doStaleWrite() must be called with staleLock_ *locked*.  The passed
// ScopedLockType& acts as a reminder to future maintainers.
void
RCLValidationsPolicy::doStaleWrite(ScopedLockType&)
{
    static const std::string insVal(
        "INSERT OR REPLACE INTO Validations "
        "(InitialSeq, LedgerSeq, LedgerHash, NodePubKey, SignTime, RawData) "
        "VALUES (:initialSeq, :ledgerSeq, "
        ":ledgerHash,:nodePubKey,:signTime,:rawData);");

    assert(staleWriting_);

    while (!staleValidations_.empty())
    {
        std::vector<RCLValidation> currentStale;
        currentStale.reserve(512);
        staleValidations_.swap(currentStale);

        {
            ScopedUnlockType sul(staleLock_);
            {
                auto db = app_.getLedgerDB().checkoutDb();

                Serializer s(1024);
                soci::transaction tr(*db);
                for (auto const& rclValidation : currentStale)
                {
                    auto ledgerSeq = rclValidation.seq();
                    s.erase();
                    STValidation::pointer const& val = rclValidation.unwrap();
                    val->add(s);

                    auto const ledgerHash = to_string(val->getLedgerHash());

                    auto const nodePubKey =
                        toBase58(TokenType::NodePublic, val->getSignerPublic());
                    auto const signTime =
                        val->getSignTime().time_since_epoch().count();

                    soci::blob rawData(*db);
                    rawData.append(
                        reinterpret_cast<const char*>(s.peekData().data()),
                        s.peekData().size());
                    assert(rawData.get_len() == s.peekData().size());

                    *db << insVal, soci::use(ledgerSeq), soci::use(ledgerSeq),
                        soci::use(ledgerHash), soci::use(nodePubKey),
                        soci::use(signTime), soci::use(rawData);
                }

                tr.commit();
            }
        }
    }

    staleWriting_ = false;
}

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
            }, app_.doJobCounter());
        return boost::none;
    }

    assert(!ledger->open() && ledger->isImmutable());
    assert(ledger->info().hash == hash);

    return RCLValidatedLedger(std::move(ledger), j_);
}

}  // namespace ripple
