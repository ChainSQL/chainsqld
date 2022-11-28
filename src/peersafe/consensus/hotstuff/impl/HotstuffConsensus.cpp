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

#include <chrono>
#include <peersafe/app/util/Common.h>
#include <peersafe/consensus/hotstuff/HotstuffConsensus.h>
#include <peersafe/consensus/hotstuff/impl/Config.h>
#include <peersafe/consensus/hotstuff/impl/RecoverData.h>
#include <peersafe/serialization/hotstuff/ExecutedBlock.h>

namespace ripple {

HotstuffConsensus::HotstuffConsensus(
    Adaptor& adaptor,
    clock_type const& clock,
    beast::Journal j)
    : ConsensusBase(clock, j)
    , adaptor_(*(HotstuffAdaptor*)(&adaptor))
    , blockAcquiring_(
          "blockAcquiring",
          256,
          adaptor_.parms().consensusTIMEOUT,
          const_cast<clock_type&>(clock),
          j)
{
    JLOG(j_.info()) << "Creating HOTSTUFF consensus object";

    hotstuff::Config config;

    config.id = adaptor_.valPublic();

    // TODO
    // HotstuffConsensusParms to hotstuff::Config
    config.timeout =
        std::ceil(adaptor_.parms().consensusTIMEOUT.count() / 1000.0);
    config.interval_extract = adaptor_.parms().extractINTERVAL.count();

    hotstuff_ = hotstuff::Hotstuff::Builder(adaptor_.getIOService(), j)
                    .setConfig(config)
                    .setCommandManager(this)
                    .setStateCompute(this)
                    .setNetWork(this)
                    .setProposerElection(&adaptor_)
                    .build();

    // Unused now
    using beast::hash_append;
    ripple::sha512_half_hasher h;
    hash_append(h, std::string("EPOCHCHANGE"));
    epochChangeHash_ = static_cast<typename sha512_half_hasher::result_type>(h);
}

HotstuffConsensus::~HotstuffConsensus()
{
    if (hotstuff_)
    {
        hotstuff_->stop();
    }
}

void
HotstuffConsensus::startRound(
    NetClock::time_point const& now,
    typename Ledger_t::ID const& prevLedgerID,
    Ledger_t prevLedger,
    hash_set<NodeID> const& nowUntrusted,
    bool proposing)
{
    ConsensusMode startMode =
        proposing ? ConsensusMode::proposing : ConsensusMode::observing;

    // We were handed the wrong ledger
    if (prevLedger.id() != prevLedgerID)
    {
        // try to acquire the correct one
        if (auto newLedger = adaptor_.acquireLedger(prevLedgerID))
        {
            prevLedger = *newLedger;
        }
        else  // Unable to acquire the correct ledger
        {
            startMode = ConsensusMode::wrongLedger;
            JLOG(j_.info())
                << "Entering consensus with: " << previousLedger_.id();
            JLOG(j_.info()) << "Correct LCL is: " << prevLedgerID;
        }
    }

    startRoundInternal(now, prevLedgerID, prevLedger, startMode, !startup_);
}

void
HotstuffConsensus::timerEntry(NetClock::time_point const& now)
{
    if (!startup_)
        return;

    now_ = now;

    if (now - sweepTime_ >= sweepInterval_)
    {
        blockAcquiring_.sweep();
        sweepTime_ = now;
    }

    if (mode_.get() == ConsensusMode::wrongLedger)
    {
        if (auto newLedger = adaptor_.acquireLedger(prevLedgerID_))
        {
            JLOG(j_.info()) << "Have the consensus ledger " << newLedger->seq()
                            << ":" << prevLedgerID_;
            adaptor_.removePoolTxs(
                newLedger->ledger_->txMap(),
                newLedger->ledger_->info().seq,
                newLedger->ledger_->info().parentHash);

            startRoundInternal(
                now_,
                prevLedgerID_,
                *newLedger,
                ConsensusMode::switchedLedger,
                waitingForInit());
        }
        return;
    }

    if (waitingForInit() && adaptor_.validating())
    {
        consensusTime_ = now_;

        if (mode_.get() != ConsensusMode::wrongLedger)
        {
            initAnnounce();
        }

        return;
    }

    if ((now_ - consensusTime_) > (adaptor_.parms().consensusTIMEOUT *
                                   adaptor_.parms().timeoutCOUNT_ROLLBACK))
    {
        JLOG(j_.error()) << "Consensus network partitioned, do recover";
        startRoundInternal(
            now_,
            prevLedgerID_,
            previousLedger_,
            ConsensusMode::switchedLedger,
            true);
    }
}

bool
HotstuffConsensus::peerConsensusMessage(
    std::shared_ptr<PeerImp>& peer,
    bool isTrusted,
    std::shared_ptr<protocol::TMConsensus> const& m)
{
    if (startup_)
    {
        JLOG(j_.info()) << "Processing peerConsensusMessage mt(" << m->msgtype()
                        << ")";

        switch (m->msgtype())
        {
            case ConsensusMessageType::mtPROPOSAL:
                peerProposal(peer, isTrusted, m);
                break;
            case ConsensusMessageType::mtVOTE:
                peerVote(peer, isTrusted, m);
                break;
            case ConsensusMessageType::mtACQUIREBLOCK:
                peerAcquireBlock(peer, isTrusted, m);
                break;
            case ConsensusMessageType::mtBLOCKDATA:
                peerBlockData(peer, isTrusted, m);
                break;
            case ConsensusMessageType::mtEPOCHCHANGE:
                peerEpochChange(peer, isTrusted, m);
                break;
            case ConsensusMessageType::mtINITANNOUNCE:
                peerInitAnnounce(peer, isTrusted, m);
                break;
            case ConsensusMessageType::mtVALIDATION:
                peerValidation(peer, isTrusted, m);
                break;
            default:
                break;
        }
    }

    return false;
}

void
HotstuffConsensus::gotTxSet(
    NetClock::time_point const& now,
    TxSet_t const& txSet)
{
    (void)now;

    auto id = txSet.id();

    JLOG(j_.info()) << "gotTxSet " << id;

    if (acquired_.find(id) == acquired_.end())
    {
        acquired_.emplace(id, txSet);
    }

    LedgerIndex curMaxSeq = 0;

    auto it = nextProposalCache_.rbegin();
    if (it != nextProposalCache_.rend())
    {
        curMaxSeq = it->first;
        JLOG(j_.info()) << "current highest proposal sequence: " << curMaxSeq;
    }

    for (auto it : curProposalCache_[id])
    {
        if (it.second->block().getLedgerInfo().seq >= curMaxSeq)
            adaptor_.getJobQueue().addJob(
                jtACCEPT, "handle_proposal", [=](Job&) {
                    this->hotstuff_->handleProposal(it.second->block());
                }, adaptor_.app_.doJobCounter());
            //hotstuff_->handleProposal(it.second->block());
        else
            JLOG(j_.warn()) << "proposal for txSet " << id << " is stale";
    }
}

Json::Value
HotstuffConsensus::getJson(bool full) const
{
    using std::to_string;
    using Int = Json::Value::Int;
    using UInt = Json::Value::UInt;

    Json::Value ret(Json::objectValue);

    ret["type"] = "hotstuff";

    ret["proposing"] = (mode_.get() == ConsensusMode::proposing);

    // Maybe consensus isn't begin
    if (previousLedger_.ledger_ != nullptr)
    {
        if (mode_.get() != ConsensusMode::wrongLedger)
        {
            ret["synched"] = true;
            ret["ledger_seq"] = previousLedger_.seq() + 1;
        }
        else
        {
            ret["synched"] = false;
            ret["ledger_seq"] = previousLedger_.seq() + 1;
        }
    }

    ret["new_round"] = static_cast<UInt>(newRound_);

    ret["tx_count_in_pool"] = static_cast<Int>(adaptor_.getPoolTxCount());

    ret["initialized"] = !waitingForInit();

    ret["parms"] = adaptor_.parms().getJson();

    if (full)
    {
        if (!acquired_.empty())
        {
            Json::Value acq(Json::arrayValue);
            for (auto const& at : acquired_)
            {
                acq.append(to_string(at.first));
            }
            ret["acquired"] = std::move(acq);
        }
    }

    return ret;
}

bool
HotstuffConsensus::waitingForInit() const
{
    // This code is for initialization,wait 90 seconds for loading ledger before
    // real start-mode.
    return /*(previousLedger_.seq() == GENESIS_LEDGER_INDEX) &&*/
        (std::chrono::duration_cast<std::chrono::seconds>(now_ - startTime_)
             .count() < adaptor_.parms().initTIME.count());
}

uint64_t
HotstuffConsensus::getCurrentTurn() const
{
    return newRound_;
}

bool
HotstuffConsensus::canExtract()
{
    if (!adaptor_.validating())
    {
        return false;
    }

    auto sinceClose = timeSinceLastClose().count();

    if (sinceClose <= 0)
    {
        sinceClose = std::chrono::duration_cast<std::chrono::milliseconds>(
                         adaptor_.closeTime() - consensusTime_)
                         .count();
    }

    auto txQueuedCount = adaptor_.getPoolQueuedTxCount();

    JLOG(j_.info()) << "sinceClose: " << sinceClose
                    << " txQueuedCount: " << txQueuedCount
                    << " txQueuedCount_: " << txQueuedCount_;

    if (openTime_ < consensusTimeMil_)
    {
        // Case omit empty block, last close time maybe very big
        auto sinceConsensus =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now_ - consensusTime_)
                .count();
        if (sinceConsensus >= adaptor_.parms().maxBLOCK_TIME)
            return true;
    }
    else
    {
        if (sinceClose >= adaptor_.parms().maxBLOCK_TIME)
            return true;
    }

    if (adaptor_.parms().maxTXS_IN_LEDGER >=
            adaptor_.parms().minTXS_IN_LEDGER_ADVANCE &&
        txQueuedCount >= adaptor_.parms().maxTXS_IN_LEDGER &&
        sinceClose >= adaptor_.parms().minBLOCK_TIME / 2)
    {
        return true;
    }

    if (txQueuedCount_ > 0 && txQueuedCount_ == txQueuedCount &&
        sinceClose >= adaptor_.parms().minBLOCK_TIME)
        return true;

    txQueuedCount_ = txQueuedCount;

    return false;
}

boost::optional<hotstuff::Command>
HotstuffConsensus::extract(hotstuff::BlockData& blockData)
{
    // Build new ledger
    if (configChanged_)
    {
        JLOG(j_.info())
            << "Config changed, temporarily unable to proposal new ledger";
        blockData.getLedgerInfo() = previousLedger_.ledger_->info();
        return epochChangeHash_;
    }

    auto txSet = adaptor_.onExtractTransactions(previousLedger_, mode_.get());

    uint256 cmd = txSet->getHash().as_uint256();

    if (acquired_.emplace(cmd, txSet).second)
    {
        adaptor_.share(txSet);
    }

    if (cmd == beast::zero && adaptor_.parms().omitEMPTY)
    {
        JLOG(j_.info()) << "Empty transaction-set and omit empty ledger";
        blockData.getLedgerInfo() = previousLedger_.ledger_->info();
        return cmd;
    }

    //--------------------------------------------------------------------
    auto closeTime = adaptor_.closeTime();
    closeTime = std::max<NetClock::time_point>(
        closeTime,
        previousLedger_.closeTime() +
            std::chrono::seconds{adaptor_.parms().minBLOCK_TIME / 1000});

    auto built = std::make_shared<Ledger>(*previousLedger_.ledger_, closeTime);

    built->setAccepted(
        closeTime, closeResolution_, true, adaptor_.getAppConfig());

    blockData.getLedgerInfo() = built->info();

    return cmd;
}

bool
HotstuffConsensus::compute(
    const hotstuff::Block& block,
    hotstuff::StateComputeResult& result)
{
    if (mode_.get() == ConsensusMode::wrongLedger)
    {
        JLOG(j_.warn()) << "compute block: mode wrongLedger";
        return false;
    }

    if (block.block_data().block_type == hotstuff::BlockData::Genesis)
    {
        JLOG(j_.info()) << "compute block: Genesis Proposal";
        return true;
    }

    if (waitingForInit())
    {
        JLOG(j_.warn()) << "compute block: waiting for init";
        return false;
    }

    LedgerInfo const& info = block.getLedgerInfo();

    if (block.block_data().block_type == hotstuff::BlockData::NilBlock)
    {
        JLOG(j_.info()) << "compute block: NilBlock Proposal";
        result.ledger_info = info;
        result.parent_ledger_info =
            block.block_data().quorum_cert.certified_block().ledger_info;
        return true;
    }

    // --------------------------------------------------------------------------
    // Compute and check block

    auto payload = block.block_data().payload;
    if (!payload)
    {
        JLOG(j_.warn()) << "Proposal missing payload";
        return false;
    }

    // config change
    if (payload->cmd == epochChangeHash_)
    {
        JLOG(j_.warn()) << "leader proposed a config change";

        if (info.hash != previousLedger_.id())
        {
            JLOG(j_.warn()) << "previous ledger conflict with proposed ledger";
            return false;
        }

        hotstuff::EpochState epoch_state;
        epoch_state.epoch = block.block_data().epoch;
        epoch_state.verifier = this;

        result.ledger_info = previousLedger_.ledger_->info();
        result.parent_ledger_info =
            block.block_data().quorum_cert.certified_block().ledger_info;
        result.epoch_state = epoch_state;
        return true;
    }

    // omit empty
    if (adaptor_.parms().omitEMPTY && payload->cmd == beast::zero)
    {
        JLOG(j_.info())
            << "Empty transaction-set from leader and omit empty ledger";

        if (info.hash != previousLedger_.id())
        {
            JLOG(j_.warn()) << "previous ledger conflict with proposed ledger";
            return false;
        }

        result.ledger_info = previousLedger_.ledger_->info();
        result.parent_ledger_info =
            block.block_data().quorum_cert.certified_block().ledger_info;
        return true;
    }

    //--------------------------------------------------------------------
    auto const ait = acquired_.find(payload->cmd);
    if (ait == acquired_.end())
    {
        JLOG(j_.warn()) << "txSet " << payload->cmd << " not acquired";
        return false;
    }

    std::set<TxID> failed;

    // Put transactions into a deterministic, but unpredictable, order
    CanonicalTXSet retriableTxs{payload->cmd};
    JLOG(j_.info()) << "Building canonical tx set: " << retriableTxs.key();

    for (auto const& item : *ait->second.map_)
    {
        try
        {
            retriableTxs.insert(makeSTTx(item.slice()));
            JLOG(j_.debug()) << "    Tx: " << item.key();
        }
        catch (std::exception const&)
        {
            failed.insert(item.key());
            JLOG(j_.warn()) << "    Tx: " << item.key() << " throws!";
        }
    }

    auto built = adaptor_.buildLCL(
        previousLedger_,
        retriableTxs,
        info.closeTime,
        true,
        info.closeTimeResolution,
        std::chrono::milliseconds{0},
        failed);

    JLOG(j_.info()) << "built ledger: " << built.seq() << ":" << built.id();

    result.ledger_info = built.ledger_->info();
    result.parent_ledger_info =
        block.block_data().quorum_cert.certified_block().ledger_info;
    return true;
}

bool
HotstuffConsensus::verify(
    const hotstuff::Block& block,
    const hotstuff::StateComputeResult& result)
{
    if (block.id() == beast::zero)
    {
        JLOG(j_.warn()) << "verify block: block id is zero";
        return false;
    }

    if (result.parent_ledger_info.hash !=
        block.block_data().quorum_cert.certified_block().ledger_info.hash)
    {
        JLOG(j_.warn()) << "verify block: block and result mismatch";
        return false;
    }

    if (block.block_data().block_type == hotstuff::BlockData::Proposal)
    {
        auto payload = block.block_data().payload;
        if (!payload)
        {
            JLOG(j_.warn()) << "verify block: block missing payload";
            return false;
        }

        if ((adaptor_.parms().omitEMPTY && payload->cmd == beast::zero) ||
            payload->cmd == epochChangeHash_)
        {
            return result.ledger_info.seq == result.parent_ledger_info.seq &&
                result.ledger_info.hash == result.parent_ledger_info.hash;
        }
        else
        {
            return result.ledger_info.seq ==
                result.parent_ledger_info.seq + 1 &&
                result.ledger_info.parentHash == result.parent_ledger_info.hash;
        }
    }
    else if (block.block_data().block_type == hotstuff::BlockData::NilBlock)
    {
        return result.ledger_info.seq == result.parent_ledger_info.seq &&
            result.ledger_info.hash == result.parent_ledger_info.hash;
    }
    else
    {
        JLOG(j_.warn()) << "verify block: block type error";
        return false;
    }
}

int
HotstuffConsensus::commit(const hotstuff::ExecutedBlock& executedBlock)
{
    LedgerInfo const& info = executedBlock.state_compute_result.ledger_info;

    ScopedLockType sl(lock_);

    if (auto ledger = adaptor_.checkLedgerAccept(info))
    {
        JLOG(j_.info()) << "commit ledger " << ledger->seq();
        adaptor_.doValidLedger(ledger);
    }

    return 0;
}

bool
HotstuffConsensus::syncState(const hotstuff::BlockInfo& prevInfo)
{
    newRound_ = prevInfo.round + 1;

    if (waitingForInit())
    {
        JLOG(j_.warn()) << "I'm waiting for initial ledger";
        return false;
    }

    consensusTime_ = now_;
    consensusTimeMil_ = utcTime();

    ScopedLockType sl(lock_);

    if (prevInfo.ledger_info.seq > previousLedger_.seq())
    {
        if (!adaptor_.doAccept(prevInfo.ledger_info.hash))
        {
            return handleWrongLedger(prevInfo.ledger_info.hash);
        }
    }
    else if (
        prevInfo.ledger_info.seq == previousLedger_.seq() &&
        adaptor_.parms().omitEMPTY && prevInfo.round > 0)
    {
        adaptor_.onConsensusReached(waitingConsensusReach_, previousLedger_, newRound_);
        if (waitingConsensusReach_)
        {
            waitingConsensusReach_ = false;
        }
    }

    return true;
}

/* deprecated */
bool
HotstuffConsensus::syncBlock(
    const uint256& blockID,
    const hotstuff::Author& author,
    hotstuff::ExecutedBlock& executedBlock)
{
    return false;
}

void
HotstuffConsensus::asyncBlock(
    const uint256& blockID,
    const hotstuff::Author& author,
    hotstuff::StateCompute::AsyncCompletedHander asyncCompletedHandler)
{
    // Send acquire block message
    adaptor_.acquireBlock(author, blockID);

    if (auto cbs = blockAcquiring_.fetch(blockID))
    {
        cbs->push_back(asyncCompletedHandler);
    }
    else
    {
        blockAcquiring_.insert(
            blockID,
            std::vector<hotstuff::StateCompute::AsyncCompletedHander>{
                asyncCompletedHandler});
    }
}

const hotstuff::Author&
HotstuffConsensus::Self() const
{
    return adaptor_.valPublic();
}

bool
HotstuffConsensus::signature(
    const uint256& digest,
    hotstuff::Signature& signature)
{
    signature = signDigest(adaptor_.valPublic(), adaptor_.valSecret(), digest);
    return true;
}

bool
HotstuffConsensus::verifySignature(
    const hotstuff::Author& author,
    const hotstuff::Signature& signature,
    const hotstuff::HashValue& digest) const
{
    if (adaptor_.trusted(author))
    {
        return verifyDigest(
            author, digest, Slice(signature.data(), signature.size()), false);
    }

    return false;
}

bool
HotstuffConsensus::verifySignature(
    const hotstuff::Author& author,
    const hotstuff::Signature& signature,
    const hotstuff::Block& block) const
{
    if (calculateLedgerHash(block.block_data().getLedgerInfo()) !=
        block.block_data().getLedgerInfo().hash)
    {
        JLOG(j_.warn()) << "verify block signature: LedgerInfo hash missmatch";
        return false;
    }

    if (hotstuff::BlockData::hash(block.block_data()) != block.id())
    {
        JLOG(j_.warn()) << "verify block signature: Block id missmatch";
        return false;
    }

    return verifyDigest(
        author, block.id(), Slice(signature.data(), signature.size()), false);
}

bool
HotstuffConsensus::verifySignature(
    const hotstuff::Author& author,
    const hotstuff::Signature& signature,
    const hotstuff::Vote& vote) const
{
    if (vote.vote_data().hash() != vote.ledger_info().consensus_data_hash)
    {
        JLOG(j_.warn()) << "verify vote signature: vote hash missmatch";
        return false;
    }

    return verifyDigest(
        author,
        vote.ledger_info().consensus_data_hash,
        Slice(signature.data(), signature.size()),
        false);
}

bool
HotstuffConsensus::verifyLedgerInfo(
    const hotstuff::BlockInfo& commit_info,
    const hotstuff::HashValue& consensus_data_hash,
    const std::map<hotstuff::Author, hotstuff::Signature>& signatures)
{
    // 1. Check previous vote whether the consensus threshold has been reached
    for (auto iter = signatures.begin(); iter != signatures.end(); iter++)
    {
        boost::optional<PublicKey> pubKey = adaptor_.getTrustedKey(iter->first);
        if (!pubKey)
        {
            return false;
        }

        if (!verifyDigest(
                iter->first,
                consensus_data_hash,
                Slice(iter->second.data(), iter->second.size()),
                false))
        {
            return false;
        }
    }

    if (signatures.size() < adaptor_.getQuorum())
    {
        return false;
    }

    // 2. Do something check for previous commit_info

    return true;
}

bool
HotstuffConsensus::checkVotingPower(
    const std::map<hotstuff::Author, hotstuff::Signature>& signatures) const
{
    return signatures.size() >= adaptor_.getQuorum();
}

void
HotstuffConsensus::broadcast(
    const hotstuff::Block& block,
    const hotstuff::SyncInfo& syncInfo)
{
    auto proposal =
        std::make_shared<STProposal>(block, syncInfo, adaptor_.valPublic());

    adaptor_.broadcast(*proposal);

    adaptor_.getJobQueue().addJob(
        jtACCEPT, "handle_proposal", [this, block](Job&) {
            //ScopedLockType _(this->adaptor_.peekConsensusMutex());
            this->hotstuff_->handleProposal(block);
        }, adaptor_.app_.doJobCounter());
}

void
HotstuffConsensus::broadcast(
    const hotstuff::Vote& vote,
    const hotstuff::SyncInfo& syncInfo)
{
    auto stVote =
        std::make_shared<STVote>(vote, syncInfo, adaptor_.valPublic());

    adaptor_.broadcast(*stVote);

    adaptor_.getJobQueue().addJob(
        jtCONSENSUS_t, "broadcast_vote", [this, vote, syncInfo](Job&) {
            ScopedLockType _(this->adaptor_.peekConsensusMutex());
            this->hotstuff_->handleVote(vote, syncInfo);
        }, adaptor_.app_.doJobCounter());
}

void
HotstuffConsensus::sendVote(
    const hotstuff::Author& author,
    const hotstuff::Vote& vote,
    const hotstuff::SyncInfo& syncInfo)
{
    if (author == adaptor_.valPublic())
    {
        adaptor_.getJobQueue().addJob(
            jtCONSENSUS_t, "send_vote", [this, vote, syncInfo](Job&) {
                ScopedLockType _(this->adaptor_.peekConsensusMutex());
                this->hotstuff_->handleVote(vote, syncInfo);
            }, adaptor_.app_.doJobCounter());
    }
    else
    {
        auto stVote =
            std::make_shared<STVote>(vote, syncInfo, adaptor_.valPublic());
        adaptor_.sendVote(author, *stVote);
    }
}

void
HotstuffConsensus::broadcast(
    const hotstuff::EpochChange& epochChange,
    const hotstuff::SyncInfo& syncInfo)
{
    auto stEpochChange = std::make_shared<STEpochChange>(
        epochChange, syncInfo, adaptor_.valPublic());

    adaptor_.broadcast(*stEpochChange);

    adaptor_.getJobQueue().addJob(
        jtCONSENSUS_t,
        "broadcast_epochChange",
        [this, epochChange, syncInfo](Job&) {
            ScopedLockType _(this->adaptor_.peekConsensusMutex());
            if (hotstuff_->checkEpochChange(epochChange, syncInfo))
            {
                configChanged_ = false;
                // epoch_++;
                startRoundInternal(
                    now_,
                    prevLedgerID_,
                    previousLedger_,
                    ConsensusMode::switchedLedger,
                    true);
            }
        }, adaptor_.app_.doJobCounter());
}

std::chrono::milliseconds 
HotstuffConsensus::getConsensusTimeOut() const
{
    return adaptor_.parms().consensusTIMEOUT;
}

// -------------------------------------------------------------------
// Private member functions

std::chrono::milliseconds
HotstuffConsensus::timeSinceLastClose() const
{
    using namespace std::chrono;
    // This computes how long since last ledger's close time
    std::chrono::milliseconds sinceClose;
    {
        bool previousCloseCorrect =
            (mode_.get() != ConsensusMode::wrongLedger) &&
            previousLedger_.closeAgree() &&
            (previousLedger_.closeTime().time_since_epoch().count() != 0);

        auto lastCloseTime = previousCloseCorrect
            ? previousLedger_.closeTime()  // use consensus timing
            : consensusTime_;              // use the time we saw internally

        if (now_ >= lastCloseTime)
            sinceClose = std::chrono::duration_cast<std::chrono::milliseconds>(
                now_ - lastCloseTime);
        else
            sinceClose = -std::chrono::duration_cast<std::chrono::milliseconds>(
                lastCloseTime - now_);
    }
    return sinceClose;
}

void
HotstuffConsensus::initAnnounce()
{
    if (now_ - initAnnounceTime_ < adaptor_.parms().initANNOUNCE_INTERVAL)
        return;

    initAnnounceTime_ = now_;

    JLOG(j_.info()) << "Init announce to other peers prevSeq="
                    << previousLedger_.seq() << ", prevHash=" << prevLedgerID_;

    auto initAnnounce = std::make_shared<STInitAnnounce>(
        previousLedger_.seq(),
        prevLedgerID_,
        adaptor_.valPublic(),
        adaptor_.closeTime());

    adaptor_.InitAnnounce(*initAnnounce);
}

void
HotstuffConsensus::peerProposal(
    std::shared_ptr<PeerImp>& peer,
    bool isTrusted,
    std::shared_ptr<protocol::TMConsensus> const& m)
{
    if (!isTrusted)
    {
        JLOG(j_.info()) << "drop UNTRUSTED proposal";
        return;
    }

    try
    {
        STProposal::pointer proposal;

        SerialIter sit(makeSlice(m->msg()));
        PublicKey const publicKey{makeSlice(m->signerpubkey())};

        proposal = std::make_shared<STProposal>(sit, publicKey);

        // Check message public key same with proposal public key.
        if (proposal->nodePublic() != proposal->block().block_data().author())
        {
            JLOG(j_.warn()) << "message public key different with proposal "
                               "public key, drop proposal";
            return;
        }

        if (waitingForInit())
        {
            LedgerInfo const& netLoaded = proposal->block()
                                              .block_data()
                                              .quorum_cert.certified_block()
                                              .ledger_info;
            if (netLoaded.seq > previousLedger_.seq())
            {
                JLOG(j_.warn()) << "Init time switch to netLedger "
                                << netLoaded.seq << ":" << netLoaded.hash;
                handleWrongLedger(netLoaded.hash);
            }
        }

        if (hotstuff_->CheckProposal(proposal->block(), proposal->syncInfo()))
        {
            peerProposalInternal(proposal);
        }
        else  // Maybe acquiring previous hotstuff block
        {
            if (blockAcquiring_.fetch(proposal->syncInfo()
                                          .HighestQuorumCert()
                                          .certified_block()
                                          .id))
            {
                auto payload = proposal->block().block_data().payload;
                if (!payload)
                {
                    Throw<std::runtime_error>("proposal missing payload");
                }
                nextProposalCache_[proposal->block().getLedgerInfo().seq]
                                  [payload->author] = proposal;
            }
        }
    }
    catch (std::exception const& e)
    {
        JLOG(j_.warn()) << "Proposal: Exception, " << e.what();
        peer->charge(Resource::feeInvalidRequest);
    }
}

void
HotstuffConsensus::peerProposalInternal(STProposal::ref proposal)
{
    LedgerInfo const& info = proposal->block().getLedgerInfo();

    JLOG(j_.info()) << "peerProposalInternal proposal seq=" << info.seq
                    << ", prev seq=" << previousLedger_.seq();

    if (info.seq < previousLedger_.seq())
    {
        JLOG(j_.warn()) << "proposal is fall behind";
        return;
    }

    auto payload = proposal->block().block_data().payload;
    if (!payload)
    {
        JLOG(j_.warn()) << "proposal missing payload";
        return;
    }

    if (info.seq == previousLedger_.seq())
    {
        if (!adaptor_.parms().omitEMPTY || payload->cmd != beast::zero)
        {
            JLOG(j_.warn()) << "proposal is fall behind";
            return;
        }
    }

    if (info.seq > previousLedger_.seq() + 1)
    {
        JLOG(j_.warn()) << "we are fall behind";
        prevLedgerID_ = info.parentHash;
        adaptor_.acquireLedger(info.parentHash);
        nextProposalCache_[info.seq][payload->author] = proposal;
        return;
    }

    auto const ait = acquired_.find(payload->cmd);
    if (ait == acquired_.end())
    {
        JLOG(j_.info()) << "Don't have tx set for proposal:" << payload->cmd;

        curProposalCache_[payload->cmd][payload->author] = proposal;

        // acquireTxSet will return the set if it is available, or
        // spawn a request for it and return none/nullptr.  It will call
        // gotTxSet once it arrives
        if (auto set = adaptor_.acquireTxSet(payload->cmd))
        {
            gotTxSet(adaptor_.closeTime(), *set);
        }
        return;
    }

    adaptor_.getJobQueue().addJob(jtACCEPT, "handle_proposal", [=](Job&) {
        this->hotstuff_->handleProposal(proposal->block());
    }, adaptor_.app_.doJobCounter());
    //hotstuff_->handleProposal(proposal->block());
}

void
HotstuffConsensus::peerVote(
    std::shared_ptr<PeerImp>& peer,
    bool isTrusted,
    std::shared_ptr<protocol::TMConsensus> const& m)
{
    if (!isTrusted)
    {
        JLOG(j_.info()) << "drop UNTRUSTED vote";
        return;
    }

    try
    {
        STVote::pointer vote;

        SerialIter sit(makeSlice(m->msg()));
        PublicKey const publicKey{makeSlice(m->signerpubkey())};

        vote = std::make_shared<STVote>(sit, publicKey);

        // Check message public key same with vote public key.
        if (vote->nodePublic() != vote->vote().author())
        {
            JLOG(j_.warn()) << "message public key different with vote public "
                               "key, drop vote";
            return;
        }

        if (waitingForInit())
        {
            LedgerInfo const& netLoaded =
                vote->vote().vote_data().parent().ledger_info;
            if (netLoaded.seq > previousLedger_.seq())
            {
                JLOG(j_.warn()) << "Init time switch to netLedger "
                                << netLoaded.seq << ":" << netLoaded.hash;
                handleWrongLedger(netLoaded.hash);
            }
        }

        hotstuff_->handleVote(vote->vote(), vote->syncInfo());
    }
    catch (std::exception const& e)
    {
        JLOG(j_.warn()) << "Vote: Exception, " << e.what();
        peer->charge(Resource::feeInvalidRequest);
    }
}

void
HotstuffConsensus::peerAcquireBlock(
    std::shared_ptr<PeerImp>& peer,
    bool isTrusted,
    std::shared_ptr<protocol::TMConsensus> const& m)
{
    if (m->msg().size() != 32)
    {
        JLOG(j_.warn()) << "Acquire block: malformed";
        peer->charge(Resource::feeInvalidSignature);
        return;
    }

    uint256 blockID;
    memcpy(blockID.begin(), m->msg().data(), 32);

    hotstuff::ExecutedBlock block;

    if (!hotstuff_->expectBlock(blockID, block))
    {
        JLOG(j_.warn()) << "Acquire block: not found, block hash: " << blockID;
        return;
    }

    adaptor_.sendBLock(peer, block);
}

void
HotstuffConsensus::peerBlockData(
    std::shared_ptr<PeerImp>& peer,
    bool isTrusted,
    std::shared_ptr<protocol::TMConsensus> const& m)
{
    if (!isTrusted)
    {
        JLOG(j_.info()) << "drop UNTRUSTED block data";
        return;
    }

    try
    {
        hotstuff::ExecutedBlock executedBlock =
            serialization::deserialize<hotstuff::ExecutedBlock>(
                Buffer(m->msg().data(), m->msg().size()));

        if (!verify(executedBlock.block, executedBlock.state_compute_result))
        {
            JLOG(j_.warn())
                << "acquired block " << executedBlock.block.id()
                << " round: " << executedBlock.block.block_data().round
                << ", but verify failed";
            return;
        }

        JLOG(j_.info()) << "acquired block " << executedBlock.block.id()
                        << " round: " << executedBlock.block.block_data().round
                        << " success";

        auto hanlders = blockAcquiring_.fetch(executedBlock.block.id());
        if (hanlders)
        {
            for (auto const& cb : *hanlders)
            {
                boost::optional<hotstuff::Block> block = executedBlock.block;
                if (hotstuff::StateCompute::AsyncBlockResult::
                        PROPOSAL_SUCCESS ==
                    cb(hotstuff::StateCompute::AsyncBlockErrorCode::
                           ASYNC_SUCCESS,
                       executedBlock,
                       block))
                {
                    assert(block && block->block_data().payload);
                    auto payload = block->block_data().payload;
                    auto proposal =
                        nextProposalCache_[block->getLedgerInfo().seq]
                                          [payload->author];
                    if (proposal && proposal->block().id() == block->id())
                    {
                        peerProposalInternal(proposal);
                    }
                }
            }

            blockAcquiring_.del(executedBlock.block.id(), false);
        }
    }
    catch (std::exception const& e)
    {
        JLOG(j_.warn()) << "BlockData: Exception, " << e.what();
        peer->charge(Resource::feeInvalidRequest);
    }
}

void
HotstuffConsensus::peerEpochChange(
    std::shared_ptr<PeerImp>& peer,
    bool isTrusted,
    std::shared_ptr<protocol::TMConsensus> const& m)
{
    if (!isTrusted)
    {
        JLOG(j_.info()) << "drop UNTRUSTED vote";
        return;
    }

    try
    {
        STEpochChange::pointer epochChange;

        SerialIter sit(makeSlice(m->msg()));
        PublicKey const publicKey{makeSlice(m->signerpubkey())};

        epochChange = std::make_shared<STEpochChange>(sit, publicKey);

        // Check message public key same with vote public key.
        if (epochChange->nodePublic() != epochChange->epochChange().author)
        {
            JLOG(j_.warn()) << "message public key different with epochChange "
                               "public key, drop epochChange";
            return;
        }

        if (hotstuff_->checkEpochChange(
                epochChange->epochChange(), epochChange->syncInfo()))
        {
            configChanged_ = false;
            // epoch_++;
            startRoundInternal(
                now_,
                prevLedgerID_,
                previousLedger_,
                ConsensusMode::switchedLedger,
                true);
        }
    }
    catch (std::exception const& e)
    {
        JLOG(j_.warn()) << "Vote: Exception, " << e.what();
        peer->charge(Resource::feeInvalidRequest);
    }
}

void
HotstuffConsensus::peerInitAnnounce(
    std::shared_ptr<PeerImp>& peer,
    bool isTrusted,
    std::shared_ptr<protocol::TMConsensus> const& m)
{
    if (!isTrusted)
    {
        JLOG(j_.info()) << "drop UNTRUSTED InitAnnounce";
        return;
    }

    try
    {
        STInitAnnounce::pointer initAnnounce;

        SerialIter sit(makeSlice(m->msg()));
        PublicKey const publicKey{makeSlice(m->signerpubkey())};

        initAnnounce = std::make_shared<STInitAnnounce>(sit, publicKey);

        peerInitAnnounceInternal(initAnnounce);
    }
    catch (std::exception const& e)
    {
        JLOG(j_.warn()) << "InitAnnounce: Exception, " << e.what();
        peer->charge(Resource::feeInvalidRequest);
    }

    return;
}

void
HotstuffConsensus::peerInitAnnounceInternal(STInitAnnounce::ref initAnnounce)
{
    if (!waitingForInit())
    {
        JLOG(j_.info()) << "Ignored InitAnnounce, I'm initialized";
        return;
    }

    JLOG(j_.info()) << "Processing peer InitAnnounce prevSeq="
                    << initAnnounce->prevSeq()
                    << ", prevHash=" << initAnnounce->prevHash();

    if (initAnnounce->prevSeq() > previousLedger_.seq())
    {
        // if mode_.get() == ConsensusMode::wrongLedger
        // acquiring a netLedger now, so don't checkLedger.
        if (mode_.get() != ConsensusMode::wrongLedger)
        {
            JLOG(j_.warn())
                << "Init time switch to netLedger " << initAnnounce->prevSeq()
                << ":" << initAnnounce->prevHash();
            handleWrongLedger(initAnnounce->prevHash());
        }
        else if (initAnnounce->prevHash() == prevLedgerID_)
        {
            // acquiring netLedger is prevLedgerID_, touch it.
            adaptor_.touchAcquringLedger(prevLedgerID_);
        }
    }

    return;
}

void
HotstuffConsensus::peerValidation(
    std::shared_ptr<PeerImp>& peer,
    bool isTrusted,
    std::shared_ptr<protocol::TMConsensus> const& m)
{
    if (!isTrusted)
    {
        JLOG(j_.info()) << "drop UNTRUSTED validattion";
        return;
    }

    if (m->msg().size() < 50)
    {
        JLOG(j_.warn()) << "Validation: Too small";
        peer->charge(Resource::feeInvalidRequest);
        return;
    }

    try
    {
        STValidation::pointer val;
        {
            SerialIter sit(makeSlice(m->msg()));
            PublicKey const publicKey{makeSlice(m->signerpubkey())};
            val = std::make_shared<STValidation>(
                std::ref(sit), publicKey, [&](PublicKey const& pk) {
                    return calcNodeID(adaptor_.getMasterKey(pk));
                });

            if (((val->getFieldU32(sfLedgerSequence) + 1) % 256) != 0)
            {
                JLOG(j_.warn()) << "Validation: Not a flag ledger";
                peer->charge(Resource::feeInvalidRequest);
                return;
            }

            val->setSeen(adaptor_.closeTime());
            val->setTrusted();
        }

        adaptor_.peerValidation(peer, val);
    }
    catch (std::exception const& e)
    {
        JLOG(j_.warn()) << "Validation: Exception, " << e.what();
        peer->charge(Resource::feeInvalidRequest);
    }

    return;
}

void
HotstuffConsensus::startRoundInternal(
    NetClock::time_point const& now,
    RCLCxLedger::ID const& prevLgrId,
    RCLCxLedger const& prevLgr,
    ConsensusMode mode,
    bool recover)
{
    JLOG(j_.info()) << "startRoundInternal";

    now_ = now;
    openTime_ = utcTime();

    mode_.set(mode, adaptor_);

    prevLedgerID_ = prevLgrId;
    previousLedger_ = prevLgr;

    acquired_.clear();
    curProposalCache_.clear();

    txQueuedCount_ = 0;

    if (recover)
    {
        if (startup_)
        {
            JLOG(j_.info())
                << "recover hotstuff, used ledger " << previousLedger_.seq();
            hotstuff_->stop();
        }
        else
        {
            JLOG(j_.info()) << "startup once hotstuff";
            startup_ = true;
            startTime_ = now;
            consensusTime_ = now;
            sweepTime_ = now;
        }

        hotstuff::EpochState init_epoch_state;
        init_epoch_state.epoch = epoch_;
        init_epoch_state.verifier = this;

        hotstuff_->start(hotstuff::RecoverData{previousLedger_.ledger_->info(),
                                               init_epoch_state,
                                               adaptor_.validating()});

        nextProposalCache_.clear();
    }

    if (mode != ConsensusMode::wrongLedger)
    {
        checkCache();
    }
}

void
HotstuffConsensus::checkCache()
{
    std::uint32_t curSeq = previousLedger_.seq() + 1;
    if (nextProposalCache_.find(curSeq) != nextProposalCache_.end())
    {
        JLOG(j_.info()) << "Handle cached proposal seq: " << curSeq;
        for (auto const& it : nextProposalCache_[curSeq])
        {
            if (!blockAcquiring_.fetch(it.second->syncInfo()
                                           .HighestQuorumCert()
                                           .certified_block()
                                           .id))
            {
                peerProposalInternal(it.second);
            }
        }
    }

    for (auto it = nextProposalCache_.begin(); it != nextProposalCache_.end();)
    {
        if (it->first < curSeq - 1)
        {
            it = nextProposalCache_.erase(it);
        }
        else
        {
            it++;
        }
    }
}

bool
HotstuffConsensus::handleWrongLedger(typename Ledger_t::ID const& lgrId)
{
    assert(lgrId != prevLedgerID_ || previousLedger_.id() != lgrId);

    // Stop proposing because we are out of sync
    if (mode_.get() == ConsensusMode::proposing)
    {
        mode_.set(ConsensusMode::observing, adaptor_);
        JLOG(j_.warn()) << "Bowing out of consensus";
    }

    // First time switching to this ledger
    if (prevLedgerID_ != lgrId)
    {
        prevLedgerID_ = lgrId;
    }

    if (previousLedger_.id() == prevLedgerID_)
        return true;

    // we need to switch the ledger we're working from
    if (auto newLedger = adaptor_.acquireLedger(prevLedgerID_))
    {
        JLOG(j_.info()) << "Have the consensus ledger " << newLedger->seq()
                        << ":" << prevLedgerID_;
        adaptor_.removePoolTxs(
            newLedger->ledger_->txMap(),
            newLedger->ledger_->info().seq,
            newLedger->ledger_->info().parentHash);

        startRoundInternal(
            now_,
            lgrId,
            *newLedger,
            ConsensusMode::switchedLedger,
            waitingForInit());

        return true;
    }

    mode_.set(ConsensusMode::wrongLedger, adaptor_);

    return false;
}

}  // namespace ripple