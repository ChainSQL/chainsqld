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


#include <ripple/basics/make_lock.h>
#include <ripple/core/ConfigSections.h>
#include <ripple/beast/core/LexicalCast.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/app/misc/AmendmentTable.h>
#include <ripple/app/ledger/TransactionMaster.h>
#include <ripple/app/ledger/LocalTxs.h>
#include <peersafe/consensus/ConsensusBase.h>
#include <peersafe/consensus/ConsensusParams.h>
#include <peersafe/consensus/pop/PopAdaptor.h>
#include <peersafe/app/misc/StateManager.h>


namespace ripple {


PopAdaptor::PopAdaptor(
    Application& app,
    std::unique_ptr<FeeVote>&& feeVote,
    LedgerMaster& ledgerMaster,
    InboundTransactions& inboundTransactions,
    ValidatorKeys const & validatorKeys,
    beast::Journal journal,
    LocalTxs& localTxs,
    ConsensusParms const& consensusParms)
    : RpcaPopAdaptor(
        app,
        std::move(feeVote),
        ledgerMaster,
        inboundTransactions,
        validatorKeys,
        journal,
        localTxs)
{
    if (app_.config().exists(SECTION_PCONSENSUS))
    {
        parms_.minBLOCK_TIME = std::max(parms_.minBLOCK_TIME, app.config().loadConfig(SECTION_PCONSENSUS, "min_block_time", (unsigned)0));
        parms_.maxBLOCK_TIME = std::max(parms_.maxBLOCK_TIME, app.config().loadConfig(SECTION_PCONSENSUS, "max_block_time", (unsigned)0));
        parms_.maxBLOCK_TIME = std::max(parms_.minBLOCK_TIME, parms_.maxBLOCK_TIME);

        parms_.maxTXS_IN_LEDGER = std::min(
            app.config().loadConfig(SECTION_PCONSENSUS, "max_txs_per_ledger", parms_.maxTXS_IN_LEDGER),
            consensusParms.txPOOL_CAPACITY);

        parms_.consensusTIMEOUT = std::chrono::milliseconds {
            std::max(
                (int)parms_.consensusTIMEOUT.count(),
                app.config().loadConfig(SECTION_PCONSENSUS, "time_out", 0)) };
        if (parms_.consensusTIMEOUT.count() <= parms_.maxBLOCK_TIME)
        {
            parms_.consensusTIMEOUT = std::chrono::milliseconds{ parms_.maxBLOCK_TIME * 2 };
        }

        parms_.initTIME = std::chrono::seconds{ app.config().loadConfig(SECTION_PCONSENSUS, "init_time", parms_.initTIME.count()) };

        parms_.omitEMPTY = app.config().loadConfig(SECTION_PCONSENSUS, "omit_empty_block", parms_.omitEMPTY);
    }
}

bool PopAdaptor::isLeader(PublicKey const& publicKey, LedgerIndex curSeq, std::uint64_t view)
{
    auto const& validators = app_.validators().validators();
    assert(validators.size() > 0);

    int leaderIndex = (view + curSeq) % validators.size();

    return publicKey == validators[leaderIndex];
}

inline bool PopAdaptor::isLeader(LedgerIndex curSeq, std::uint64_t view)
{
    return isLeader(valPublic_, curSeq, view);
}

int PopAdaptor::getPubIndex(PublicKey const& publicKey)
{
    auto const& validators = app_.validators().validators();

    for (int i = 0; i < validators.size(); i++)
    {
        if (validators[i] == publicKey)
            return i + 1;
    }

    return 0;
}

inline int PopAdaptor::getPubIndex()
{
    return getPubIndex(valPublic_);
}

auto PopAdaptor::onCollectFinish(
    RCLCxLedger const& ledger,
    std::vector<uint256> const& transactions,
    NetClock::time_point const& closeTime,
    std::uint64_t const& view,
    ConsensusMode mode) -> Result
{
    const bool wrongLCL = mode == ConsensusMode::wrongLedger;
    const bool proposing = mode == ConsensusMode::proposing;

    notify(protocol::neCLOSING_LEDGER, ledger, !wrongLCL);

    auto const& prevLedger = ledger.ledger_;

    //ledgerMaster_.applyHeldTransactions();
    // Tell the ledger master not to acquire the ledger we're probably building
    ledgerMaster_.setBuildingLedger(prevLedger->info().seq + 1);
    //auto initialLedger = app_.openLedger().current();

    auto initialSet = std::make_shared<SHAMap>(
        SHAMapType::TRANSACTION, app_.family(), SHAMap::version{ 1 });
    initialSet->setUnbacked();

    // Build SHAMap containing all transactions in our open ledger
    for (auto const& txID : transactions)
    {
        auto tx = app_.getMasterTransaction().fetch(txID, false);
        if (!tx)
        {
            JLOG(j_.error()) << "fetch transaction " + to_string(txID) + "failed";
            continue;
        }

        JLOG(j_.trace()) << "Adding open ledger TX " << txID;
        Serializer s(2048);
        tx->getSTransaction()->add(s);
        initialSet->addItem(
            SHAMapItem(tx->getID(), std::move(s)),
            true,
            false);
    }

    // Add pseudo-transactions to the set
    if ((app_.config().standalone() || (proposing && !wrongLCL)) &&
        ((prevLedger->info().seq % 256) == 0))
    {
        // previous ledger was flag ledger, add pseudo-transactions
        auto const validations =
            app_.getValidations().getTrustedForLedger(
                prevLedger->info().parentHash);

        if (validations.size() >= app_.validators().quorum())
        {
            feeVote_->doVoting(prevLedger, validations, initialSet);
            app_.getAmendmentTable().doVoting(
                prevLedger, validations, initialSet);
        }
    }

    // Now we need an immutable snapshot
    initialSet = initialSet->snapShot(false);
    auto setHash = initialSet->getHash().as_uint256();

    return Result{
        std::move(initialSet),
        RCLCxPeerPos::Proposal{
            RCLCxPeerPos::Proposal::seqJoin,
            setHash,
            prevLedger->info().hash,
            closeTime,
            app_.timeKeeper().closeTime(),
            nodeID_,
            valPublic_,
            prevLedger->info().seq + 1,
            view} };
}

void PopAdaptor::launchViewChange(STViewChange const& viewChange)
{
    Blob v = viewChange.getSerialized();

    protocol::TMConsensus consensus;

    consensus.set_msg(&v[0], v.size());
    consensus.set_msgtype(ConsensusMessageType::mtVIEWCHANGE);

    signAndSendMessage(consensus);
}

void PopAdaptor::onViewChanged(bool bWaitingInit, Ledger_t previousLedger)
{
    app_.getLedgerMaster().onViewChanged(bWaitingInit, previousLedger.ledger_);
    //Try to clear state cache.
    if (app_.getLedgerMaster().getPublishedLedgerAge() > 3 * parms_.consensusTIMEOUT
        && app_.getTxPool().isEmpty())
    {
        app_.getStateManager().clear();
    }

    if (bWaitingInit)
    {
        notify(protocol::neSWITCHED_LEDGER, previousLedger, true);
    }
    if (app_.openLedger().current()->info().seq != previousLedger.seq() + 1)
    {
        //Generate new openLedger
        CanonicalTXSet retriableTxs{ beast::zero };
        auto const lastVal = ledgerMaster_.getValidatedLedger();
        boost::optional<Rules> rules;
        if (lastVal)
            rules.emplace(*lastVal, app_.config().features);
        else
            rules.emplace(app_.config().features);
        app_.openLedger().accept(
            app_,
            *rules,
            previousLedger.ledger_,
            localTxs_.getTxSet(),
            false,
            retriableTxs,
            tapNONE,
            "consensus",
            [&](OpenView& view, beast::Journal j) {
            // Stuff the ledger with transactions from the queue.
            return app_.getTxQ().accept(app_, view);
        });
    }

    if (!validating())
    {
        notify(protocol::neCLOSING_LEDGER, previousLedger, mode() != ConsensusMode::wrongLedger);
    }
}

void PopAdaptor::touchAcquringLedger(LedgerHash const& prevLedgerHash)
{
    auto inboundLedger = app_.getInboundLedgers().find(prevLedgerHash);
    if (inboundLedger)
    {
        inboundLedger->touch();
    }
}

// ----------------------------------------------------------------------------
// Private member functions.

void PopAdaptor::doAccept(
    Result const& result,
    RCLCxLedger const& prevLedger,
    NetClock::duration closeResolution,
    ConsensusCloseTimes const& rawCloseTimes,
    ConsensusMode const& mode,
    Json::Value && consensusJson)
{
    prevProposers_ = result.proposers;
    prevRoundTime_ = result.roundTime.read();

    bool closeTimeCorrect;

    const bool proposing = (mode == ConsensusMode::proposing || mode == ConsensusMode::switchedLedger);
    const bool haveCorrectLCL = mode != ConsensusMode::wrongLedger;
    const bool consensusFail = result.state == ConsensusState::MovedOn;

    auto consensusCloseTime = result.position.closeTime();

    if (consensusCloseTime == NetClock::time_point{})
    {
        // We agreed to disagree on the close time
        using namespace std::chrono_literals;
        consensusCloseTime = prevLedger.closeTime() + std::chrono::seconds(1);
        closeTimeCorrect = false;
    }
    else
    {
        //Not need to round close time any more,just use leader's close time,adjust by prevLedger
        consensusCloseTime = std::max<NetClock::time_point>(consensusCloseTime, prevLedger.closeTime() + std::chrono::seconds(1));
        JLOG(j_.info()) << "consensusCloseTime:" << consensusCloseTime.time_since_epoch().count();

        closeTimeCorrect = true;
    }

    JLOG(j_.debug()) << "Report: Prop=" << (proposing ? "yes" : "no")
        << " val=" << (validating_ ? "yes" : "no")
        << " corLCL=" << (haveCorrectLCL ? "yes" : "no")
        << " fail=" << (consensusFail ? "yes" : "no");
    JLOG(j_.debug()) << "Report: Prev = " << prevLedger.id() << ":"
        << prevLedger.seq();

    //--------------------------------------------------------------------------
    std::set<TxID> failed;

    // Put transactions into a deterministic, but unpredictable, order
    CanonicalTXSet retriableTxs{ result.txns.map_->getHash().as_uint256() };
    JLOG(j_.debug()) << "Building canonical tx set: " << retriableTxs.key();

    for (auto const& item : *result.txns.map_)
    {
        try
        {
            retriableTxs.insert(std::make_shared<STTx const>(SerialIter{ item.slice() }));
            JLOG(j_.debug()) << "    Tx: " << item.key();
        }
        catch (std::exception const&)
        {
            failed.insert(item.key());
            JLOG(j_.warn()) << "    Tx: " << item.key() << " throws!";
        }
    }

    auto timeStart = utcTime();
    auto built = buildLCL(
        prevLedger,
        retriableTxs,
        consensusCloseTime,
        closeTimeCorrect,
        closeResolution,
        result.roundTime.read(),
        failed);
    JLOG(j_.info()) << "buildLCL time used:" << utcTime() - timeStart << "ms";
    timeStart = utcTime();

    auto const newLCLHash = built.id();
    JLOG(j_.debug()) << "Built ledger #" << built.seq() << ": " << newLCLHash;

    // Tell directly connected peers that we have a new LCL
    notify(protocol::neACCEPTED_LEDGER, built, haveCorrectLCL);

    if (validating_)
        validating_ = ledgerMaster_.isCompatible(
            *built.ledger_, j_.warn(), "Not validating");

    if (validating_ && !consensusFail &&
        app_.getValidations().canValidateSeq(built.seq()))
    {
        validate(built, result.txns, proposing);
        JLOG(j_.info()) << "CNF Val " << newLCLHash;
    }
    else
        JLOG(j_.info()) << "CNF buildLCL " << newLCLHash;

    // See if we can accept a ledger as fully-validated
    consensusBuilt(built.ledger_, result.txns.id(), std::move(consensusJson));

    //-------------------------------------------------------------------------
    {
        // Apply disputed transactions that didn't get in
        //
        // The first crack of transactions to get into the new
        // open ledger goes to transactions proposed by a validator
        // we trust but not included in the consensus set.
        //
        // These are done first because they are the most likely
        // to receive agreement during consensus. They are also
        // ordered logically "sooner" than transactions not mentioned
        // in the previous consensus round.
        //
        bool anyDisputes = false;
        for (auto& it : result.disputes)
        {
            if (!it.second.getOurVote())
            {
                // we voted NO
                try
                {
                    JLOG(j_.debug())
                        << "Test applying disputed transaction that did"
                        << " not get in " << it.second.tx().id();

                    SerialIter sit(it.second.tx().tx_.slice());
                    auto txn = std::make_shared<STTx const>(sit);

                    // Disputed pseudo-transactions that were not accepted
                    // can't be succesfully applied in the next ledger
                    if (isPseudoTx(*txn))
                        continue;

                    retriableTxs.insert(txn);

                    anyDisputes = true;
                }
                catch (std::exception const&)
                {
                    JLOG(j_.debug())
                        << "Failed to apply transaction we voted NO on";
                }
            }
        }

        // Build new open ledger
        auto lock = make_lock(app_.getMasterMutex(), std::defer_lock);
        auto sl = make_lock(ledgerMaster_.peekMutex(), std::defer_lock);
        std::lock(lock, sl);

        auto const lastVal = ledgerMaster_.getValidatedLedger();
        boost::optional<Rules> rules;
        if (lastVal)
            rules.emplace(*lastVal, app_.config().features);
        else
            rules.emplace(app_.config().features);
        app_.openLedger().accept(
            app_,
            *rules,
            built.ledger_,
            localTxs_.getTxSet(),
            anyDisputes,
            retriableTxs,
            tapNONE,
            "consensus",
            [&](OpenView& view, beast::Journal j) {
            // Stuff the ledger with transactions from the queue.
            return app_.getTxQ().accept(app_, view);
        });
        // Signal a potential fee change to subscribers after the open ledger
        // is created
        app_.getOPs().reportFeeChange();
    }
    JLOG(j_.info()) << "openLedger().accept time used:" << utcTime() - timeStart << "ms";
    //-------------------------------------------------------------------------
    {
        ledgerMaster_.switchLCL(built.ledger_);

        if (checkLedgerAccept(built.ledger_))
        {
            doValidLedger(built.ledger_);
        }

        // Do these need to exist?
        assert(ledgerMaster_.getClosedLedger()->info().hash == built.id());
        assert(app_.openLedger().current()->info().parentHash == built.id());
    }

    //-------------------------------------------------------------------------
    // we entered the round with the network,
    // see how close our close time is to other node's
    //  close time reports, and update our clock.
    if ((mode == ConsensusMode::proposing || mode == ConsensusMode::observing) && !consensusFail)
    {
        auto closeTime = rawCloseTimes.self;

        JLOG(j_.info()) << "We closed at "
            << closeTime.time_since_epoch().count();
        using usec64_t = std::chrono::duration<std::uint64_t>;
        usec64_t closeTotal =
            std::chrono::duration_cast<usec64_t>(closeTime.time_since_epoch());
        int closeCount = 1;

        for (auto const& p : rawCloseTimes.peers)
        {
            // FIXME: Use median, not average
            JLOG(j_.info())
                << std::to_string(p.second) << " time votes for "
                << std::to_string(p.first.time_since_epoch().count());
            closeCount += p.second;
            closeTotal += std::chrono::duration_cast<usec64_t>(
                p.first.time_since_epoch()) *
                p.second;
        }

        closeTotal += usec64_t(closeCount / 2);  // for round to nearest
        closeTotal /= closeCount;

        // Use signed times since we are subtracting
        using duration = std::chrono::duration<std::int32_t>;
        using time_point = std::chrono::time_point<NetClock, duration>;
        auto offset = time_point{ closeTotal } -
            std::chrono::time_point_cast<duration>(closeTime);
        JLOG(j_.info()) << "Our close offset is estimated at " << offset.count()
            << " (" << closeCount << ")";

        app_.timeKeeper().adjustCloseTime(offset);
    }
}

}