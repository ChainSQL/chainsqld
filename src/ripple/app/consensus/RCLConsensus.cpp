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
#include <ripple/app/consensus/RCLConsensus.h>
#include <ripple/app/consensus/RCLValidations.h>
#include <ripple/app/ledger/InboundLedgers.h>
#include <ripple/app/ledger/InboundTransactions.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/ledger/TransactionMaster.h>
#include <ripple/app/ledger/LocalTxs.h>
#include <ripple/app/ledger/OpenLedger.h>
#include <ripple/app/misc/AmendmentTable.h>
#include <ripple/app/misc/HashRouter.h>
#include <ripple/app/misc/LoadFeeTrack.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/app/misc/TxQ.h>
#include <ripple/app/misc/ValidatorKeys.h>
#include <ripple/app/misc/ValidatorList.h>
#include <ripple/app/tx/apply.h>
#include <ripple/basics/make_lock.h>
#include <ripple/beast/core/LexicalCast.h>
#include <ripple/consensus/LedgerTiming.h>
#include <ripple/overlay/Overlay.h>
#include <ripple/overlay/predicates.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/digest.h>
#include <ripple/app/misc/Transaction.h>
#include <peersafe/app/misc/StateManager.h>
#include <utility>
#if USE_TBB
#ifdef _CRTDBG_MAP_ALLOC
#pragma push_macro("free")
#undef free
#include <tbb/parallel_for.h>
#pragma pop_macro("free")
#else
#include <tbb/parallel_for.h>
#endif
#include <tbb/concurrent_vector.h>
#include <tbb/blocked_range.h>
#include <peersafe/app/tx/ParallelApply.h>
#endif

namespace ripple {

RCLConsensus::RCLConsensus(
    Application& app,
    std::unique_ptr<FeeVote>&& feeVote,
    LedgerMaster& ledgerMaster,
    LocalTxs& localTxs,
    InboundTransactions& inboundTransactions,
    Consensus<Adaptor>::clock_type const& clock,
    ValidatorKeys const& validatorKeys,
    beast::Journal journal)
    : adaptor_(
          app,
          std::move(feeVote),
          ledgerMaster,
          localTxs,
          inboundTransactions,
          validatorKeys,
          journal)
    , consensus_ripple_(std::make_shared<Consensus<Adaptor>>(clock, adaptor_, journal))
	, consensus_peersafe_(std::make_shared<PConsensus<Adaptor>>(clock, adaptor_, journal))
    , j_(journal)

{
	//consensus_ = consensus_ripple_;
	consensus_ = consensus_peersafe_;
}

RCLConsensus::Adaptor::Adaptor(
    Application& app,
    std::unique_ptr<FeeVote>&& feeVote,
    LedgerMaster& ledgerMaster,
    LocalTxs& localTxs,
    InboundTransactions& inboundTransactions,
    ValidatorKeys const& validatorKeys,
    beast::Journal journal)
    : app_(app)
        , feeVote_(std::move(feeVote))
        , ledgerMaster_(ledgerMaster)
        , localTxs_(localTxs)
        , inboundTransactions_{inboundTransactions}
        , j_(journal)
        , nodeID_{calcNodeID(app.nodeIdentity().first)}
        , valPublic_{validatorKeys.publicKey}
        , valSecret_{validatorKeys.secretKey}
{
}

void RCLConsensus::Adaptor::touchAcquringLedger(LedgerHash const& prevLedgerHash)
{
	auto inboundLedger = app_.getInboundLedgers().find(prevLedgerHash);
	if (inboundLedger)
	{
		inboundLedger->touch();
	}
}

boost::optional<RCLCxLedger>
RCLConsensus::Adaptor::acquireLedger(LedgerHash const& ledger)
{
    // we need to switch the ledger we're working from
    auto buildLCL = ledgerMaster_.getLedgerByHash(ledger);
    if (!buildLCL)
    {
        if (acquiringLedger_ != ledger)
        {
            // need to start acquiring the correct consensus LCL
            JLOG(j_.warn()) << "Need consensus ledger " << ledger;

            // Tell the ledger acquire system that we need the consensus ledger
            acquiringLedger_ = ledger;

            auto app = &app_;
            auto hash = acquiringLedger_;
            app_.getJobQueue().addJob(
                jtADVANCE, "getConsensusLedger", [app, hash](Job&) {
                    app->getInboundLedgers().acquire(
                        hash, 0, InboundLedger::fcCONSENSUS);
                });
        }
        return boost::none;
    }

    assert(!buildLCL->open() && buildLCL->isImmutable());
    assert(buildLCL->info().hash == ledger);

    // Notify inbound transactions of the new ledger sequence number
    inboundTransactions_.newRound(buildLCL->info().seq);

    // Use the ledger timing rules of the acquired ledger
    parms_.useRoundedCloseTime = buildLCL->rules().enabled(fix1528);

    return RCLCxLedger(buildLCL);
}


void
RCLConsensus::Adaptor::relay(RCLCxPeerPos const& peerPos)
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

void
RCLConsensus::Adaptor::relay(RCLCxTx const& tx)
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
        app_.overlay().foreach (send_always(
            std::make_shared<Message>(msg, protocol::mtTRANSACTION)));
    }
    else
    {
        JLOG(j_.debug()) << "Not relaying disputed tx " << tx.id();
    }
}
void
RCLConsensus::Adaptor::propose(RCLCxPeerPos::Proposal const& proposal)
{
    JLOG(j_.trace()) << "We propose: "
                     << (proposal.isBowOut()
                             ? std::string("bowOut")
                             : ripple::to_string(proposal.position()));

    protocol::TMProposeSet prop;

    prop.set_currenttxhash(
        proposal.position().begin(), proposal.position().size());
    prop.set_microledgersethash(
        proposal.position2().first.begin(), proposal.position2().first.size());
    prop.set_emptyledgers(proposal.position2().second);
    prop.set_previousledger(
        proposal.prevLedger().begin(), proposal.position().size());
	prop.set_curledgerseq(proposal.curLedgerSeq());
	prop.set_view(proposal.view());
    prop.set_proposeseq(proposal.proposeSeq());
    prop.set_closetime(proposal.closeTime().time_since_epoch().count());
    prop.set_shardid(proposal.shardID());

    prop.set_nodepubkey(valPublic_.data(), valPublic_.size());

    auto signingHash = sha512Half(
        HashPrefix::proposal,
        std::uint32_t(proposal.proposeSeq()),
        proposal.closeTime().time_since_epoch().count(),
        proposal.prevLedger(),
        proposal.position(),
        proposal.position2().first,
        proposal.position2().second);

    auto sig = signDigest(valPublic_, valSecret_, signingHash);

    prop.set_signature(sig.data(), sig.size());

    auto const m = std::make_shared<Message>(prop, protocol::mtPROPOSE_LEDGER);

    app_.getShardManager().nodeBase().sendMessage(m);
    //app_.overlay().send(prop);
}

void
RCLConsensus::Adaptor::sendViewChange(ViewChange& change)
{
	protocol::TMViewChange msg;
	auto signingHash = change.signingHash();
	auto sig = signDigest(valPublic_, valSecret_, signingHash);
    change.setSignatrue(sig);

    msg.set_reason((protocol::TMViewChange_genReason)change.genReason());
	msg.set_previousledgerseq(change.prevSeq());
	msg.set_previousledgerhash(change.prevHash().begin(), change.prevHash().size());
	msg.set_nodepubkey(valPublic_.data(),valPublic_.size());
	msg.set_toview(change.toView());
	msg.set_signature(sig.data(), sig.size());

    auto const m = std::make_shared<Message>(msg, protocol::mtVIEW_CHANGE);

    app_.getShardManager().nodeBase().sendMessage(m);
	//app_.overlay().send(msg);
}

void
RCLConsensus::Adaptor::relay(uint256 setID, RCLTxSet const& set)
{
    inboundTransactions_.giveSet(setID, set.map_, false);
}

void
RCLConsensus::Adaptor::relay(RCLTxSet const& set)
{
    inboundTransactions_.giveSet(set.id(), set.map_, false);
}

boost::optional<RCLTxSet>
RCLConsensus::Adaptor::acquireTxSet(RCLTxSet::ID const& setId)
{
    if (auto set = inboundTransactions_.getSet(setId, true))
    {
        return RCLTxSet{std::move(set)};
    }
    return boost::none;
}

bool
RCLConsensus::Adaptor::hasOpenTransactions() const
{
    return !app_.openLedger().empty();
}

std::size_t
RCLConsensus::Adaptor::proposersValidated(LedgerHash const& h) const
{
    return app_.getValidations().numTrustedForLedger(h);
}

std::size_t
RCLConsensus::Adaptor::proposersFinished(LedgerHash const& h) const
{
    return app_.getValidations().getNodesAfter(h);
}

uint256
RCLConsensus::Adaptor::getPrevLedger(
    uint256 ledgerID,
    RCLCxLedger const& ledger,
    ConsensusMode mode)
{
    uint256 parentID;
    // Only set the parent ID if we believe ledger is the right ledger
    if (mode != ConsensusMode::wrongLedger)
        parentID = ledger.parentID();

    // Get validators that are on our ledger, or "close" to being on
    // our ledger.
    hash_map<uint256, std::uint32_t> ledgerCounts =
        app_.getValidations().currentTrustedDistribution(
            ledgerID, parentID, ledgerMaster_.getValidLedgerIndex());

    uint256 netLgr = ledgerID;
    int netLgrCount = 0;
    for (auto const & it : ledgerCounts)
    {
        // Switch to ledger supported by more peers
        // Or stick with ours on a tie
        if ((it.second > netLgrCount) ||
            ((it.second == netLgrCount) && (it.first == ledgerID)))
        {
            netLgr = it.first;
            netLgrCount = it.second;
        }
    }

    if (netLgr != ledgerID)
    {
        if (mode != ConsensusMode::wrongLedger)
            app_.getOPs().consensusViewChange();

        if (auto stream = j_.info())
        {
            for (auto const & it : ledgerCounts)
                stream << "V: " << it.first << ", " << it.second;
        }
    }

    return netLgr;
}

auto
RCLConsensus::Adaptor::onClose(
    RCLCxLedger const& ledger,
    NetClock::time_point const& closeTime,
    ConsensusMode mode) -> Result
{
    const bool wrongLCL = mode == ConsensusMode::wrongLedger;
    const bool proposing = mode == ConsensusMode::proposing;

    notify(protocol::neCLOSING_LEDGER, ledger, !wrongLCL);

    auto const& prevLedger = ledger.ledger_;

    ledgerMaster_.applyHeldTransactions();
    // Tell the ledger master not to acquire the ledger we're probably building
    ledgerMaster_.setBuildingLedger(prevLedger->info().seq + 1);

    auto initialLedger = app_.openLedger().current();

    auto initialSet = std::make_shared<SHAMap>(
        SHAMapType::TRANSACTION, app_.family(), SHAMap::version{1});
    initialSet->setUnbacked();

    // Build SHAMap containing all transactions in our open ledger
    for (auto const& tx : initialLedger->txs)
    {
        JLOG(j_.trace()) << "Adding open ledger TX " <<
            tx.first->getTransactionID();
        Serializer s(2048);
        tx.first->add(s);
        initialSet->addItem(
            SHAMapItem(tx.first->getTransactionID(), std::move(s)),
            true,
            false);
    }

    // Add pseudo-transactions to the set
    if ((app_.config().standalone() || (proposing && !wrongLCL)) &&
        ((prevLedger->info().seq % 256) == 0))
    {
        // previous ledger was flag ledger, add pseudo-transactions
        auto const validations =
            app_.getValidations().getTrustedForLedger (
                prevLedger->info().parentHash);

        if (validations.size() >= app_.validators ().quorum ())
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
            initialLedger->info().parentHash,
            RCLCxPeerPos::Proposal::seqJoin,
            setHash,
            closeTime,
            app_.timeKeeper().closeTime(),
            nodeID_}};
}

void 
RCLConsensus::Adaptor::onViewChanged(bool bWaitingInit, Ledger_t previousLedger)
{
	app_.getLedgerMaster().onViewChanged(true, bWaitingInit, previousLedger.ledger_);

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

auto
RCLConsensus::Adaptor::onCollectFinish(
	RCLCxLedger const& ledger,
	std::vector<uint256>& transactions,
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

    ShardManager& shardMgr = app_.getShardManager();

    std::pair<uint256, bool> position2 = std::make_pair(beast::zero, true);
    if (shardMgr.myShardRole() == ShardManager::COMMITTEE)
    {
        position2 = shardMgr.committee().microLedgerSetHash();

        // All microLedger is empty, pre-execute transactions
        if (position2.second)
        {
            for (auto const& txID : transactions)
            {
                auto tx = app_.getMasterTransaction().fetch(txID, false);
                if (!tx)
                {
                    JLOG(j_.error()) << "fetch transaction " + to_string(txID) + "failed";
                    continue;
                }
                app_.getOPs().doTransactionAsync(tx, false, NetworkOPs::FailHard::no);
            }
            app_.getOPs().waitBatchComplete();

            transactions.clear();
            auto const& hTxVector = app_.getTxPool().topTransactions(maxTxsInLedger_);
            for (auto const& tx : hTxVector)
            {
                transactions.push_back(tx);
            }
        }
    }

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
            app_.getShardManager().myShardRole() == ShardManager::SHARD
            ? app_.getValidations().getTrustedForLedgerSeq(
                prevLedger->seq() - 1)
            : app_.getValidations().getTrustedForLedger(
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

    uint256 position = initialSet->getHash().as_uint256();

	return Result{
		std::move(initialSet),
		RCLCxPeerPos::Proposal{
			prevLedger->info().hash,
			prevLedger->info().seq + 1,
			view,
			RCLCxPeerPos::Proposal::seqJoin,
            position,
            position2,
			closeTime,
			app_.timeKeeper().closeTime(),
			nodeID_, 
            app_.getShardManager().node().shardID()} };
}

void
RCLConsensus::Adaptor::onForceAccept(
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

void
RCLConsensus::Adaptor::onAccept(
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
        [=, cj = std::move(consensusJson) ](auto&) mutable {
            // Note that no lock is held or acquired during this job.
            // This is because generic Consensus guarantees that once a ledger
            // is accepted, the consensus results and capture by reference state
            // will not change until startRound is called (which happens via
            // endConsensus).
			auto timeStart = utcTime();
            if (app_.getShardManager().myShardRole() == ShardManager::SHARD)
            {
                app_.getShardManager().node().doAccept(
                    result.set,
                    prevLedger,
                    result.position.closeTime());
            }
            else
            {
                this->doAccept(
                    result,
                    prevLedger,
                    closeResolution,
                    rawCloseTimes,
                    mode,
                    std::move(cj));
                {
                    std::lock_guard <std::recursive_mutex> lock(
                        app_.getLedgerMaster().peekMutex());
                    app_.getShardManager().committee().checkAccept();
                }
            }
			JLOG(j_.info()) << "doAccept time used:" << utcTime() - timeStart << "ms";
        });
}

void
RCLConsensus::Adaptor::doAccept(
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
        consensusCloseTime = prevLedger.closeTime() + 1s;
        closeTimeCorrect = false;
    }
    else
    {
		//Not need to round close time any more,just use leader's close time,adjust by prevLedger
		consensusCloseTime = std::max<NetClock::time_point>(consensusCloseTime, prevLedger.closeTime() + 1s);
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
    // Put transactions into a deterministic, but unpredictable, order
    CanonicalTXSet retriableTxs{result.set.id()};
	
	auto timeStart = utcTime();
    auto sharedLCL = buildLCL(
        prevLedger,
        result.set,
        consensusCloseTime,
        closeTimeCorrect,
        closeResolution,
        result.roundTime.read(),
        retriableTxs);
	JLOG(j_.info()) << "buildLCL time used:" << utcTime() - timeStart << "ms";

    app_.getOPs().getConsensus().setPhase(ConsensusPhase::validating);

	timeStart = utcTime();
    auto const newLCLHash = sharedLCL.id();
    JLOG(j_.debug()) << "Report: NewL  = " << newLCLHash << ":"
                     << sharedLCL.seq();

    // Tell directly connected peers that we have a new LCL
    notify(protocol::neACCEPTED_LEDGER, sharedLCL, haveCorrectLCL);

    if (validating_)
        validating_ = ledgerMaster_.isCompatible(
            *sharedLCL.ledger_,
            app_.journal("LedgerConsensus").warn(),
            "Not validating");

    if (validating_ && !consensusFail)
    {
        validate(sharedLCL, proposing);
        JLOG(j_.info()) << "CNF Val " << newLCLHash;
    }
    else
        JLOG(j_.info()) << "CNF buildLCL " << newLCLHash;

    // See if we can accept a ledger as fully-validated
    ledgerMaster_.consensusBuilt(sharedLCL.ledger_, std::move(consensusJson));

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
            sharedLCL.ledger_,
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

        app_.getLedgerMaster().processHeldTransactions();
    }
	JLOG(j_.info()) << "openLedger().accept time used:" << utcTime() - timeStart << "ms";
    //-------------------------------------------------------------------------
    {
        ledgerMaster_.switchLCL(sharedLCL.ledger_);

        // Do these need to exist?
        assert(ledgerMaster_.getClosedLedger()->info().hash == sharedLCL.id());
        assert(
            app_.openLedger().current()->info().parentHash == sharedLCL.id());
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
        auto offset = time_point{closeTotal} -
            std::chrono::time_point_cast<duration>(closeTime);
        JLOG(j_.info()) << "Our close offset is estimated at " << offset.count()
                        << " (" << closeCount << ")";

        app_.timeKeeper().adjustCloseTime(offset);
    }
}

void
RCLConsensus::Adaptor::notify(
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
    app_.overlay().foreach (
        send_always(std::make_shared<Message>(s, protocol::mtSTATUS_CHANGE)));

    JLOG(j_.trace()) << "send status change to peer";
}

/** Apply a set of transactions to a ledger.

  Typically the txFilter is used to reject transactions
  that already accepted in the prior ledger.

  @param set            set of transactions to apply
  @param view           ledger to apply to
  @param txFilter       callback, return false to reject txn
  @return               retriable transactions
*/
#if USE_TBB
CanonicalTXSet
applyTransactions(
    Application& app,
    RCLTxSet const& cSet,
    OpenView& view,
    std::function<bool(uint256 const&)> txFilter)
{
    auto j = app.journal("LedgerConsensus");

    auto& set = *(cSet.map_);
    CanonicalTXSet retriableTxs(set.getHash().as_uint256());
    ParallelApply::Txs shouldApplyTxs;
    ParallelApply::Txs retryTxs;

    for (auto const& item : set)
    {
        if (!txFilter(item.key()))
            continue;

        // The transaction wan't filtered
        // Add it to the set to be tried in canonical order
        JLOG(j.debug()) << "Processing candidate transaction: " << item.key();
        try
        {
            shouldApplyTxs.push_back(std::make_shared<STTx const>(SerialIter{ item.slice() }));
        }
        catch (std::exception const&)
        {
            JLOG(j.warn()) << "Txn " << item.key() << " throws";
        }
    }

    bool certainRetry = true;
    int changes = 0;

    ParallelApply parallelApply{ shouldApplyTxs, retryTxs, certainRetry, changes, app, view, j };

    // Attempt to apply all of the retriable transactions
    for (int pass = 0; pass < LEDGER_TOTAL_PASSES; ++pass)
    {
        JLOG(j.info()) << "Pass: " << pass << " Txns: " << shouldApplyTxs.size()
                        << (certainRetry ? " retriable" : " final");

        tbb::parallel_for(shouldApplyTxs.range(), parallelApply, tbb::auto_partitioner());

        JLOG(j.info()) << "Pass: " << pass << " finished " << changes
                        << " changes and retry Txns remaining " << retryTxs.size();

        // A non-retry pass made no changes
        if (!changes && !certainRetry)
            break;

        // Stop retriable passes
        if (!changes || (pass >= LEDGER_RETRY_PASSES))
            certainRetry = false;

        parallelApply.preRetry();
    }

    for (auto it = retryTxs.begin(); it != retryTxs.end(); ++it)
        retriableTxs.insert(*it);

    // If there are any transactions left, we must have
    // tried them in at least one final pass
    assert(retriableTxs.empty() || !certainRetry);
    return retriableTxs;
}

#else

std::shared_ptr<CanonicalTXSet>
applyTransactions(
    Application& app,
    RCLTxSet const& cSet,
    OpenView& view,
    std::function<bool(uint256 const&)> txFilter)
{
    auto j = app.journal("LedgerConsensus");

    std::uint32_t baseTxIndex = 0;
    if (app.getShardManager().myShardRole() == ShardManager::SHARD)
    {
        baseTxIndex = (app.getShardManager().node().shardID() - 1) * MaxTxsInLedger;
    }
    else
    {
        baseTxIndex = app.getShardManager().shardCount() * MaxTxsInLedger;
    }

    auto& set = *(cSet.map_);
    CanonicalTXSet retriableTxs(set.getHash().as_uint256());
    std::shared_ptr<CanonicalTXSet> processedTxs = std::make_shared<CanonicalTXSet>(set.getHash().as_uint256());

    for (auto const& item : set)
    {
        if (!txFilter(item.key()))
            continue;

        // The transaction wan't filtered
        // Add it to the set to be tried in canonical order
        JLOG(j.debug()) << "Processing candidate transaction: " << item.key();
        try
        {
            retriableTxs.insert(
                std::make_shared<STTx const>(SerialIter{ item.slice() }));
        }
        catch (std::exception const&)
        {
            JLOG(j.warn()) << "Txn " << item.key() << " throws";
        }
    }

    bool certainRetry = true;
    // Attempt to apply all of the retriable transactions
    for (int pass = 0; pass < LEDGER_TOTAL_PASSES; ++pass)
    {
        JLOG(j.debug()) << "Pass: " << pass << " Txns: " << retriableTxs.size()
            << (certainRetry ? " retriable" : " final");
        int changes = 0;

        auto it = retriableTxs.begin();

        while (it != retriableTxs.end())
        {
            try
            {
                switch (applyTransaction(
                    app, view, *it->second, baseTxIndex, certainRetry, tapNO_CHECK_SIGN | tapForConsensus, j))
                {
                case ApplyResult::Success:
                    processedTxs->insert(it->second);
                    it = retriableTxs.erase(it);
                    ++changes;
                    break;

                case ApplyResult::Fail:
                    processedTxs->insert(it->second);
                    it = retriableTxs.erase(it);
                    break;

                case ApplyResult::Retry:
                    ++it;
                }
            }
            catch (std::exception const&)
            {
                JLOG(j.warn()) << "Transaction throws";
                processedTxs->insert(it->second);
                it = retriableTxs.erase(it);
            }
        }

        JLOG(j.debug()) << "Pass: " << pass << " finished " << changes
            << " changes";

        // A non-retry pass made no changes
        if (!changes && !certainRetry)
            return processedTxs;

        // Stop retriable passes
        if (!changes || (pass >= LEDGER_RETRY_PASSES))
            certainRetry = false;
    }

    // If there are any transactions left, we must have
    // tried them in at least one final pass
    assert(retriableTxs.empty() || !certainRetry);
    return processedTxs;
}

#endif

void
applyMicroLedgers(
    Application& app,
    std::vector<std::shared_ptr<MicroLedger const>> const& microLedgers,
    OpenView& view)
{
    //boost::ignore_unused(app);
    auto j = app.journal("ApplyMicroLedger");

    // flag ledger
    if (((view.info().seq - 1) % 256) == 0)
    {
        view.initFeeShardVoting(app);
        view.initAmendmentSet();
    }

    for (auto const& microLedger : microLedgers)
    {
        microLedger->apply(view, j, app);
    }
}

void preExecTransactions(Application& app, OpenView const& view, RCLTxSet& cSet, unsigned maxTxsInLedger)
{
    auto j = app.journal("preExecTransactions");

    auto current_ = app.openLedger().current();
    app.openLedger().replace(view);

    int count = 0;

    for (auto const& item : *(cSet.map_))
    {
        count++;
        auto tx = app.getMasterTransaction().fetch(item.key(), false);
        if (!tx)
        {
            JLOG(j.error()) << "fetch transaction " + to_string(item.key()) + "failed";
            continue;
        }
        app.getOPs().doTransactionAsync(tx, false, NetworkOPs::FailHard::no);
    }
    app.getOPs().waitBatchComplete();

    app.openLedger().replace(*current_);

    auto const& hTxVector = app.getTxPool().topTransactions(maxTxsInLedger);

    // Re-build cSet use hTxVector Or Remove transactions which Pre-exec failed.
    // Perfer the second method, Does this cause bugs?
    std::vector<uint256> failedTxs;

    if (count != hTxVector.size())
    {
        for (auto const& item : *(cSet.map_))
        {
            if (std::find(hTxVector.begin(), hTxVector.end(), item.key()) == hTxVector.end())
            {
                if (((view.info().seq - 1) % 256) == 0)
                {
                    auto tx = std::make_shared<STTx const>(SerialIter{ item.slice() });
                    if (tx->getTxnType() == ttFEE || tx->getTxnType() == ttAMENDMENT)
                    {
                        continue;
                    }
                }

                failedTxs.push_back(item.key());
            }
        }
    }
    for (auto const& key : failedTxs)
    {
        cSet.map_->delItem(key);
    }
}

RCLCxLedger
RCLConsensus::Adaptor::buildLCL(
    RCLCxLedger const& previousLedger,
    RCLTxSet const& set,
    NetClock::time_point closeTime,
    bool closeTimeCorrect,
    NetClock::duration closeResolution,
    std::chrono::milliseconds roundTime,
    CanonicalTXSet& retriableTxs)
{
    auto replay = ledgerMaster_.releaseReplay();
    if (replay)
    {
        // replaying, use the time the ledger we're replaying closed
        closeTime = replay->closeTime_;
        closeTimeCorrect = ((replay->closeFlags_ & sLCF_NoConsensusTime) == 0);
    }

    JLOG(j_.debug()) << "Report: TxSt = " << set.id() << ", close "
                     << closeTime.time_since_epoch().count()
                     << (closeTimeCorrect ? "" : "X");

    // Build the new last closed ledger
    auto buildLCL =
        std::make_shared<Ledger>(*previousLedger.ledger_, closeTime);

    auto const v2_enabled = buildLCL->rules().enabled(featureSHAMapV2);
	auto const disablev2_enabled = buildLCL->rules().enabled(featureDisableV2);

	if (disablev2_enabled && buildLCL->stateMap().is_v2())
	{
		buildLCL->make_v1();
		JLOG(j_.warn()) << "Begin transfer to v1,LedgerSeq = " << buildLCL->info().seq;
	}
	else if (!disablev2_enabled && v2_enabled && !buildLCL->stateMap().is_v2())
	{
		buildLCL->make_v2();
		JLOG(j_.warn()) << "Begin transfer to v2,LedgerSeq = " << buildLCL->info().seq;
	}

    // Set up to write SHAMap changes to our database,
    //   perform updates, extract changes
    JLOG(j_.debug()) << "Applying consensus set transactions to the"
                     << " last closed ledger";

    OpenView accum(&*buildLCL);
    assert(!accum.open());

    {
        auto timeStart = utcTime();
        if (replay)
        {
            // Special case, we are replaying a ledger close
            for (auto& tx : replay->txns_)
                applyTransaction(
                    app_, accum, *tx.second, 0, false, tapNO_CHECK_SIGN | tapForConsensus, j_);
        }
        else
        {
            applyMicroLedgers(app_, app_.getShardManager().committee().canonicalMicroLedgers(),
                accum);
            JLOG(j_.info()) << "applyMicroLedgers time used:" << utcTime() - timeStart << "ms";

            if (set.map_->isMutable())
            {
                timeStart = utcTime();
                preExecTransactions(app_, accum, (RCLTxSet&)(*(&set)), maxTxsInLedger_);
                JLOG(j_.info()) << "preExecTransactions time used:" << utcTime() - timeStart << "ms";
            }

            timeStart = utcTime();
            auto canonicalTXSet = applyTransactions(
                app_, set, accum, [&buildLCL](uint256 const& txID) {
                    return !buildLCL->txExists(txID);
                });
            JLOG(j_.info()) << "applyTransactions " << canonicalTXSet->size() << "txs, time used:" << utcTime() - timeStart << "ms";

            auto const& microLedger0 = app_.getShardManager().committee().buildMicroLedger(accum, canonicalTXSet);

            if (((accum.info().seq - 1) % 256) == 0)
            {
                accum.finalVote(microLedger0, app_);
            }
        }
        // Update fee computations.
        //app_.getTxQ().processClosedLedger(app_, accum, roundTime > 5s);

        timeStart = utcTime();
        accum.apply(*buildLCL);
		JLOG(j_.info()) << "apply sle time used:" << utcTime() - timeStart << "ms";
    }

    // retriableTxs will include any transactions that
    // made it into the consensus set but failed during application
    // to the ledger.

    buildLCL->updateSkipList();

    {
        // Write the final version of all modified SHAMap
        // nodes to the node store to preserve the new LCL
		auto timeStart = utcTime();
        int asf = buildLCL->stateMap().flushDirty(
            hotACCOUNT_NODE, buildLCL->info().seq);
        int tmf = buildLCL->txMap().flushDirty(
            hotTRANSACTION_NODE, buildLCL->info().seq);
        JLOG(j_.debug()) << "Flushed " << asf << " accounts and " << tmf
                         << " transaction nodes";

		JLOG(j_.info()) << "flushDirty time used:" << utcTime() - timeStart << "ms";
    }
    buildLCL->unshare();

    // Accept ledger
    buildLCL->setAccepted(
        closeTime, closeResolution, closeTimeCorrect, app_.config());

    JLOG(j_.info()) << "buildLCL: " << buildLCL->info().hash;

    // Genarate final ledger
    app_.getShardManager().committee().buildFinalLedger(accum, buildLCL);

    // And stash the ledger in the ledger master
    if (ledgerMaster_.storeLedger(buildLCL))
        JLOG(j_.debug()) << "Consensus built ledger we already had";
    else if (app_.getInboundLedgers().find(buildLCL->info().hash))
        JLOG(j_.debug()) << "Consensus built ledger we were acquiring";
    else
        JLOG(j_.debug()) << "Consensus built new ledger";
    return RCLCxLedger{std::move(buildLCL)};
}

void
RCLConsensus::Adaptor::validate(RCLCxLedger const& ledger, bool proposing)
{
    auto validationTime = app_.timeKeeper().closeTime();
    if (validationTime <= lastValidationTime_)
        validationTime = lastValidationTime_ + 1s;
    lastValidationTime_ = validationTime;

    // Build validation
    auto v = std::make_shared<STValidation>(
        ledger.id(),
        app_.getShardManager().committee().getFinalLedgerHash(),
        app_.getShardManager().committee().getMicroLedgerHash(),
        validationTime, valPublic_, proposing);
    v->setFieldU32(sfLedgerSequence, ledger.seq());
    v->setFieldU32(sfShardID, app_.getShardManager().node().shardID());

    // Add our load fee to the validation
    auto const& feeTrack = app_.getFeeTrack();
    std::uint32_t fee =
        std::max(feeTrack.getLocalFee(), feeTrack.getClusterFee());

    if (fee > feeTrack.getLoadBase())
        v->setFieldU32(sfLoadFee, fee);

    if (((ledger.seq() + 1) % 256) == 0)
    // next ledger is flag ledger
    {
        // Suggest fee changes and new features
        feeVote_->doValidation(ledger.ledger_, *v);
        app_.getAmendmentTable().doValidation(ledger.ledger_, *v);
    }

    auto const signingHash = v->sign(valSecret_);
    v->setTrusted();
    // suppress it if we receive it - FIXME: wrong suppression
    app_.getHashRouter().addSuppression(signingHash);
    handleNewValidation(app_, v, "local");
    Blob validation = v->getSerialized();
    protocol::TMValidation val;
    val.set_validation(&validation[0], validation.size());
    // Send signed validation to all of our directly connected peers(committe node)
    //app_.overlay().send(val);
    auto const sm = std::make_shared<Message>(
        val, protocol::mtVALIDATION);
    app_.getShardManager().committee().sendMessage(sm);
}

void
RCLConsensus::Adaptor::onModeChange(
    ConsensusMode before,
    ConsensusMode after)
{
	if (before != after)
	{
		JLOG(j_.info()) << "Consensus mode change before=" << to_string(before)
			<< ", after=" << to_string(after);
		mode_ = after;
	}
}

Json::Value
RCLConsensus::getJson(bool full) const
{
    Json::Value ret;
    {
      ScopedLockType _{mutex_};
      ret = consensus_->getJson(full);
    }
    ret["validating"] = adaptor_.validating();
    return ret;
}

void
RCLConsensus::timerEntry(NetClock::time_point const& now)
{
    try
    {
        ScopedLockType _{mutex_};
        consensus_->timerEntry(now);
    }
    catch (SHAMapMissingNode const& mn)
    {
        // This should never happen
        JLOG(j_.error()) << "Missing node during consensus process " << mn;
        Rethrow();
    }
}

void
RCLConsensus::gotTxSet(NetClock::time_point const& now, RCLTxSet const& txSet)
{
    try
    {
        ScopedLockType _{mutex_};
        consensus_->gotTxSet(now, txSet);
    }
    catch (SHAMapMissingNode const& mn)
    {
        // This should never happen
        JLOG(j_.error()) << "Missing node during consensus process " << mn;
        Rethrow();
    }
}

void
RCLConsensus::gotMicroLedgerSet(NetClock::time_point const& now, uint256 const& mlSet)
{
    ScopedLockType _{ mutex_ };
    consensus_->gotMicroLedgerSet(now, mlSet);
}


//! @see Consensus::simulate

void
RCLConsensus::simulate(
    NetClock::time_point const& now,
    boost::optional<std::chrono::milliseconds> consensusDelay)
{
    ScopedLockType _{mutex_};
    consensus_->simulate(now, consensusDelay);
}

bool
RCLConsensus::peerProposal(
    NetClock::time_point const& now,
    RCLCxPeerPos const& newProposal)
{
    ScopedLockType _{mutex_};
    return consensus_->peerProposal(now, newProposal);
}

bool
RCLConsensus::peerViewChange(
	ViewChange const& change)
{
	ScopedLockType _{ mutex_ };
	return consensus_->peerViewChange(change);
}

bool
RCLConsensus::Adaptor::preStartRound(RCLCxLedger const & prevLgr)
{
    // We have a key and do not want out of sync validations after a restart,
    // and are not amendment blocked.
    validating_ = valPublic_.size() != 0 &&
                  //prevLgr.seq() >= app_.getMaxDisallowedLedger() &&
                  !app_.getOPs().isAmendmentBlocked();

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

    // Use parent ledger's rules to determine whether to use rounded close time
    parms_.useRoundedCloseTime = prevLgr.ledger_->rules().enabled(fix1528);

    // propose only if we're in sync with the network (and validating)
    return validating_ && synced;
}

void
RCLConsensus::startRound(
    NetClock::time_point const& now,
    RCLCxLedger::ID const& prevLgrId,
    RCLCxLedger const& prevLgr)
{
    ScopedLockType _{mutex_};
    consensus_->startRound(
        now, prevLgrId, prevLgr, adaptor_.preStartRound(prevLgr));
}

}
