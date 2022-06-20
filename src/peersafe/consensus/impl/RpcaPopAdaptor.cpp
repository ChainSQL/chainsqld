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
#include <ripple/app/misc/HashRouter.h>
#include <ripple/app/misc/LoadFeeTrack.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/app/misc/Transaction.h>
#include <ripple/app/misc/ValidatorKeys.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/digest.h>
#include <ripple/protocol/STValidationSet.h>
#include <peersafe/schema/PeerManager.h>
#include <peersafe/app/misc/TxPool.h>
#include <peersafe/app/util/Common.h>
#include <peersafe/consensus/ConsensusBase.h>
#include <peersafe/consensus/RpcaPopAdaptor.h>
#include <peersafe/protocol/STViewChange.h>

namespace ripple {

RpcaPopAdaptor::RpcaPopAdaptor(
    Schema& app,
    std::unique_ptr<FeeVote>&& feeVote,
    LedgerMaster& ledgerMaster,
    InboundTransactions& inboundTransactions,
    ValidatorKeys const& validatorKeys,
    beast::Journal journal,
    LocalTxs& localTxs)
    : Adaptor(
          app,
          std::move(feeVote),
          ledgerMaster,
          inboundTransactions,
          validatorKeys,
          journal,
          localTxs)
{
}

void
RpcaPopAdaptor::onAccept(
    Result const& result,
    RCLCxLedger const& prevLedger,
    NetClock::duration const& closeResolution,
    ConsensusCloseTimes const& rawCloseTimes,
    ConsensusMode const& mode,
    Json::Value&& consensusJson)
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
            JLOG(j_.info())
                << "doAccept time used:" << utcTime() - timeStart << "ms";
            this->app_.getOPs().endConsensus();
        }, app_.doJobCounter());
}

std::shared_ptr<Ledger const>
RpcaPopAdaptor::checkLedgerAccept(LedgerInfo const& info)
{
    auto ledger = Adaptor::checkLedgerAccept(info);
    if (!ledger)
    {
        return nullptr;
    }

    std::lock_guard ml(ledgerMaster_.peekMutex());

    auto const minVal = getNeededValidations();
    auto validations = app_.validators().negativeUNLFilter(
        app_.getValidations().getTrustedForLedger(ledger->info().hash));
    auto const tvc = validations.size();
    if (tvc < minVal)  // nothing we can do
    {
        JLOG(j_.trace()) << "Only " << tvc << " validations for " << info.hash;
        return nullptr;
    }

    JLOG(j_.info()) << "Advancing accepted ledger to " << info.seq
                    << " with >= " << minVal << " validations";

    return ledger;
}

void
RpcaPopAdaptor::propose(RCLCxPeerPos::Proposal const& proposal)
{
    JLOG(j_.trace()) << "We propose: "
                     << (proposal.isBowOut()
                             ? std::string("bowOut")
                             : ripple::to_string(proposal.position()));

    Blob p = proposal.getSerialized();

    protocol::TMConsensus consensus;
    consensus.set_msg(&p[0], p.size());
    consensus.set_msgtype(ConsensusMessageType::mtPROPOSESET);
    consensus.set_schemaid(app_.schemaId().begin(), uint256::size());

    signAndSendMessage(consensus);
}

void
RpcaPopAdaptor::validate(
    RCLCxLedger const& ledger,
    RCLTxSet const& txns,
    bool proposing)
{
    using namespace std::chrono_literals;
    auto validationTime = app_.timeKeeper().closeTime();
    if (validationTime <= lastValidationTime_)
        validationTime = lastValidationTime_ + 1s;
    lastValidationTime_ = validationTime;

    auto v = std::make_shared<STValidation>(
        lastValidationTime_,
        valPublic_,
        nodeID_,
        [&](STValidation& v) {
            v.setFieldVL(sfSigningPubKey, valPublic_);
            v.setFieldH256(sfLedgerHash, ledger.id());
            v.setFieldH256(sfConsensusHash, txns.id());

            v.setFieldU32(sfLedgerSequence, ledger.seq());

            if (proposing)
                v.setFlag(vfFullValidation);

            if (ledger.ledger_->rules().enabled(featureHardenedValidations))
            {
                // Attest to the hash of what we consider to be the last fully
                // validated ledger. This may be the hash of the ledger we are
                // validating here, and that's fine.
                if (auto const vl = ledgerMaster_.getValidatedLedger())
                    v.setFieldH256(sfValidatedHash, vl->info().hash);

                v.setFieldU64(sfCookie, valCookie_);

                // Report our server version every flag ledger:
                if (ledger.ledger_->isVotingLedger())
                    v.setFieldU64(
                        sfServerVersion, BuildInfo::getEncodedVersion());
            }

            // Report our load
            {
                auto const& ft = app_.getFeeTrack();
                auto const fee = std::max(ft.getLocalFee(), ft.getClusterFee());
                if (fee > ft.getLoadBase())
                    v.setFieldU32(sfLoadFee, fee);
            }

            // If the next ledger is a flag ledger, suggest fee changes and
            // new features:
            if (ledger.ledger_->isVotingLedger())
            {
                // Fees:
                feeVote_->doValidation(ledger.ledger_->fees(), v);

                // Amendments
                // FIXME: pass `v` and have the function insert the array
                // directly?
                auto const amendments = app_.getAmendmentTable().doValidation(
                    getEnabledAmendments(*ledger.ledger_));

                if (!amendments.empty())
                    v.setFieldV256(
                        sfAmendments, STVector256(sfAmendments, amendments));
            }
        });

    v->sign(valSecret_);
    
    handleNewValidation(v, "local");

    Blob validation = v->getSerialized();

    protocol::TMConsensus consensus;

    consensus.set_msg(&validation[0], validation.size());
    consensus.set_msgtype(ConsensusMessageType::mtVALIDATION);
    consensus.set_signflags(vfFullyCanonicalSig);
    consensus.set_schemaid(app_.schemaId().begin(), uint256::size());

    signAndSendMessage(consensus);

    app_.getOPs().pubValidation(v);
}

bool
RpcaPopAdaptor::peerValidation(
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
            JLOG(j_.warn()) << "Validation: Not current,now="
                << to_string(app_.timeKeeper().closeTime())
                << ",val.signTime=" << to_string(val->getSignTime())<< ",val.seenTime="
                <<to_string(val->getSeenTime());
            peer->charge(Resource::feeUnwantedData);
            return false;
        }

        JLOG(j_.info()) << "recvValidation " << val->getLedgerHash() << " from "
                        << getPubIndex(val->getSignerPublic());

        app_.getOPs().pubValidation(val);

        if (handleNewValidation(val, std::to_string(peer->id())) ||
            peer->cluster())
        {
            return true;
        }
    }
    catch (std::exception const& e)
    {
        JLOG(j_.warn()) << "Validation: Exception, " << e.what();
        peer->charge(Resource::feeInvalidRequest);
    }

    return false;
}

std::size_t
RpcaPopAdaptor::proposersFinished(
    RCLCxLedger const& ledger,
    LedgerHash const& h) const
{
    RCLValidations& vals = app_.getValidations();
    return vals.getNodesAfter(
        RCLValidatedLedger(ledger.ledger_, vals.adaptor().journal()), h);
}

uint256
RpcaPopAdaptor::getPrevLedger(
    uint256 ledgerID,
    RCLCxLedger const& ledger,
    ConsensusMode mode)
{
    RCLValidations& vals = app_.getValidations();
    uint256 netLgr = vals.getPreferred(
        RCLValidatedLedger{ledger.ledger_, vals.adaptor().journal()},
        getValidLedgerIndex());

    if (netLgr == ledger.id())
    {
        netLgr = ledgerID;
    }

    if (netLgr != ledgerID)
    {
        if (mode != ConsensusMode::wrongLedger)
            app_.getOPs().consensusViewChange();

        JLOG(j_.debug()) << Json::Compact(app_.getValidations().getJsonTrie());
    }

    return netLgr;
}

// ------------------------------------------------------------------------
// Protected member functions

void
RpcaPopAdaptor::consensusBuilt(
    std::shared_ptr<Ledger const> const& ledger,
    uint256 const& consensusHash,
    Json::Value consensus)
{
    // Because we just built a ledger, we are no longer building one
    ledgerMaster_.setBuildingLedger(0);

    // No need to process validations in standalone mode
    if (app_.config().standalone())
        return;

    ledgerMaster_.getLedgerHistory().builtLedger(
        ledger, consensusHash, std::move(consensus));

    if (ledger->info().seq <= getValidLedgerIndex())
    {
        auto stream = app_.journal("LedgerConsensus").info();
        JLOG(stream) << "Consensus built old ledger: " << ledger->info().seq
                     << " <= " << getValidLedgerIndex();
        return;
    }

    // See if this ledger can be the new fully-validated ledger
    if (checkLedgerAccept(ledger->info()))
    {
        doValidLedger(ledger);
    }

    if (ledger->info().seq <= getValidLedgerIndex())
    {
        auto stream = app_.journal("LedgerConsensus").debug();
        JLOG(stream) << "Consensus ledger fully validated";
        return;
    }

    // This ledger cannot be the new fully-validated ledger, but
    // maybe we saved up validations for some other ledger that can be

    auto validations = app_.validators().negativeUNLFilter(
        app_.getValidations().currentTrusted());

    // Track validation counts with sequence numbers
    class valSeq
    {
    public:
        valSeq() : valCount_(0), ledgerSeq_(0)
        {
            ;
        }

        void
        mergeValidation(LedgerIndex seq)
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
    hash_map<uint256, valSeq> count;
    for (auto const& v : validations)
    {
        valSeq& vs = count[v->getLedgerHash()];
        vs.mergeValidation(v->getFieldU32(sfLedgerSequence));
    }

    auto const neededValidations = getNeededValidations();
    auto maxSeq = getValidLedgerIndex();
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
            doValidLedger(result.first);
        }
    }
}

// ------------------------------------------------------------------------
// Private member functions

bool
RpcaPopAdaptor::handleNewValidation(
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

    bool shouldRelay = false;
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
        return false;
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

        if (val->isTrusted() && outcome == ValStatus::current)
        {
            auto result =
                checkLedgerAccept(hash, val->getFieldU32(sfLedgerSequence));

            if (result.first && result.second)
            {
                doValidLedger(result.first);
            }
            shouldRelay = true;
        }
    }
    else
    {
        JLOG(j.info()) << "Val for " << hash << " from "
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

auto
RpcaPopAdaptor::checkLedgerAccept(uint256 const& hash, std::uint32_t seq)
    -> std::pair<std::shared_ptr<Ledger const> const, bool>
{
    std::size_t valCount = 0;

    if (seq != 0)
    {
        // Ledger is too old
        if (seq < getValidLedgerIndex())
            return {nullptr, false};

        auto validations = app_.validators().negativeUNLFilter(
            app_.getValidations().getTrustedForLedger(hash));
        valCount = validations.size();
        if (valCount >= app_.validators().quorum())
        {
            ledgerMaster_.setLastValidLedger(hash, seq);
        }

        if (seq == getValidLedgerIndex())
            return {nullptr, false};

        // Ledger could match the ledger we're already building
        if (seq == ledgerMaster_.getBuildingLedger())
            return {nullptr, false};
    }

    auto ledger = ledgerMaster_.getLedgerHistory().getLedgerByHash(hash);

    if (!ledger)
    {
        if ((seq != 0) && (getValidLedgerIndex() == 0))
        {
            // Set peers sane early if we can
            if (valCount >= app_.validators().quorum())
                app_.peerManager().checkSanity(seq);
        }

        // FIXME: We may not want to fetch a ledger with just one
        // trusted validation
        ledger = app_.getInboundLedgers().acquire(
            hash, seq, InboundLedger::Reason::GENERIC);
    }

    if (ledger)
        return {ledger, !!checkLedgerAccept(ledger->info())};

    return {nullptr, false};
}

bool
RpcaPopAdaptor::peerAcquirValidationSet(std::uint32_t validatedSeq, std::shared_ptr<PeerImp>& peer)
{
    JLOG(j_.warn()) << "Processing peer AcquirValidation ValidatedSeq=" << validatedSeq;

    auto ledger = getValidatedLedger();
    if (ledger && ledger->info().seq == validatedSeq)
    {
        auto validations = getLastValidations(ledger->info().seq, ledger->info().hash);
        if (validations.size() > 0 )
            sendAcquirValidationSet(std::make_shared<STValidationSet>(ledger->info().seq,ledger->info().hash, valPublic_, validations), peer);
    }
    
    return false;
}

bool
RpcaPopAdaptor::sendAcquirValidationSet(std::shared_ptr<STValidationSet> const& validationSet, std::shared_ptr<PeerImp>& peer)
{
    Blob valSet =  validationSet->getSerialized();

    protocol::TMConsensus consensus;

    consensus.set_msg(&valSet[0], valSet.size());
    consensus.set_msgtype(ConsensusMessageType::mtVALIDATIONSETDATA);
    consensus.set_signflags(vfFullyCanonicalSig);
    consensus.set_schemaid(app_.schemaId().begin(), uint256::size());

    signAndSendMessage(peer, consensus);
    return true;
}

bool
RpcaPopAdaptor::peerValidationSetData(
    STValidationSet::ref vaildationSet)
{
    JLOG(j_.warn()) << "Processing peer ValidationData seq="
                    << vaildationSet->getFieldU32(sfSequence);

    auto ledger = getValidatedLedger();
    auto seq = vaildationSet->getFieldU32(sfSequence);
    auto hash = vaildationSet->getFieldH256(sfLedgerHash);
    if (ledger && ledger->info().seq < seq)
    {
        auto validations = std::vector<std::shared_ptr<STValidation>>();
        auto valSet = vaildationSet->getValSet();
        for (auto const& item : valSet)
        {
            if (seq != item->getFieldU32(sfSequence) || hash != item->getLedgerHash())
                return false;
            if (item->verify())
            {
                validations.push_back(item);
            }
        }
        if (validations.size() > app_.validators().quorum())
        {
            app_.getValidations().setLastValidations(validations);
            auto result = checkLedgerAccept(hash, seq);
            if (result.first && result.second)
            {
                doValidLedger(result.first);
            }
        }
    }
    
    return false;
}
std::vector<std::shared_ptr<STValidation>>
RpcaPopAdaptor::getLastValidations(std::uint32_t seq, uint256 id)
{
    auto valSet = app_.getValidations().getLastValidationsFromCache(seq, id);
    if (valSet.size() == 0)
        return getLastValidationsFromDB(seq, id);
    return valSet;
}
std::vector<std::shared_ptr<STValidation>>
RpcaPopAdaptor::getLastValidationsFromDB(std::uint32_t seq, uint256 id)
{
    auto valSet = std::vector<std::shared_ptr<STValidation>>();
    auto db = app_.getLedgerDB().checkoutDb();
        boost::optional<std::string> sNodePubKey;
    Blob RawData;

    soci::blob sRawData(*db);
    soci::indicator rawDataPresent;

    std::ostringstream sqlSuffix;
    sqlSuffix << "WHERE LedgerSeq = " << seq
                << " AND LedgerHash = '" << id << "'";
    std::string const sql =
        "SELECT NodePubKey, RawData "
        "FROM LastValidations " +
        sqlSuffix.str() + ";";

    soci::statement st =
        (db->prepare << sql,
            soci::into(sNodePubKey),
            soci::into(sRawData, rawDataPresent));

    st.execute();
    while (st.fetch())
    {
        if (sNodePubKey && rawDataPresent == soci::i_ok)
        {
            convert(sRawData, RawData);
            SerialIter sit(makeSlice(RawData));
            auto const publicKey =
                parseBase58<PublicKey>(TokenType::NodePublic, *sNodePubKey);
            if (!publicKey)
            {
                continue;
            }
            STValidation::pointer val = std::make_shared<STValidation>(
                std::ref(sit), *publicKey, [&](PublicKey const& pk) {
                    return calcNodeID(app_.validatorManifests().getMasterKey(pk));
                });
            valSet.push_back(val);
        }
    }

    return valSet;
}
}  // namespace ripple