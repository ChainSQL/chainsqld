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

#include <ripple/app/ledger/LocalTxs.h>
#include <ripple/app/ledger/TransactionMaster.h>
#include <ripple/app/misc/AmendmentTable.h>
#include <ripple/app/misc/LoadFeeTrack.h>
#include <ripple/core/ConfigSections.h>
#include <peersafe/consensus/ConsensusBase.h>
#include <peersafe/consensus/hotstuff/HotstuffAdaptor.h>
#include <peersafe/serialization/hotstuff/ExecutedBlock.h>
#include <peersafe/app/misc/StateManager.h>
#include <peersafe/schema/PeerManager.h>
#include <peersafe/app/util/NetworkUtil.h>

namespace ripple {

HotstuffAdaptor::HotstuffAdaptor(
    Schema& app,
    std::unique_ptr<FeeVote>&& feeVote,
    LedgerMaster& ledgerMaster,
    InboundTransactions& inboundTransactions,
    ValidatorKeys const& validatorKeys,
    beast::Journal journal,
    LocalTxs& localTxs,
    ConsensusParms const& consensusParms)
    : Adaptor(
          app,
          std::move(feeVote),
          ledgerMaster,
          inboundTransactions,
          validatorKeys,
          journal,
          localTxs)
{
    if (app_.config().exists(SECTION_CONSENSUS))
    {
        parms_.minBLOCK_TIME = std::max(
            parms_.minBLOCK_TIME,
            app.config().loadConfig(
                SECTION_CONSENSUS, "min_block_time", parms_.minBLOCK_TIME));
        parms_.maxBLOCK_TIME = std::max(
            parms_.maxBLOCK_TIME,
            app.config().loadConfig(
                SECTION_CONSENSUS, "max_block_time", parms_.maxBLOCK_TIME));
        parms_.maxBLOCK_TIME =
            std::max(parms_.minBLOCK_TIME, parms_.maxBLOCK_TIME);

        parms_.maxTXS_IN_LEDGER = std::min(
            app.config().loadConfig(
                SECTION_CONSENSUS,
                "max_txs_per_ledger",
                parms_.maxTXS_IN_LEDGER),
            consensusParms.txPOOL_CAPACITY);

        // default: 5000ms
        // min: 2 * maxBLOCK_TIME + 3000
        parms_.consensusTIMEOUT = std::chrono::milliseconds{std::max(
            parms_.consensusTIMEOUT.count(),
            app.config().loadConfig(
                SECTION_CONSENSUS,
                "time_out",
                parms_.consensusTIMEOUT.count()))};
        if (parms_.consensusTIMEOUT.count() < 2 * parms_.maxBLOCK_TIME + 1000)
        {
            parms_.consensusTIMEOUT =
                std::chrono::milliseconds{2 * parms_.maxBLOCK_TIME + 1000};
        }

        // default: 90s
        // min : 2 * consensusTIMEOUT
        parms_.initTIME = std::chrono::seconds{std::max(
            std::chrono::duration_cast<std::chrono::seconds>(
                parms_.consensusTIMEOUT)
                    .count() *
                2,
            app.config().loadConfig(
                SECTION_CONSENSUS, "init_time", parms_.initTIME.count()))};

        parms_.omitEMPTY = app.config().loadConfig(
            SECTION_CONSENSUS, "omit_empty_block", parms_.omitEMPTY);

        parms_.extractINTERVAL = consensusParms.ledgerGRANULARITY;
    }
}

TrustChanges
HotstuffAdaptor::onConsensusReached(
    bool waitingConsensusReach,
    Ledger_t previousLedger,
    uint64_t newRound)
{
    TrustChanges const changes = Adaptor::onConsensusReached(
        waitingConsensusReach, previousLedger, newRound);

    // Try to clear state cache.
    if (app_.getLedgerMaster().getPublishedLedgerAge() >
            3 * parms_.consensusTIMEOUT &&
        app_.getTxPool().isEmpty())
    {
        app_.getStateManager().clear();
    }

    return changes;
}

inline HotstuffAdaptor::Author
HotstuffAdaptor::GetValidProposer(Round round) const
{
    return app_.validators().getLeaderPubKey(round);
}

std::shared_ptr<SHAMap>
HotstuffAdaptor::onExtractTransactions(
    RCLCxLedger const& prevLedger,
    ConsensusMode mode)
{
    const bool wrongLCL = mode == ConsensusMode::wrongLedger;
    const bool proposing = mode == ConsensusMode::proposing;

    notify(app_, protocol::neCLOSING_LEDGER, prevLedger, !wrongLCL, j_);

    // Tell the ledger master not to acquire the ledger we're probably building
    ledgerMaster_.setBuildingLedger(prevLedger.seq() + 1);

    H256Set txs;
    if (isPoolAvailable())
    {
        topTransactions(parms_.maxTXS_IN_LEDGER, prevLedger.seq() + 1, txs);
    }
    else
    {
        JLOG(j_.warn()) << "onExtractTransactions: tx pool is not available";
    }

    auto initialSet =
        std::make_shared<SHAMap>(SHAMapType::TRANSACTION, app_.getNodeFamily());
    initialSet->setUnbacked();

    // Build SHAMap containing all transactions in our open ledger
    for (auto const& txID : txs)
    {
        auto tx = app_.getMasterTransaction().fetch(txID);
        if (!tx)
        {
            JLOG(j_.error())
                << "fetch transaction " + to_string(txID) + "failed";
            continue;
        }

        JLOG(j_.trace()) << "Adding open ledger TX " << txID;
        Serializer s(2048);
        tx->getSTransaction()->add(s);
        initialSet->addItem(SHAMapItem(tx->getID(), std::move(s)), true, false);
    }

    // Add pseudo-transactions to the set
    if ((app_.config().standalone() || (proposing && !wrongLCL)) &&
        ((prevLedger.seq() % 256) == 0))
    {
        // previous ledger was flag ledger, add pseudo-transactions
        auto const validations =
            app_.getValidations().getTrustedForLedger(prevLedger.parentID());

        if (validations.size() >= app_.validators().quorum())
        {
            feeVote_->doVoting(prevLedger.ledger_, validations, initialSet);
            app_.getAmendmentTable().doVoting(
                prevLedger.ledger_, validations, initialSet);
        }
    }

    // Now we need an immutable snapshot
    return std::move(initialSet->snapShot(false));
}

void
HotstuffAdaptor::broadcast(STProposal const& proposal)
{
    Blob p = proposal.getSerialized();

    protocol::TMConsensus consensus;

    consensus.set_msg(&p[0], p.size());
    consensus.set_msgtype(ConsensusMessageType::mtPROPOSAL);
    consensus.set_schemaid(app_.schemaId().begin(), uint256::size());

    JLOG(j_.info()) << "broadcast PROPOSAL";

    signAndSendMessage(consensus);
}

void
HotstuffAdaptor::broadcast(STVote const& vote)
{
    Blob v = vote.getSerialized();

    protocol::TMConsensus consensus;

    consensus.set_msg(&v[0], v.size());
    consensus.set_msgtype(ConsensusMessageType::mtVOTE);
    consensus.set_schemaid(app_.schemaId().begin(), uint256::size());

    JLOG(j_.info()) << "broadcast VOTE";

    signAndSendMessage(consensus);
}

void
HotstuffAdaptor::sendVote(PublicKey const& pubKey, STVote const& vote)
{
    Blob v = vote.getSerialized();

    protocol::TMConsensus consensus;

    consensus.set_msg(&v[0], v.size());
    consensus.set_msgtype(ConsensusMessageType::mtVOTE);
    consensus.set_schemaid(app_.schemaId().begin(), uint256::size());

    if (auto peer = app_.peerManager().findPeerByValPublicKey(pubKey))
    {
        JLOG(j_.info()) << "send VOTE to leader (Publickey index)"
                        << getPubIndex(pubKey) << " "
                        << peer->getRemoteAddress();
        signAndSendMessage(peer, consensus);
    }
    else
    {
        JLOG(j_.warn()) << "send VOTE to leader (Publickey index)"
                        << getPubIndex(pubKey) << ":"
                        << toBase58(TokenType::NodePublic, pubKey) << " closed";
    }
}

void
HotstuffAdaptor::broadcast(STEpochChange const& epochChange)
{
    Blob e = epochChange.getSerialized();

    protocol::TMConsensus consensus;

    consensus.set_msg(&e[0], e.size());
    consensus.set_msgtype(ConsensusMessageType::mtEPOCHCHANGE);
    consensus.set_schemaid(app_.schemaId().begin(), uint256::size());

    JLOG(j_.info()) << "broadcast EpochChange";

    signAndSendMessage(consensus);
}

void
HotstuffAdaptor::acquireBlock(PublicKey const& pubKey, uint256 const& hash)
{
    protocol::TMConsensus consensus;

    consensus.set_msg(hash.data(), hash.bytes);
    consensus.set_msgtype(ConsensusMessageType::mtACQUIREBLOCK);
    consensus.set_schemaid(app_.schemaId().begin(), uint256::size());

    if (auto peer = app_.peerManager().findPeerByValPublicKey(pubKey))
    {
        JLOG(j_.info()) << "acquiring Executedblock " << hash << " from peer "
                        << getPubIndex(pubKey) << " "
                        << peer->getRemoteAddress();
        signAndSendMessage(peer, consensus);
    }
    else
    {
        JLOG(j_.warn()) << "acquiring Executedblock " << hash << "from peer "
                        << getPubIndex(pubKey) << ":"
                        << toBase58(TokenType::NodePublic, pubKey) << " closed";
    }
}

void
HotstuffAdaptor::sendBLock(
    std::shared_ptr<PeerImp> peer,
    hotstuff::ExecutedBlock const& block)
{
    protocol::TMConsensus consensus;

    Buffer b(std::move(serialization::serialize(block)));

    consensus.set_msg(b.data(), b.size());
    consensus.set_msgtype(ConsensusMessageType::mtBLOCKDATA);
    consensus.set_schemaid(app_.schemaId().begin(), uint256::size());

    JLOG(j_.info()) << "send ExecutedBlock " << block.block.id() << " to peer "
                    << getPubIndex(peer->getValPublic()) << " "
                    << peer->getRemoteAddress();

    signMessage(consensus);

    auto const m = std::make_shared<Message>(consensus, protocol::mtCONSENSUS);

    peer->send(m);
}

bool
HotstuffAdaptor::doAccept(typename Ledger_t::ID const& lgrId)
{
    auto ledger = ledgerMaster_.getLedgerByHash(lgrId);
    if (!ledger)
    {
        return false;
    }

    ledgerMaster_.updateConsensusTime();

    // next ledger is flag ledger
    if (ledger->isVotingLedger())
    {
        validate(ledger);
    }

    if (ledgerMaster_.getCurrentLedger()->seq() < ledger->seq() + 1)
    {
        {
            // Build new open ledger
            std::unique_lock lock{app_.getMasterMutex(), std::defer_lock};
            std::unique_lock sl{ledgerMaster_.peekMutex(), std::defer_lock};
            std::lock(lock, sl);

            auto const lastVal = ledgerMaster_.getValidatedLedger();
            boost::optional<Rules> rules;
            if (lastVal)
                rules.emplace(*lastVal, app_.config().features);
            else
                rules.emplace(app_.config().features);

            CanonicalTXSet retriableTxs{beast::zero};
            app_.openLedger().accept(
                app_,
                *rules,
                ledger,
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

        // Tell directly connected peers that we have a new LCL
        notify(
            app_,
            protocol::neACCEPTED_LEDGER,
            ledger,
            mode_ != ConsensusMode::wrongLedger,
            j_);

        ledgerMaster_.switchLCL(ledger);

        updatePoolAvoid(ledger->txMap(), ledger->seq());

        app_.getOPs().endConsensus();
    }

    return true;
}

void
HotstuffAdaptor::peerValidation(
    std::shared_ptr<PeerImp>& peer,
    STValidation::ref val)
{
    try
    {
        if (!isCurrent(
                app_.getValidations().parms(),
                app_.timeKeeper().closeTime(),
                val->getSignTime(),
                val->getSeenTime()))
        {
            JLOG(j_.info()) << "Validation: Not current";
            peer->charge(Resource::feeUnwantedData);
            return;
        }

        JLOG(j_.info()) << "recvValidation " << val->getLedgerHash() << " from "
                        << peer->id();

        app_.getOPs().pubValidation(val);

        handleNewValidation(val, std::to_string(peer->id()));
    }
    catch (std::exception const& e)
    {
        JLOG(j_.warn()) << "Validation: Exception, " << e.what();
        peer->charge(Resource::feeInvalidRequest);
    }

    return;
}

// -------------------------------------------------------------------
// Private member functions

void
HotstuffAdaptor::validate(std::shared_ptr<Ledger const> ledger)
{
    using namespace std::chrono_literals;
    auto validationTime = app_.timeKeeper().closeTime();
    if (validationTime <= lastValidationTime_)
        validationTime = lastValidationTime_ + 1s;
    lastValidationTime_ = validationTime;

    auto v = std::make_shared<STValidation>(
        lastValidationTime_, valPublic_, nodeID_, [&](STValidation& v) {
            v.setFieldH256(sfLedgerHash, ledger->info().hash);
            v.setFieldH256(sfConsensusHash, ledger->info().txHash);

            v.setFieldU32(sfLedgerSequence, ledger->seq());

            if (mode_ == ConsensusMode::proposing ||
                mode_ == ConsensusMode::switchedLedger)
                v.setFlag(vfFullValidation);

            // Report our load
            {
                auto const& ft = app_.getFeeTrack();
                auto const fee = std::max(ft.getLocalFee(), ft.getClusterFee());
                if (fee > ft.getLoadBase())
                    v.setFieldU32(sfLoadFee, fee);
            }

            // If the next ledger is a flag ledger, suggest fee changes and
            // new features:
            // Fees:
            feeVote_->doValidation(ledger->fees(), v);

            // Amendments
            // FIXME: pass `v` and have the function insert the array
            // directly?
            auto const amendments = app_.getAmendmentTable().doValidation(
                getEnabledAmendments(*ledger));

            if (!amendments.empty())
                v.setFieldV256(
                    sfAmendments, STVector256(sfAmendments, amendments));
        });

    handleNewValidation(v, "local");

    Blob validation = v->getSerialized();

    protocol::TMConsensus consensus;

    consensus.set_msg(&validation[0], validation.size());
    consensus.set_msgtype(ConsensusMessageType::mtVALIDATION);
    consensus.set_schemaid(app_.schemaId().begin(), uint256::size());
    consensus.set_signflags(vfFullyCanonicalSig);

    signAndSendMessage(consensus);

    app_.getOPs().pubValidation(v);
}

void
HotstuffAdaptor::handleNewValidation(
    STValidation::ref val,
    std::string const& source)
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

    RCLValidations& validations = app_.getValidations();
    beast::Journal j = validations.adaptor().journal();

    auto dmp = [&](beast::Journal::Stream s, std::string const& msg) {
        std::string id = toBase58(TokenType::NodePublic, signingKey);

        if (masterKey)
            id += ":" + toBase58(TokenType::NodePublic, *masterKey);

        s << (val->isTrusted() ? "trusted" : "untrusted") << " "
          << (val->isFull() ? "full" : "partial") << " validation: " << hash
          << " from " << id << " via " << source << ": " << msg << "\n"
          << " [" << val->getSerializer().slice() << "]";
    };

    if (!val->isFieldPresent(sfLedgerSequence))
    {
        if (j.error())
            dmp(j.error(), "missing ledger sequence field");
        return;
    }

    // masterKey is seated only if validator is trusted or listed
    if (masterKey)
    {
        ValStatus const outcome = validations.add(calcNodeID(*masterKey), val);
        auto const seq = val->getFieldU32(sfLedgerSequence);

        if (j.debug())
            dmp(j.debug(), to_string(outcome));

        // One might think that we would not wish to relay validations that
        // fail these checks. Somewhat counterintuitively, we actually want
        // to do it for validations that we receive but deem suspicious, so
        // that our peers will also observe them and realize they're bad.
        if (outcome == ValStatus::conflicting && j.warn())
        {
            dmp(j.warn(),
                "conflicting validations issued for " + to_string(seq) +
                    " (likely from a Byzantine validator)");
        }

        if (outcome == ValStatus::multiple && j.warn())
        {
            dmp(j.warn(),
                "multiple validations issued for " + to_string(seq) +
                    " (multiple validators operating with the same key?)");
        }

        if (outcome == ValStatus::badSeq && j.warn())
        {
            auto const seq = val->getFieldU32(sfLedgerSequence);
            dmp(j.warn(),
                "already validated sequence at or past " + to_string(seq));
        }
    }
    else
    {
        JLOG(j.debug()) << "Val for " << hash << " from "
                        << toBase58(TokenType::NodePublic, signingKey)
                        << " not added UNlisted";
    }
}

}  // namespace ripple