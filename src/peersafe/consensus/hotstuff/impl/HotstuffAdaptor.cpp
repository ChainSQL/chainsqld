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
#include <ripple/app/ledger/TransactionMaster.h>
#include <ripple/app/ledger/LocalTxs.h>
#include <ripple/app/misc/LoadFeeTrack.h>
#include <ripple/app/misc/AmendmentTable.h>
#include <peersafe/consensus/ConsensusBase.h>
#include <peersafe/consensus/hotstuff/HotstuffAdaptor.h>
#include <peersafe/serialization/hotstuff/ExecutedBlock.h>


namespace ripple {


HotstuffAdaptor::HotstuffAdaptor(
    Application& app,
    std::unique_ptr<FeeVote>&& feeVote,
    LedgerMaster& ledgerMaster,
    InboundTransactions& inboundTransactions,
    ValidatorKeys const & validatorKeys,
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
    if (app_.config().exists(SECTION_HCONSENSUS))
    {
        parms_.minBLOCK_TIME = app.config().loadConfig(SECTION_HCONSENSUS, "min_block_time", parms_.minBLOCK_TIME);
        parms_.maxBLOCK_TIME = app.config().loadConfig(SECTION_HCONSENSUS, "max_block_time", parms_.maxBLOCK_TIME);
        parms_.maxBLOCK_TIME = std::max(parms_.minBLOCK_TIME, parms_.maxBLOCK_TIME);

        parms_.maxTXS_IN_LEDGER = std::min(
            app.config().loadConfig(SECTION_HCONSENSUS, "max_txs_per_ledger", parms_.maxTXS_IN_LEDGER),
            consensusParms.txPOOL_CAPACITY);

        parms_.omitEMPTY = app.config().loadConfig(SECTION_HCONSENSUS, "omit_empty_block", parms_.omitEMPTY);

        // default: 6s
        // min: 6s
        parms_.consensusTIMEOUT = std::chrono::seconds{
            std::max(
                (int)parms_.consensusTIMEOUT.count(),
                app.config().loadConfig(SECTION_HCONSENSUS, "time_out", 0)) };

        // default: 90s
        // min : 2 * consensusTIMEOUT
        parms_.initTIME = std::chrono::seconds{
            std::max(
                parms_.consensusTIMEOUT.count() * 2,
                app.config().loadConfig(SECTION_HCONSENSUS, "init_time", parms_.initTIME.count())) };
    }
}

inline HotstuffAdaptor::Author HotstuffAdaptor::GetValidProposer(Round round) const
{
    return app_.validators().getLeaderPubKey(round);
}

std::shared_ptr<SHAMap> HotstuffAdaptor::onExtractTransactions(RCLCxLedger const& prevLedger, ConsensusMode mode)
{
    const bool wrongLCL = mode == ConsensusMode::wrongLedger;
    const bool proposing = mode == ConsensusMode::proposing;

    //notify(protocol::neCLOSING_LEDGER, prevLedger, !wrongLCL);

    // Tell the ledger master not to acquire the ledger we're probably building
    ledgerMaster_.setBuildingLedger(prevLedger.seq() + 1);

    H256Set txs;
    topTransactions(parms_.maxTXS_IN_LEDGER, prevLedger.seq() + 1, txs);

    auto initialSet = std::make_shared<SHAMap>(
        SHAMapType::TRANSACTION, app_.family(), SHAMap::version{ 1 });
    initialSet->setUnbacked();

    // Build SHAMap containing all transactions in our open ledger
    for (auto const& txID : txs)
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
        ((prevLedger.seq() % 256) == 0))
    {
        // previous ledger was flag ledger, add pseudo-transactions
        auto const validations =
            app_.getValidations().getTrustedForLedger(
                prevLedger.parentID());

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

void HotstuffAdaptor::broadcast(STProposal const& proposal)
{
    Blob p = proposal.getSerialized();

    protocol::TMConsensus consensus;

    consensus.set_msg(&p[0], p.size());
    consensus.set_msgtype(ConsensusMessageType::mtPROPOSAL);

    JLOG(j_.info()) << "broadcast PROPOSAL";

    signAndSendMessage(consensus);
}

void HotstuffAdaptor::broadcast(STVote const& vote)
{
    Blob v = vote.getSerialized();

    protocol::TMConsensus consensus;

    consensus.set_msg(&v[0], v.size());
    consensus.set_msgtype(ConsensusMessageType::mtVOTE);

    JLOG(j_.info()) << "broadcast VOTE";

    signAndSendMessage(consensus);
}

void HotstuffAdaptor::sendVote(PublicKey const& pubKey, STVote const& vote)
{
    Blob v = vote.getSerialized();

    protocol::TMConsensus consensus;

    consensus.set_msg(&v[0], v.size());
    consensus.set_msgtype(ConsensusMessageType::mtVOTE);

    JLOG(j_.info()) << "send VOTE to leader, leader index: " << getPubIndex(pubKey);

    signAndSendMessage(pubKey, consensus);
}

void HotstuffAdaptor::broadcast(STEpochChange const& epochChange)
{
    Blob e = epochChange.getSerialized();

    protocol::TMConsensus consensus;

    consensus.set_msg(&e[0], e.size());
    consensus.set_msgtype(ConsensusMessageType::mtEPOCHCHANGE);

    JLOG(j_.info()) << "broadcast EpochChange";

    signAndSendMessage(consensus);
}

void HotstuffAdaptor::acquireBlock(PublicKey const& pubKey, uint256 const& hash)
{
    protocol::TMConsensus consensus;

    consensus.set_msg(hash.data(), hash.bytes);
    consensus.set_msgtype(ConsensusMessageType::mtACQUIREBLOCK);

    JLOG(j_.info()) << "acquiring Executedblock " << hash << " from peer " << getPubIndex(pubKey);

    signAndSendMessage(pubKey, consensus);
}

void HotstuffAdaptor::sendBLock(std::shared_ptr<PeerImp> peer, hotstuff::ExecutedBlock const& block)
{
    protocol::TMConsensus consensus;

    Buffer b(std::move(serialization::serialize(block)));

    consensus.set_msg(b.data(), b.size());
    consensus.set_msgtype(ConsensusMessageType::mtBLOCKDATA);

    JLOG(j_.info()) << "send ExecutedBlock to peer " << getPubIndex(peer->getValPublic());

    signMessage(consensus);

    auto const m = std::make_shared<Message>(
        consensus, protocol::mtCONSENSUS);

    peer->send(m);
}

bool HotstuffAdaptor::doAccept(typename Ledger_t::ID const& lgrId)
{
    auto ledger = ledgerMaster_.getLedgerByHash(lgrId);
    if (!ledger)
    {
        return false;
    }

    ledgerMaster_.updateConsensusTime();

    // next ledger is flag ledger
    if (((ledger->seq() + 1) % 256) == 0)
    {
        validate(ledger);
    }

    if (ledgerMaster_.getCurrentLedger()->seq() < ledger->seq() + 1)
    {
        {
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

            CanonicalTXSet retriableTxs{ beast::zero };
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
        notify(protocol::neACCEPTED_LEDGER, ledger, mode_ != ConsensusMode::wrongLedger);

        ledgerMaster_.switchLCL(ledger);

        updatePoolAvoid(ledger->txMap(), ledger->seq());

        app_.getOPs().endConsensus();
    }

    return true;
}

void HotstuffAdaptor::peerValidation(std::shared_ptr<PeerImp>& peer, STValidation::ref val)
{
    try
    {
        if (!isCurrent(app_.getValidations().parms(),
            app_.timeKeeper().closeTime(),
            val->getSignTime(),
            val->getSeenTime()))
        {
            JLOG(j_.info()) << "Validation: Not current";
            peer->charge(Resource::feeUnwantedData);
            return;
        }

        JLOG(j_.info()) << "recvValidation " << val->getLedgerHash()
            << " from " << peer->id();

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

void HotstuffAdaptor::validate(std::shared_ptr<Ledger const> ledger)
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

    // Suggest fee changes and new features
    feeVote_->doValidation(ledger, fees);
    amendments = app_.getAmendmentTable().doValidation(getEnabledAmendments(*ledger));

    auto v = std::make_shared<STValidation>(
        ledger->info().hash,
        ledger->seq(),
        ledger->info().txHash,
        valPublic_,
        validationTime,
        nodeID_,
        mode_ == ConsensusMode::proposing || mode_ == ConsensusMode::switchedLedger, /* full if proposed */
        fees,
        amendments);

    handleNewValidation(v, "local");

    Blob validation = v->getSerialized();

    protocol::TMConsensus consensus;

    consensus.set_msg(&validation[0], validation.size());
    consensus.set_msgtype(ConsensusMessageType::mtVALIDATION);
    consensus.set_signflags(vfFullyCanonicalSig);

    signAndSendMessage(consensus);

    app_.getOPs().pubValidation(v);
}

void HotstuffAdaptor::handleNewValidation(STValidation::ref val, std::string const& source)
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
        return;
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
    }
    else
    {
        JLOG(j.debug()) << "Val for " << hash << " from "
            << toBase58(TokenType::NodePublic, signingKey)
            << " not added UNlisted";
    }
}


}