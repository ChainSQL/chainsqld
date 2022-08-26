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
#include <ripple/app/ledger/BuildLedger.h>
#include <ripple/app/ledger/TransactionMaster.h>
#include <ripple/app/ledger/LocalTxs.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/digest.h>
#include <ripple/basics/random.h>
#include <ripple/overlay/predicates.h>
#include <peersafe/schema/PeerManager.h>
#include <peersafe/consensus/Adaptor.h>
#include <peersafe/consensus/ConsensusBase.h>
#include <peersafe/app/misc/TxPool.h>
#include <peersafe/app/util/Common.h>


namespace ripple {

Adaptor::Adaptor(
    Schema& app,
    std::unique_ptr<FeeVote>&& feeVote,
    LedgerMaster& ledgerMaster,
    InboundTransactions& inboundTransactions,
    ValidatorKeys const& validatorKeys,
    beast::Journal journal,
    LocalTxs& localTxs)
    : app_(app)
    , j_(journal)
    , feeVote_(std::move(feeVote))
    , ledgerMaster_(ledgerMaster)
    , inboundTransactions_{inboundTransactions}
    , nodeID_{validatorKeys.nodeID}
    , valPublic_{validatorKeys.publicKey}
    , valSecret_{validatorKeys.secretKey}
    , valCookie_{rand_int<std::uint64_t>(
          1,
          std::numeric_limits<std::uint64_t>::max())}
    , nUnlVote_(nodeID_, j_)
    , localTxs_(localTxs)
{
    assert(valCookie_ != 0);
}

bool
Adaptor::preStartRound(
    RCLCxLedger const& prevLgr,
    hash_set<NodeID> const& nowTrusted)
{
    // We have a key, we do not want out of sync validations after a restart
    // and are not amendment blocked.
	validating_ = valPublic_.size() != 0 &&
		// prevLgr.seq() >= app_.getMaxDisallowedLedger() &&
		!app_.getOPs().isAmendmentBlocked();

	if (app_.schemaId() == beast::zero && app_.config().ONLY_VALIDATE_FOR_SCHEMA)
		validating_ = false;
		

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

    const bool synced = app_.getOPs().getOperatingMode() == OperatingMode::FULL;

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

    // Notify NegativeUNLVote that new validators are added
    if (prevLgr.ledger_->rules().enabled(featureNegativeUNL) &&
        !nowTrusted.empty())
        nUnlVote_.newValidators(prevLgr.seq() + 1, nowTrusted);

    // propose only if we're in sync with the network (and validating)
    return validating_ && synced;
}


void
Adaptor::InitAnnounce(STInitAnnounce const& initAnnounce, boost::optional<PublicKey> pubKey /* = boost::none */)
{
    Blob v = initAnnounce.getSerialized();

    protocol::TMConsensus consensus;

    consensus.set_msg(&v[0], v.size());
    consensus.set_msgtype(ConsensusMessageType::mtINITANNOUNCE);
    consensus.set_schemaid(app_.schemaId().begin(), uint256::size());

    if (pubKey)
        signAndSendMessage(*pubKey, consensus); 
    else
        signAndSendMessage(consensus);
}

void
Adaptor::signMessage(protocol::TMConsensus& consensus)
{
    consensus.set_signerpubkey(valPublic_.data(), valPublic_.size());

    auto const signingHash = sha512Half(makeSlice(consensus.msg()));
    auto const& signature = signDigest(valPublic_, valSecret_, signingHash);

    consensus.set_signature(signature.data(), signature.size());
}

void
Adaptor::signAndSendMessage(protocol::TMConsensus& consensus)
{
    signMessage(consensus);

    // suppress it if we receive it
    app_.getHashRouter().addSuppression(consensusMessageUniqueId(consensus));

    // Send signed consensus message to all of our directly connected peers
    app_.peerManager().send(consensus);
}

void
Adaptor::signAndSendMessage(
    std::shared_ptr<Peer> peer,
    protocol::TMConsensus& consensus)
{
    signMessage(consensus);

    // suppress it if we receive it
    app_.getHashRouter().addSuppression(consensusMessageUniqueId(consensus));

    // Send signed consensus message to all of our directly connected peers
    app_.peerManager().send(peer, consensus);
}

void
Adaptor::signAndSendMessage(
    PublicKey const& pubKey,
    protocol::TMConsensus& consensus)
{
    signMessage(consensus);

    // suppress it if we receive it
    app_.getHashRouter().addSuppression(consensusMessageUniqueId(consensus));

    // Send signed consensus message to all of our directly connected peers
    app_.peerManager().send(pubKey, consensus);
}

boost::optional<RCLTxSet>
Adaptor::acquireTxSet(RCLTxSet::ID const& setId)
{
    if (auto txns = inboundTransactions_.getSet(setId, true))
    {
        return RCLTxSet{std::move(txns)};
    }
    return boost::none;
}

boost::optional<RCLCxLedger>
Adaptor::acquireLedger(LedgerHash const& hash)
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

            app_.getJobQueue().addJob(
                jtADVANCE,
                "getConsensusLedger",
                [id = hash, &app = app_](Job&) {
                    app.getInboundLedgers().acquire(
                        id, 0, InboundLedger::Reason::CONSENSUS);
                }, app_.doJobCounter());
        }
        return boost::none;
    }

    assert(!built->open() && built->isImmutable());
    assert(built->info().hash == hash);

    // Notify inbound transactions of the new ledger sequence number
    inboundTransactions_.newRound(built->info().seq);

    return RCLCxLedger(built);
}

void
Adaptor::touchAcquringLedger(LedgerHash const& prevLedgerHash)
{
    auto inboundLedger = app_.getInboundLedgers().find(prevLedgerHash);
    if (inboundLedger && !inboundLedger->isComplete() && !inboundLedger->isFailed())
    {
        JLOG(j_.info()) << "touch inboundLedger for " << prevLedgerHash;
        inboundLedger->touch();
    }
}

RCLCxLedger
Adaptor::buildLCL(
    RCLCxLedger const& previousLedger,
    CanonicalTXSet& retriableTxs,
    NetClock::time_point closeTime,
    bool closeTimeCorrect,
    NetClock::duration closeResolution,
    std::chrono::milliseconds roundTime,
    std::set<TxID>& failedTxs)
{
    std::shared_ptr<Ledger> built = [&]() {
        if (auto const replayData = ledgerMaster_.releaseReplay())
        {
            assert(replayData->parent()->info().hash == previousLedger.id());
            return buildLedger(
                *replayData, tapNO_CHECK_SIGN | tapForConsensus, app_, j_);
        }

        return buildLedger(
            previousLedger.ledger_,
            closeTime,
            closeTimeCorrect,
            closeResolution,
            app_,
            retriableTxs,
            failedTxs,
            j_);
    }();

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
    return RCLCxLedger{std::move(built)};
}

std::shared_ptr<Ledger const>
Adaptor::checkLedgerAccept(LedgerInfo const& info)
{
    auto ledger = ledgerMaster_.getLedgerByHash(info.hash);
    if (!ledger || ledger->seq() != info.seq)
    {
        return nullptr;
    }

    std::lock_guard ml(ledgerMaster_.peekMutex());

    if (info.seq <= getValidLedgerIndex())
        return nullptr;

    return ledger;
}

void
Adaptor::onModeChange(ConsensusMode before, ConsensusMode after)
{
    if (before != after)
    {
        JLOG(j_.info()) << "Consensus mode change before=" << to_string(before)
                        << ", after=" << to_string(after);
        mode_ = after;
    }
}

TrustChanges
Adaptor::onConsensusReached(bool waitingConsensusReach, Ledger_t previousLedger, uint64_t curTurn)
{
    app_.getLedgerMaster().onConsensusReached(
        waitingConsensusReach, previousLedger.ledger_);

    if (waitingConsensusReach)
    {
        notify(app_, protocol::neSWITCHED_LEDGER, previousLedger, true, j_);
    }
    if (app_.openLedger().current()->info().seq != previousLedger.seq() + 1)
    {
        // Generate new openLedger
        CanonicalTXSet retriableTxs{beast::zero};
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
        notify(app_,
            protocol::neCLOSING_LEDGER,
            previousLedger,
            mode() != ConsensusMode::wrongLedger,
            j_);
    }

    return app_.validators().updateTrustedAndBroadcast(
        app_.getValidations().getCurrentNodeIDs(),
        app_.schemaId(),
        previousLedger.seq() + 1,
        curTurn,
        app_.peerManager(),
        app_.getHashRouter());
}

}  // namespace ripple