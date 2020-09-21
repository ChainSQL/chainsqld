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


#include <ripple/app/misc/ValidatorKeys.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/app/misc/HashRouter.h>
#include <ripple/app/misc/LoadFeeTrack.h>
#include <ripple/app/misc/Transaction.h>
#include <ripple/app/misc/AmendmentTable.h>
#include <ripple/app/ledger/LocalTxs.h>
#include <ripple/app/ledger/BuildLedger.h>
#include <ripple/app/ledger/TransactionMaster.h>
#include <ripple/overlay/Overlay.h>
#include <ripple/overlay/predicates.h>
#include <ripple/protocol/Feature.h>
#include <ripple/basics/make_lock.h>
#include <peersafe/consensus/Adaptor.h>
#include <peersafe/app/misc/StateManager.h>
#include <peersafe/app/misc/TxPool.h>
#include <peersafe/app/util/Common.h>


namespace ripple {

Adaptor::Adaptor(
    Application& app,
    std::unique_ptr<FeeVote>&& feeVote,
    LedgerMaster& ledgerMaster,
    LocalTxs& localTxs,
    InboundTransactions& inboundTransactions,
    ValidatorKeys const& validatorKeys,
    beast::Journal journal)
    : app_(app)
    , j_(journal)
    , feeVote_(std::move(feeVote))
    , ledgerMaster_(ledgerMaster)
    , localTxs_(localTxs)
    , inboundTransactions_{ inboundTransactions }
    , nodeID_{ validatorKeys.nodeID }
    , valPublic_{ validatorKeys.publicKey }
    , valSecret_{ validatorKeys.secretKey }
{
}

bool Adaptor::preStartRound(RCLCxLedger const & prevLgr)
{
    // We have a key, we do not want out of sync validations after a restart
    // and are not amendment blocked.
    validating_ = valPublic_.size() != 0 &&
        //prevLgr.seq() >= app_.getMaxDisallowedLedger() &&
        !app_.getOPs().isAmendmentBlocked();

    // If we are not running in standalone mode and there's a configured UNL,
    // check to make sure that it's not expired.
    if (validating_ && !app_.config().standalone() && app_.validators().count())
    {
        auto const when = app_.validators().expires();

        if (!when || *when < app_.timeKeeper().now())
        {
            JLOG(j_.error()) << "Voluntarily bowing out of consensus process "
                "because of an expired validator list.";
            validating_ = false;
        }
    }

    const bool synced = app_.getOPs().getOperatingMode() == NetworkOPs::omFULL;

    if (validating_)
    {
        JLOG(j_.info()) << "Entering consensus process, validating, synced="
            << (synced ? "yes" : "no");
    }
    else
    {
        // Otherwise we just want to monitor the validation process.
        JLOG(j_.info()) << "Entering consensus process, watching, synced="
            << (synced ? "yes" : "no");
    }

    // Notify inbound ledgers that we are starting a new round
    inboundTransactions_.newRound(prevLgr.seq());

    // propose only if we're in sync with the network (and validating)
    return validating_ && synced;
}

void Adaptor::notify(
    protocol::NodeEvent ne,
    RCLCxLedger const& ledger,
    bool haveCorrectLCL)
{
    protocol::TMStatusChange s;

    if (!haveCorrectLCL)
        s.set_newevent(protocol::neLOST_SYNC);
    else
        s.set_newevent(ne);

    s.set_ledgerseq(ledger.seq());
    s.set_networktime(app_.timeKeeper().now().time_since_epoch().count());
    s.set_ledgerhashprevious(
        ledger.parentID().begin(),
        std::decay_t<decltype(ledger.parentID())>::bytes);
    s.set_ledgerhash(
        ledger.id().begin(), std::decay_t<decltype(ledger.id())>::bytes);

    std::uint32_t uMin, uMax;
    if (!ledgerMaster_.getFullValidatedRange(uMin, uMax))
    {
        uMin = 0;
        uMax = 0;
    }
    else
    {
        // Don't advertise ledgers we're not willing to serve
        uMin = std::max(uMin, ledgerMaster_.getEarliestFetch());
    }
    s.set_firstseq(uMin);
    s.set_lastseq(uMax);
    app_.overlay().foreach(
        send_always(std::make_shared<Message>(s, protocol::mtSTATUS_CHANGE)));
    JLOG(j_.trace()) << "send status change to peer";
}

boost::optional<RCLCxLedger> Adaptor::acquireLedger(LedgerHash const& hash)
{
    // we need to switch the ledger we're working from
    auto built = ledgerMaster_.getLedgerByHash(hash);
    if (!built)
    {
        if (acquiringLedger_ != hash)
        {
            // need to start acquiring the correct consensus LCL
            JLOG(j_.warn()) << "Need consensus ledger " << hash;

            // Tell the ledger acquire system that we need the consensus ledger
            acquiringLedger_ = hash;

            app_.getJobQueue().addJob(jtADVANCE, "getConsensusLedger",
                [id = hash, &app = app_](Job&)
            {
                app.getInboundLedgers().acquire(id, 0,
                    InboundLedger::Reason::CONSENSUS);
            });
        }
        return boost::none;
    }

    assert(!built->open() && built->isImmutable());
    assert(built->info().hash == hash);

    // Notify inbound transactions of the new ledger sequence number
    inboundTransactions_.newRound(built->info().seq);

    return RCLCxLedger(built);
}

void Adaptor::relay(RCLCxPeerPos const& peerPos)
{
    protocol::TMProposeSet prop;

    auto const& proposal = peerPos.proposal();

    prop.set_proposeseq(proposal.proposeSeq());
    prop.set_closetime(proposal.closeTime().time_since_epoch().count());

    prop.set_currenttxhash(
        proposal.position().begin(), proposal.position().size());
    prop.set_previousledger(
        proposal.prevLedger().begin(), proposal.position().size());

    auto const pk = peerPos.publicKey().slice();
    prop.set_nodepubkey(pk.data(), pk.size());

    auto const sig = peerPos.signature();
    prop.set_signature(sig.data(), sig.size());

    app_.overlay().relay(prop, peerPos.suppressionID());
}

void Adaptor::relay(RCLCxTx const& tx)
{
    // If we didn't relay this transaction recently, relay it to all peers
    if (app_.getHashRouter().shouldRelay(tx.id()))
    {
        JLOG(j_.debug()) << "Relaying disputed tx " << tx.id();
        auto const slice = tx.tx_.slice();
        protocol::TMTransaction msg;
        msg.set_rawtransaction(slice.data(), slice.size());
        msg.set_status(protocol::tsNEW);
        msg.set_receivetimestamp(
            app_.timeKeeper().now().time_since_epoch().count());
        app_.overlay().foreach(send_always(
            std::make_shared<Message>(msg, protocol::mtTRANSACTION)));
    }
    else
    {
        JLOG(j_.debug()) << "Not relaying disputed tx " << tx.id();
    }
}

boost::optional<RCLTxSet> Adaptor::acquireTxSet(RCLTxSet::ID const& setId)
{
    if (auto txns = inboundTransactions_.getSet(setId, true))
    {
        return RCLTxSet{ std::move(txns) };
    }
    return boost::none;
}

std::size_t Adaptor::proposersFinished(
    RCLCxLedger const& ledger,
    LedgerHash const& h) const
{
    RCLValidations& vals = app_.getValidations();
    return vals.getNodesAfter(
        RCLValidatedLedger(ledger.ledger_, vals.adaptor().journal()), h);
}

void Adaptor::propose(RCLCxPeerPos::Proposal const& proposal)
{
    JLOG(j_.trace()) << "We propose: "
        << (proposal.isBowOut()
            ? std::string("bowOut")
            : ripple::to_string(proposal.position()));

    protocol::TMProposeSet prop;

    prop.set_currenttxhash(
        proposal.position().begin(), proposal.position().size());
    prop.set_previousledger(
        proposal.prevLedger().begin(), proposal.position().size());
    prop.set_curledgerseq(proposal.curLedgerSeq());
    prop.set_view(proposal.view());
    prop.set_proposeseq(proposal.proposeSeq());
    prop.set_closetime(proposal.closeTime().time_since_epoch().count());

    prop.set_nodepubkey(valPublic_.data(), valPublic_.size());

    auto signingHash = sha512Half(
        HashPrefix::proposal,
        std::uint32_t(proposal.proposeSeq()),
        proposal.closeTime().time_since_epoch().count(),
        proposal.prevLedger(),
        proposal.position());

    auto sig = signDigest(valPublic_, valSecret_, signingHash);

    prop.set_signature(sig.data(), sig.size());

    auto const suppression = proposalUniqueId(
        proposal.position(),
        proposal.prevLedger(),
        proposal.proposeSeq(),
        proposal.closeTime(),
        valPublic_,
        sig);

    app_.getHashRouter().addSuppression(suppression);

    app_.overlay().send(prop);
}

uint256 Adaptor::getPrevLedger(
    uint256 ledgerID,
    RCLCxLedger const& ledger,
    ConsensusMode mode)
{
    RCLValidations& vals = app_.getValidations();
    uint256 netLgr = vals.getPreferred(
        RCLValidatedLedger{ ledger.ledger_, vals.adaptor().journal() },
        getValidLedgerIndex());

    if (netLgr != ledgerID)
    {
        if (mode != ConsensusMode::wrongLedger)
            app_.getOPs().consensusViewChange();

        JLOG(j_.debug()) << Json::Compact(app_.getValidations().getJsonTrie());
    }

    return netLgr;
}

void Adaptor::onModeChange(ConsensusMode before, ConsensusMode after)
{
    if (before != after)
    {
        JLOG(j_.info()) << "Consensus mode change before=" << to_string(before)
            << ", after=" << to_string(after);
        mode_ = after;
    }
}

auto Adaptor::onCollectFinish(
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
            prevLedger->info().hash,
            prevLedger->info().seq + 1,
            view,
            RCLCxPeerPos::Proposal::seqJoin,
            setHash,
            closeTime,
            app_.timeKeeper().closeTime(),
            nodeID_} };
}

void Adaptor::onViewChanged(bool bWaitingInit, Ledger_t previousLedger)
{
    app_.getLedgerMaster().onViewChanged(bWaitingInit, previousLedger.ledger_);
    //Try to clear state cache.
    if (app_.getLedgerMaster().getPublishedLedgerAge() > 3 * app_.getOPs().getConsensusTimeout() &&
        app_.getTxPool().isEmpty())
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

void Adaptor::onAccept(
    Result const& result,
    RCLCxLedger const& prevLedger,
    NetClock::duration const& closeResolution,
    ConsensusCloseTimes const& rawCloseTimes,
    ConsensusMode const& mode,
    Json::Value && consensusJson)
{
    app_.getJobQueue().addJob(
        jtACCEPT,
        "acceptLedger",
        [=, cj = std::move(consensusJson)](auto&) mutable {
        // Note that no lock is held or acquired during this job.
        // This is because generic Consensus guarantees that once a ledger
        // is accepted, the consensus results and capture by reference state
        // will not change until startRound is called (which happens via
        // endConsensus).
        auto timeStart = utcTime();
        this->doAccept(
            result,
            prevLedger,
            closeResolution,
            rawCloseTimes,
            mode,
            std::move(cj));
        JLOG(j_.info()) << "doAccept time used:" << utcTime() - timeStart << "ms";
        this->app_.getOPs().endConsensus();
    });
}

void Adaptor::onForceAccept(
    Result const& result,
    RCLCxLedger const& prevLedger,
    NetClock::duration const& closeResolution,
    ConsensusCloseTimes const& rawCloseTimes,
    ConsensusMode const& mode,
    Json::Value && consensusJson)
{
    doAccept(
        result,
        prevLedger,
        closeResolution,
        rawCloseTimes,
        mode,
        std::move(consensusJson));
}

std::pair<std::shared_ptr<Ledger const> const, bool>
Adaptor::checkLedgerAccept(uint256 const& hash, std::uint32_t seq)
{
    std::size_t valCount = 0;

    if (seq != 0)
    {
        // Ledger is too old
        if (seq < ledgerMaster_.getValidLedgerIndex())
            return { nullptr, false };

        valCount = app_.getValidations().numTrustedForLedger(hash);

        if (valCount >= app_.validators().quorum())
        {
            ledgerMaster_.setLastValidLedger(hash, seq);
        }

        if (seq == ledgerMaster_.getValidLedgerIndex())
            return { nullptr, false };

        // Ledger could match the ledger we're already building
        if (seq == ledgerMaster_.getBuildingLedger())
            return { nullptr, false };
    }

    auto ledger = ledgerMaster_.getLedgerHistory().getLedgerByHash(hash);

    if (!ledger)
    {
        if ((seq != 0) && (getValidLedgerIndex() == 0))
        {
            // Set peers sane early if we can
            if (valCount >= app_.validators().quorum())
                app_.overlay().checkSanity(seq);
        }

        // FIXME: We may not want to fetch a ledger with just one
        // trusted validation
        ledger = app_.getInboundLedgers().acquire(
            hash, seq, InboundLedger::Reason::GENERIC);
    }

    if (ledger)
        return { ledger, checkLedgerAccept(ledger) };

    return { nullptr, false };
}

bool Adaptor::checkLedgerAccept(std::shared_ptr<Ledger const> const& ledger)
{
    LedgerMaster::ScopedLockType ml(ledgerMaster_.peekMutex());

    if (ledger->info().seq <= ledgerMaster_.getValidLedgerIndex())
        return false;

    auto const minVal = getNeededValidations();
    auto const tvc = app_.getValidations().numTrustedForLedger(ledger->info().hash);
    if (tvc < minVal) // nothing we can do
    {
        JLOG(j_.trace()) <<
            "Only " << tvc <<
            " validations for " << ledger->info().hash;
        return false;
    }

    JLOG(j_.info())
        << "Advancing accepted ledger to " << ledger->info().seq
        << " with >= " << minVal << " validations";

    return true;
}

void Adaptor::sendViewChange(ViewChange const& change)
{
    protocol::TMViewChange msg;
    auto signingHash = change.signingHash();
    auto sig = signDigest(valPublic_, valSecret_, signingHash);

    msg.set_previousledgerseq(change.prevSeq());
    msg.set_previousledgerhash(change.prevHash().begin(), change.prevHash().size());
    msg.set_nodepubkey(valPublic_.data(), valPublic_.size());
    msg.set_toview(change.toView());
    msg.set_signature(sig.data(), sig.size());

    app_.overlay().send(msg);
}

void Adaptor::touchAcquringLedger(LedgerHash const& prevLedgerHash)
{
    auto inboundLedger = app_.getInboundLedgers().find(prevLedgerHash);
    if (inboundLedger)
    {
        inboundLedger->touch();
    }
}

bool Adaptor::handleNewValidation(STValidation::ref val, std::string const& source)
{
    PublicKey const& signingKey = val->getSignerPublic();
    uint256 const& hash = val->getLedgerHash();

    // Ensure validation is marked as trusted if signer currently trusted
    boost::optional<PublicKey> masterKey =
        app_.validators().getTrustedKey(signingKey);
    if (!val->isTrusted() && masterKey)
        val->setTrusted();

    // If not currently trusted, see if signer is currently listed
    if (!masterKey)
        masterKey = app_.validators().getListedKey(signingKey);

    bool shouldRelay = false;
    RCLValidations& validations = app_.getValidations();
    beast::Journal j = validations.adaptor().journal();

    auto dmp = [&](beast::Journal::Stream s, std::string const& msg) {
        s << "Val for " << hash
            << (val->isTrusted() ? " trusted/" : " UNtrusted/")
            << (val->isFull() ? "full" : "partial") << " from "
            << (masterKey ? toBase58(TokenType::NodePublic, *masterKey)
                : "unknown")
            << " signing key "
            << toBase58(TokenType::NodePublic, signingKey) << " " << msg
            << " src=" << source;
    };

    if (!val->isFieldPresent(sfLedgerSequence))
    {
        if (j.error())
            dmp(j.error(), "missing ledger sequence field");
        return false;
    }

    // masterKey is seated only if validator is trusted or listed
    if (masterKey)
    {
        ValStatus const outcome = validations.add(calcNodeID(*masterKey), val);
        if (j.debug())
            dmp(j.debug(), to_string(outcome));

        if (outcome == ValStatus::badSeq && j.warn())
        {
            auto const seq = val->getFieldU32(sfLedgerSequence);
            dmp(j.warn(),
                "already validated sequence at or past " + to_string(seq));
        }

        if (val->isTrusted() && outcome == ValStatus::current)
        {
            auto result = checkLedgerAccept(hash, val->getFieldU32(sfLedgerSequence));
            if (result.first && result.second)
            {
                ledgerMaster_.doValid(result.first);
            }
            shouldRelay = true;
        }
    }
    else
    {
        JLOG(j.debug()) << "Val for " << hash << " from "
            << toBase58(TokenType::NodePublic, signingKey)
            << " not added UNlisted";
    }

    // This currently never forwards untrusted validations, though we may
    // reconsider in the future. From @JoelKatz:
    // The idea was that we would have a certain number of validation slots with
    // priority going to validators we trusted. Remaining slots might be
    // allocated to validators that were listed by publishers we trusted but
    // that we didn't choose to trust. The shorter term plan was just to forward
    // untrusted validations if peers wanted them or if we had the
    // ability/bandwidth to. None of that was implemented.
    return shouldRelay;
}

// ------------------------------------------------------------------------
// Protected member functions

RCLCxLedger Adaptor::buildLCL(
    RCLCxLedger const& previousLedger,
    CanonicalTXSet& retriableTxs,
    NetClock::time_point closeTime,
    bool closeTimeCorrect,
    NetClock::duration closeResolution,
    std::chrono::milliseconds roundTime,
    std::set<TxID>& failedTxs)
{
    std::shared_ptr<Ledger> built = [&]()
    {

        if (auto const replayData = ledgerMaster_.releaseReplay())
        {
            assert(replayData->parent()->info().hash == previousLedger.id());
            return buildLedger(*replayData, tapNO_CHECK_SIGN | tapForConsensus, app_, j_);
        }
        return buildLedger(previousLedger.ledger_, closeTime, closeTimeCorrect,
            closeResolution, app_, retriableTxs, failedTxs, j_);
    }();

    auto v2_enabled = built->rules().enabled(featureSHAMapV2);
    auto disablev2_enabled = built->rules().enabled(featureDisableV2);

    if (disablev2_enabled && built->stateMap().is_v2())
    {
        built->make_v1();
        JLOG(j_.warn()) << "Begin transfer to v1,LedgerSeq = " << built->info().seq;
    }
    else if (!disablev2_enabled && v2_enabled && !built->stateMap().is_v2())
    {
        built->make_v2();
        JLOG(j_.warn()) << "Begin transfer to v2,LedgerSeq = " << built->info().seq;
    }

    // Update fee computations based on accepted txs
    using namespace std::chrono_literals;
    app_.getTxQ().processClosedLedger(app_, *built, roundTime > 5s);

    // And stash the ledger in the ledger master
    if (ledgerMaster_.storeLedger(built))
        JLOG(j_.debug()) << "Consensus built ledger we already had";
    else if (app_.getInboundLedgers().find(built->info().hash))
        JLOG(j_.debug()) << "Consensus built ledger we were acquiring";
    else
        JLOG(j_.debug()) << "Consensus built new ledger";
    return RCLCxLedger{ std::move(built) };
}

void Adaptor::consensusBuilt(
    std::shared_ptr<Ledger const> const& ledger,
    uint256 const& consensusHash,
    Json::Value consensus)
{

    // Because we just built a ledger, we are no longer building one
    ledgerMaster_.setBuildingLedger(0);

    // No need to process validations in standalone mode
    if (app_.config().standalone())
        return;

    ledgerMaster_.getLedgerHistory().builtLedger(ledger, consensusHash, std::move(consensus));

    if (ledger->info().seq <= ledgerMaster_.getValidLedgerIndex())
    {
        auto stream = app_.journal("LedgerConsensus").info();
        JLOG(stream)
            << "Consensus built old ledger: "
            << ledger->info().seq << " <= " << ledgerMaster_.getValidLedgerIndex();
        return;
    }

    // See if this ledger can be the new fully-validated ledger
    if (checkLedgerAccept(ledger))
    {
        ledgerMaster_.doValid(ledger);
    }

    if (ledger->info().seq <= ledgerMaster_.getValidLedgerIndex())
    {
        auto stream = app_.journal("LedgerConsensus").debug();
        JLOG(stream) << "Consensus ledger fully validated";
        return;
    }

    // This ledger cannot be the new fully-validated ledger, but
    // maybe we saved up validations for some other ledger that can be

    auto const val = app_.getValidations().currentTrusted();

    // Track validation counts with sequence numbers
    class valSeq
    {
    public:

        valSeq() : valCount_(0), ledgerSeq_(0) { ; }

        void mergeValidation(LedgerIndex seq)
        {
            valCount_++;

            // If we didn't already know the sequence, now we do
            if (ledgerSeq_ == 0)
                ledgerSeq_ = seq;
        }

        std::size_t valCount_;
        LedgerIndex ledgerSeq_;
    };

    // Count the number of current, trusted validations
    hash_map <uint256, valSeq> count;
    for (auto const& v : val)
    {
        valSeq& vs = count[v->getLedgerHash()];
        vs.mergeValidation(v->getFieldU32(sfLedgerSequence));
    }

    auto const neededValidations = getNeededValidations();
    auto maxSeq = ledgerMaster_.getValidLedgerIndex();
    auto maxLedger = ledger->info().hash;

    // Of the ledgers with sufficient validations,
    // find the one with the highest sequence
    for (auto& v : count)
        if (v.second.valCount_ > neededValidations)
        {
            // If we still don't know the sequence, get it
            if (v.second.ledgerSeq_ == 0)
            {
                if (auto ledger = ledgerMaster_.getLedgerByHash(v.first))
                    v.second.ledgerSeq_ = ledger->info().seq;
            }

            if (v.second.ledgerSeq_ > maxSeq)
            {
                maxSeq = v.second.ledgerSeq_;
                maxLedger = v.first;
            }
        }

    if (maxSeq > ledgerMaster_.getCurrentLedgerIndex())
    {
        auto stream = app_.journal("LedgerConsensus").debug();
        JLOG(stream) << "Consensus triggered check of ledger";
        auto result = checkLedgerAccept(maxLedger, maxSeq);
        if (result.first && result.second)
        {
            ledgerMaster_.doValid(result.first);
        }
    }
}

std::size_t Adaptor::getNeededValidations()
{
    return app_.config().standalone() ? 0 : app_.validators().quorum();
}

void Adaptor::validate(RCLCxLedger const& ledger, RCLTxSet const& txns, bool proposing)
{
    using namespace std::chrono_literals;
    auto validationTime = app_.timeKeeper().closeTime();
    if (validationTime <= lastValidationTime_)
        validationTime = lastValidationTime_ + 1s;
    lastValidationTime_ = validationTime;

    STValidation::FeeSettings fees;
    std::vector<uint256> amendments;

    auto const& feeTrack = app_.getFeeTrack();
    std::uint32_t fee =
        std::max(feeTrack.getLocalFee(), feeTrack.getClusterFee());

    if (fee > feeTrack.getLoadBase())
        fees.loadFee = fee;

    // next ledger is flag ledger
    if (((ledger.seq() + 1) % 256) == 0)
    {
        // Suggest fee changes and new features
        feeVote_->doValidation(ledger.ledger_, fees);
        amendments = app_.getAmendmentTable().doValidation(getEnabledAmendments(*ledger.ledger_));
    }

    auto v = std::make_shared<STValidation>(
        ledger.id(),
        ledger.seq(),
        txns.id(),
        validationTime,
        valPublic_,
        valSecret_,
        nodeID_,
        proposing /* full if proposed */,
        fees,
        amendments);

    // suppress it if we receive it
    app_.getHashRouter().addSuppression(
        sha512Half(makeSlice(v->getSerialized())));
    handleNewValidation(v, "local");
    Blob validation = v->getSerialized();
    protocol::TMValidation val;
    val.set_validation(&validation[0], validation.size());
    // Send signed validation to all of our directly connected peers
    app_.overlay().send(val);
}


}