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


#include <ripple/basics/Log.h>
#include <ripple/json/json_writer.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/ledger/OpenLedger.h>
#include <ripple/app/misc/AmendmentTable.h>
#include <peersafe/consensus/LedgerTiming.h>
#include <peersafe/consensus/pop/PopConsensus.h>
#include <peersafe/consensus/pop/PopConsensusParams.h>


namespace ripple {


// -------------------------------------------------------------------
// Public member functions

PopConsensus::PopConsensus(Adaptor& adaptor, clock_type const& clock, beast::Journal journal)
    : ConsensusBase(clock, journal)
    , adaptor_(*(PopAdaptor*)(&adaptor))
    , viewChangeManager_{ journal }
{
    JLOG(j_.info()) << "Creating POP consensus object";
}

void PopConsensus::startRound(
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

    startRoundInternal(now, prevLedgerID, prevLedger, startMode);
}

void PopConsensus::timerEntry(NetClock::time_point const& now)
{
    // Nothing to do if we are currently working on a ledger
    if (phase_ == ConsensusPhase::accepted)
        return;
    // Check we are on the proper ledger (this may change phase_)
    checkLedger();

    if (waitingForInit())
    {
        consensusTime_ = utcTime();
        return;
    }

    now_ = now;

    if (mode_.get() == ConsensusMode::wrongLedger)
    {
        if (auto newLedger = adaptor_.acquireLedger(prevLedgerID_))
        {
            JLOG(j_.info()) << "Have the consensus ledger " << newLedger->seq() << ":" << prevLedgerID_;
            adaptor_.removePoolTxs(
                newLedger->ledger_->txMap(),
                newLedger->ledger_->info().seq,
                newLedger->ledger_->info().parentHash);

            startRoundInternal(now_, prevLedgerID_, *newLedger, ConsensusMode::switchedLedger);
        }
        return;
    }

    //Long time no consensus reach,rollback to initial state.
    //What if 2 of 4 validate new ledger success, but other 2 of 4 not ,can roll back work,or is there such occasion?
    if (timeOutCount_ > adaptor_.parms().timeoutCOUNT_ROLLBACK)
    {
        auto valLedger = adaptor_.getValidLedgerIndex();
        if (view_ > 0 || previousLedger_.seq() > valLedger)
        {
            JLOG(j_.warn()) << "There have been " << adaptor_.parms().timeoutCOUNT_ROLLBACK
                << " times of timeout, will rollback to validated ledger " << valLedger;
            if (auto oldLedger = adaptor_.getValidatedLedger())
            {
                startRoundInternal(
                    now_, oldLedger->info().hash, oldLedger, ConsensusMode::switchedLedger);
                //Clear view-change cache after initial state.
                viewChangeManager_.clearCache();
                //Clear validation cache,in case "checkLedger move back to advanced ledger".
                adaptor_.flushValidations();
            }
        }
    }

    if (phase_ == ConsensusPhase::open)
    {
        phaseCollecting();
    }

    checkTimeout();
}

bool PopConsensus::peerConsensusMessage(
    std::shared_ptr<PeerImp>& peer,
    bool isTrusted,
    std::shared_ptr<protocol::TMConsensus> const& m)
{
    switch (m->msgtype())
    {
    case ConsensusMessageType::mtPROPOSESET:
        return peerProposal(peer, isTrusted, m);
    case ConsensusMessageType::mtVIEWCHANGE:
        return peerViewChange(peer, isTrusted, m);
    case ConsensusMessageType::mtVALIDATION:
        return peerValidation(peer, isTrusted, m);
    default:
        break;
    }

    return false;
}

void PopConsensus::gotTxSet(NetClock::time_point const& now, TxSet_t const& txSet)
{
    // Nothing to do if we've finished work on a ledger
    if (phase_ == ConsensusPhase::accepted)
        return;

    now_ = now;

    auto id = txSet.id();

    if (acquired_.find(id) == acquired_.end())
    {
        acquired_.emplace(id, txSet);
    }

    if (!setID_ || setID_ != id)
    {
        return;
    }

    if (!adaptor_.isLeader(previousLedger_.seq() + 1, view_))
    {
        //update avoid if we got the right tx-set
        if (adaptor_.validating())
            adaptor_.updatePoolAvoid(txSet, previousLedger_.seq());

        auto set = txSet.map_->snapShot(false);
        //this place has a txSet copy,what's the time it costs?
        result_.emplace(Result(
            std::move(set),
            RCLCxPeerPos::Proposal(
                RCLCxPeerPos::Proposal::seqJoin,
                id,
                prevLedgerID_,
                closeTime_,
                now,
                adaptor_.nodeID(),
                adaptor_.valPublic(),
                previousLedger_.seq() + 1,
                view_)));

        if (phase_ == ConsensusPhase::open)
            phase_ = ConsensusPhase::establish;

        JLOG(j_.info()) << "gotTxSet time elapsed since receive set_id from leader:" << (now - rawCloseTimes_.self).count();

        if (adaptor_.validating())
        {
            JLOG(j_.info()) << "We are not leader gotTxSet, and proposing position: " << id;
            txSetVoted_[*setID_].insert(adaptor_.valPublic());
            adaptor_.propose(result_->position);
        }

        result_->roundTime.reset(proposalTime_);

        JLOG(j_.info()) << "List current votes for set(" << *setID_
            << "), total count: " << txSetVoted_[*setID_].size();
        for (auto publicKey : txSetVoted_[*setID_])
        {
            JLOG(j_.info()) << "PublicKey index: " << adaptor_.getPubIndex(publicKey);
        }
    }

    //check to see if final condition reached.
    if (result_)
    {
        checkVoting();
        return;
    }
}

Json::Value PopConsensus::getJson(bool full) const
{
    using std::to_string;
    using Int = Json::Value::Int;

    Json::Value ret(Json::objectValue);

    ret["proposing"] = (mode_.get() == ConsensusMode::proposing);
    ret["proposers"] = static_cast<Int>(txSetVoted_.size());

    if (mode_.get() != ConsensusMode::wrongLedger)
    {
        ret["synched"] = true;
        ret["ledger_seq"] = previousLedger_.seq() + 1;
        ret["close_granularity"] = static_cast<Int>(closeResolution_.count());
    }
    else
    {
        ret["synched"] = false;
        ret["ledger_seq"] = previousLedger_.seq() + 1;
    }

    ret["phase"] = to_string(phase_);
    if (phase_ == ConsensusPhase::open)
    {
        ret["transaction_count"] = static_cast<Int>(transactions_.size());
    }

    ret["tx_count_in_pool"] = static_cast<Int>(adaptor_.getPoolTxCount());

    ret["time_out"] = static_cast<Int>(adaptor_.parms().consensusTIMEOUT.count());
    ret["initialized"] = !waitingForInit();

    if (full)
    {
        if (result_)
            ret["current_ms"] =
            static_cast<Int>(result_->roundTime.read().count());
        ret["close_resolution"] = static_cast<Int>(closeResolution_.count());
        //ret["have_time_consensus"] = haveCloseTimeConsensus_;

        if (!acquired_.empty())
        {
            Json::Value acq(Json::arrayValue);
            for (auto const& at : acquired_)
            {
                acq.append(to_string(at.first));
            }
            ret["acquired"] = std::move(acq);
        }

        if (!rawCloseTimes_.peers.empty())
        {
            Json::Value ctj(Json::objectValue);
            for (auto const& ct : rawCloseTimes_.peers)
            {
                ctj[std::to_string(ct.first.time_since_epoch().count())] =
                    ct.second;
            }
            ret["close_times"] = std::move(ctj);
        }
    }

    return ret;
}

// -------------------------------------------------------------------
// Private member functions

std::chrono::milliseconds PopConsensus::timeSinceLastClose()
{
    using namespace std::chrono;
    // This computes how long since last ledger's close time
    milliseconds sinceClose;
    {
        bool previousCloseCorrect =
            (mode_.get() != ConsensusMode::wrongLedger) &&
            previousLedger_.closeAgree() &&
            (previousLedger_.closeTime().time_since_epoch().count() != 0);

        auto lastCloseTime = previousCloseCorrect
            ? previousLedger_.closeTime()  // use consensus timing
            : openTime_;              // use the time we saw internally

        if (now_ >= lastCloseTime)
            sinceClose = duration_cast<milliseconds>(now_ - lastCloseTime);
        else
            sinceClose = -duration_cast<milliseconds>(lastCloseTime - now_);
    }
    return sinceClose;
}

bool PopConsensus::waitingForInit() const
{
    // This code is for initialization,wait 60 seconds for loading ledger before real start-mode.
    if (previousLedger_.seq() == GENESIS_LEDGER_INDEX &&
        timeSinceOpen() / 1000 < adaptor_.parms().initTIME.count())
    {
        return true;
    }
    return false;
}

void PopConsensus::startRoundInternal(
    NetClock::time_point const& now,
    typename Ledger_t::ID const& prevLedgerID,
    Ledger_t const& prevLedger,
    ConsensusMode mode)
{
    phase_ = ConsensusPhase::open;
    mode_.set(mode, adaptor_);
    now_ = now;
    closeTime_ = now;
    openTime_ = now;
    openTimeMilli_ = utcTime();
    consensusTime_ = openTimeMilli_;
    prevLedgerID_ = prevLedgerID;
    previousLedger_ = prevLedger;
    result_.reset();
    acquired_.clear();
    rawCloseTimes_.peers.clear();
    rawCloseTimes_.self = {};
    txSetCached_.clear();
    txSetVoted_.clear();
    transactions_.clear();
    setID_.reset();
    leaderFailed_ = false;
    extraTimeOut_ = false;
    timeOutCount_ = 0;
    //reset view to 0 after a new close ledger.
    view_ = 0;
    toView_ = 0;

    closeResolution_ = getNextLedgerTimeResolution(
        previousLedger_.closeTimeResolution(),
        previousLedger_.closeAgree(),
        previousLedger_.seq() + 1);

    if (mode == ConsensusMode::proposing)
    {
        if (bWaitingInit_ && previousLedger_.seq() != GENESIS_LEDGER_INDEX)
            bWaitingInit_ = false;

        viewChangeManager_.onNewRound(previousLedger_);

        checkCache();
    }
}

void PopConsensus::checkLedger()
{
    auto netLgr =
        adaptor_.getPrevLedger(prevLedgerID_, previousLedger_, mode_.get());

    if (netLgr != prevLedgerID_)
    {
        JLOG(j_.warn()) << "View of consensus changed during "
            << to_string(phase_) << " status=" << to_string(phase_)
            << ", "
            << " mode=" << to_string(mode_.get());
        JLOG(j_.warn()) << prevLedgerID_ << " to " << netLgr;
        JLOG(j_.warn()) << previousLedger_.getJson();
        JLOG(j_.debug()) << "State on consensus change " << getJson(true);
        handleWrongLedger(netLgr);
    }
    else if (previousLedger_.id() != prevLedgerID_)
        handleWrongLedger(netLgr);
}

// Handle a change in the prior ledger during a consensus round
void PopConsensus::handleWrongLedger(typename Ledger_t::ID const& lgrId)
{
    assert(lgrId != prevLedgerID_ || previousLedger_.id() != lgrId);

    // Stop proposing because we are out of sync
    leaveConsensus();

    // First time switching to this ledger
    if (prevLedgerID_ != lgrId)
    {
        prevLedgerID_ = lgrId;

        rawCloseTimes_.peers.clear();
    }

    if (previousLedger_.id() == prevLedgerID_)
        return;

    // we need to switch the ledger we're working from
    if (auto newLedger = adaptor_.acquireLedger(prevLedgerID_))
    {
        JLOG(j_.info()) << "Have the consensus ledger " << newLedger->seq() << ":" << prevLedgerID_;
        adaptor_.removePoolTxs(
            newLedger->ledger_->txMap(),
            newLedger->ledger_->info().seq,
            newLedger->ledger_->info().parentHash);

        startRoundInternal(now_, lgrId, *newLedger, ConsensusMode::switchedLedger);
    }
    else
    {
        mode_.set(ConsensusMode::wrongLedger, adaptor_);
    }
}

void PopConsensus::checkCache()
{
    std::uint32_t curSeq = previousLedger_.seq() + 1;
    if (proposalCache_.find(curSeq) != proposalCache_.end())
    {
        JLOG(j_.info()) << "Check peerProposalInternal after startRoundInternal";
        for (auto it = proposalCache_[curSeq].begin(); it != proposalCache_[curSeq].end(); it++)
        {
            if (peerProposalInternal(now_, it->second))
            {
                JLOG(j_.info()) << "Position " << it->second.proposal().position()
                    << " from (PublicKey index)" << adaptor_.getPubIndex(it->first) << " success";
            }
        }
        auto iter = proposalCache_.begin();
        while (iter != proposalCache_.end())
        {
            /**
             * Maybe prosoal seq meet curSeq, but view is feture,
             * so don't remove propal which seq meet curSeq at this moment
             */
            if (iter->first < curSeq)
            {
                iter = proposalCache_.erase(iter);
            }
            else
            {
                iter++;
            }
        }
    }
}

void PopConsensus::phaseCollecting()
{
    if (leaderFailed_)
        return;

    auto sinceClose = timeSinceLastClose().count();
    auto sinceOpen = timeSinceOpen();
    auto sinceConsensus = timeSinceConsensus();
    //view change have taken effect
    if (sinceOpen != sinceConsensus)
    {
        sinceOpen = sinceConsensus;
        sinceClose = sinceConsensus;
    }

    JLOG(j_.debug()) << "phaseCollecting time sinceOpen:" << sinceOpen << "ms";

    // Decide if we should propose a tx-set
    if (adaptor_.isLeader(previousLedger_.seq() + 1, view_) && !result_)
    {
        if (!adaptor_.isPoolAvailable())
        {
            return;
        }

        int tx_count = transactions_.size();

        //if time for this block's tx-set reached
        bool bTimeReached = sinceOpen >= adaptor_.parms().maxBLOCK_TIME;
        if (tx_count < adaptor_.parms().maxTXS_IN_LEDGER && !bTimeReached)
        {
            H256Set txs;

            adaptor_.topTransactions(
                adaptor_.parms().maxTXS_IN_LEDGER - tx_count,
                previousLedger_.seq() + 1,
                txs);

            for (auto const& tx : txs)
            {
                transactions_.push_back(tx);
            }
        }

        if (finalCondReached(sinceOpen, sinceClose))
        {
            /**
            1. construct result_
            2. propose position
            3. add position to self
            */

            rawCloseTimes_.self = now_;

            result_.emplace(adaptor_.onCollectFinish(previousLedger_, transactions_, now_, view_, mode_.get()));
            result_->roundTime.reset(clock_.now());
            setID_ = result_->txns.id();
            extraTimeOut_ = true;

            // Share the newly created transaction set if we haven't already
            // received it from a peer
            if (acquired_.emplace(*setID_, result_->txns).second)
                adaptor_.relay(result_->txns);

            adaptor_.propose(result_->position);

            //Omit empty block ,launch view-change
            if (adaptor_.parms().omitEMPTY && *setID_ == beast::zero)
            {
                //set zero,trigger time-out
                consensusTime_ = 0;
                leaderFailed_ = true;
                JLOG(j_.info()) << "Empty transaction-set from self,will trigger view_change.";
                return;
            }

            txSetVoted_[*setID_] = std::set<PublicKey>{ adaptor_.valPublic() };

            phase_ = ConsensusPhase::establish;
            JLOG(j_.info()) << "We are leader,proposing position:" << *setID_;

            checkVoting();
        }
    }
    else
    {
        //in case we are not leader,the proposal leader should propose not received,
        // but other nodes have accepted the ledger of this sequence
        if (adaptor_.proposersFinished(previousLedger_, prevLedgerID_) >= adaptor_.getQuorum())
        {
            //result_.emplace(adaptor_.onCollectFinish(previousLedger_, transactions_, now_,view_, mode_.get()));
            //result_->roundTime.reset(clock_.now());
            //rawCloseTimes_.self = now_;
            //phase_ = ConsensusPhase::establish;
            JLOG(j_.warn()) << "Other nodes have enter establish phase for previous ledger " << previousLedger_.seq();
        }
    }
}

bool PopConsensus::finalCondReached(int64_t sinceOpen, int64_t sinceLastClose)
{
    if (sinceLastClose < 0)
    {
        sinceLastClose = sinceOpen;
    }

    if (sinceLastClose >= adaptor_.parms().maxBLOCK_TIME)
        return true;

    if (adaptor_.parms().maxTXS_IN_LEDGER >= adaptor_.parms().minTXS_IN_LEDGER_ADVANCE &&
        transactions_.size() >= adaptor_.parms().maxTXS_IN_LEDGER &&
        sinceLastClose >= adaptor_.parms().minBLOCK_TIME / 2)
    {
        return true;
    }

    if (transactions_.size() > 0 && sinceLastClose >= adaptor_.parms().minBLOCK_TIME)
        return true;

    return false;
}

void PopConsensus::checkVoting()
{
    ScopedLockType sl(lock_);

    // can only establish consensus if we already took a stance
    assert(result_);

    using namespace std::chrono;
    {
        result_->roundTime.tick(clock_.now());
        //result_->proposers = currPeerPositions_.size();

        JLOG(j_.info()) << "checkVoting roundTime:" << result_->roundTime.read().count();
    }

    // Nothing to do if we don't have consensus.
    if (!haveConsensus())
    {
        return;
    }

    phase_ = ConsensusPhase::accepted;
    adaptor_.onAccept(
        *result_,
        previousLedger_,
        closeResolution_,
        rawCloseTimes_,
        mode_.get(),
        getJson(true));
}

bool PopConsensus::haveConsensus()
{
    // Must have a stance if we are checking for consensus
    if (!result_)
        return false;

    int agreed = txSetVoted_[*setID_].size();
    int minVal = adaptor_.getQuorum();
    auto currentFinished = previousLedger_.seq() == GENESIS_LEDGER_INDEX
        ? 0
        : adaptor_.proposersFinished(previousLedger_, prevLedgerID_);

    JLOG(j_.debug()) << "Checking for TX consensus: agree=" << agreed;
    JLOG(j_.debug()) << "Checking for TX consensus: currentFinished=" << currentFinished;

    // Determine if we actually have consensus or not
    if (agreed >= minVal)
    {
        JLOG(j_.info()) << "Consensus for tx-set reached with agreed = " << agreed;
        result_->state = ConsensusState::Yes;
        return true;
    }
    else if (currentFinished >= minVal)
    {
        result_->state = ConsensusState::MovedOn;
        JLOG(j_.error()) << "Unable to reach consensus";
        JLOG(j_.error()) << getJson(true);
        return true;
    }
    else
    {
        result_->state = ConsensusState::No;
        return false;
    }
}

void PopConsensus::checkTimeout()
{
    if (phase_ == ConsensusPhase::accepted)
        return;

    auto timeOut = extraTimeOut_
        ? adaptor_.parms().consensusTIMEOUT.count() * 1.5
        : adaptor_.parms().consensusTIMEOUT.count();

    if (timeSinceConsensus() < timeOut)
        return;

    if (adaptor_.validating())
        launchViewChange();

    timeOutCount_++;
}

void PopConsensus::launchViewChange()
{
    toView_ = view_ + 1;
    consensusTime_ = utcTime();

    JLOG(j_.info()) << "Check timeout prevLedgerSeq=" << previousLedger_.seq()
        << " PublicKey index=" << adaptor_.getPubIndex()
        << ", sending ViewChange toView=" << toView_;

    auto viewChange = std::make_shared<STViewChange>(
        previousLedger_.seq(),
        prevLedgerID_,
        toView_,
        adaptor_.valPublic(),
        adaptor_.closeTime());

    adaptor_.launchViewChange(*viewChange);

    peerViewChangeInternal(viewChange);
}

void PopConsensus::leaveConsensus()
{
    if (mode_.get() == ConsensusMode::proposing)
    {
        if (result_ && !result_->position.isBowOut())
        {
            result_->position.bowOut(now_);
            adaptor_.propose(result_->position);
        }

        mode_.set(ConsensusMode::observing, adaptor_);
        JLOG(j_.info()) << "Bowing out of consensus";
    }
}

bool PopConsensus::peerProposal(
    std::shared_ptr<PeerImp>& peer,
    bool isTrusted,
    std::shared_ptr<protocol::TMConsensus> const& m)
{
    if (!isTrusted)
    {
        JLOG(j_.warn()) << "drop UNTRUSTED proposal";
        return false;
    }

    try
    {
        STProposeSet::pointer propose;

        SerialIter sit(makeSlice(m->msg()));
        PublicKey const publicKey{ makeSlice(m->signerpubkey()) };

        propose = std::make_shared<STProposeSet>(
            sit,
            adaptor_.closeTime(),
            calcNodeID(publicKey),
            publicKey);

        auto newPeerPos = RCLCxPeerPos(
            publicKey, makeSlice(m->signature()), consensusMessageUniqueId(*m),
            std::move(*propose));

        return peerProposalInternal(adaptor_.closeTime(), newPeerPos);
    }
    catch (std::exception const& e)
    {
        JLOG(j_.warn()) << "ProposeSet: Exception, " << e.what();
        peer->charge(Resource::feeInvalidRequest);
    }

    return false;
}

bool PopConsensus::peerProposalInternal(
    NetClock::time_point const& now,
    PeerPosition_t const& newPeerPos)
{
    // Nothing to do for now if we are currently working on a ledger
    if (phase_ == ConsensusPhase::accepted)
    {
        checkSaveNextProposal(newPeerPos);
        return false;
    }

    Proposal_t const& newPeerProp = newPeerPos.proposal();
    if (newPeerProp.curLedgerSeq() < previousLedger_.seq() + 1)
    {
        // in case the leader is fall behind and proposing previous tx-set...
        return false;
    }

    if (newPeerProp.curLedgerSeq() > previousLedger_.seq() + 1)
    {
        checkSaveNextProposal(newPeerPos);
        // in case the we are fall behind and get proposal from a non-leader node ,but we think it's our leader
        return false;
    }

    if (newPeerProp.prevLedger() != prevLedgerID_)
    {
        checkSaveNextProposal(newPeerPos);
        JLOG(j_.info()) << "Got proposal for " << newPeerProp.prevLedger()
            << " from (PublicKey index)" << adaptor_.getPubIndex(newPeerPos.publicKey())
            << ", but we are on " << prevLedgerID_;
        return false;
    }

    if (newPeerProp.view() != view_)
    {
        checkSaveNextProposal(newPeerPos);
        JLOG(j_.info()) << "Got proposal for " << newPeerProp.prevLedger()
            << " view(" << newPeerProp.view()
            << ") from (PublicKey index)" << adaptor_.getPubIndex(newPeerPos.publicKey())
            << " but we are on view(" << view_ << ")";
        return false;
    }

    now_ = now;

    JLOG(j_.info()) << "Processing peer proposal " << newPeerProp.proposeSeq()
        << "/" << newPeerProp.position();

    {
        PublicKey publicKey = newPeerPos.publicKey();
        auto newSetID = newPeerProp.position();
        auto iter = txSetVoted_.find(newSetID);
        if (iter != txSetVoted_.end())
        {
            JLOG(j_.info()) << "Got proposal for set from (PublicKey index)" << adaptor_.getPubIndex(publicKey);
            iter->second.insert(publicKey);
        }
        else
        {
            if (adaptor_.isLeader(publicKey, previousLedger_.seq() + 1, view_))
            {
                JLOG(j_.info()) << "Got proposal from leader,time since consensus:" << timeSinceConsensus() << "ms.";

                if (adaptor_.parms().omitEMPTY && newSetID == beast::zero)
                {
                    consensusTime_ = 0;
                    leaderFailed_ = true;

                    JLOG(j_.info()) << "Empty proposal from leader,will trigger view_change.";
                    return true;
                }

                txSetVoted_[newSetID] = std::set<PublicKey>{ publicKey };
                setID_ = newSetID;
                rawCloseTimes_.self = now_;
                closeTime_ = newPeerProp.closeTime();
                proposalTime_ = clock_.now();

                if (txSetCached_.find(*setID_) != txSetCached_.end())
                {
                    for (auto publicKey : txSetCached_[*setID_])
                        txSetVoted_[newSetID].insert(publicKey);
                    txSetCached_.erase(*setID_);
                }

                JLOG(j_.info()) << "List current votes for set(" << *setID_
                    << "), total count: " << txSetVoted_[*setID_].size();
                for (auto publicKey : txSetVoted_[*setID_])
                {
                    JLOG(j_.info()) << "PublicKey index: " << adaptor_.getPubIndex(publicKey);
                }

                extraTimeOut_ = true;
                //if (phase_ == ConsensusPhase::open)
                //    phase_ = ConsensusPhase::establish;
            }
            else
            {
                JLOG(j_.info()) << "Got proposal for set from public " << adaptor_.getPubIndex(publicKey) << " and added to cache";
                if (txSetCached_.find(newSetID) != txSetCached_.end())
                {
                    txSetCached_[newSetID].insert(publicKey);
                }
                else
                {
                    txSetCached_[newSetID] = std::set<PublicKey>{ publicKey };
                }
            }
        }
    }

    if (newPeerProp.isInitial())
    {
        ++rawCloseTimes_.peers[newPeerProp.closeTime()];
    }


    {
        auto const ait = acquired_.find(newPeerProp.position());
        if (ait == acquired_.end())
        {
            JLOG(j_.debug()) << "Don't have tx set for peer_position:" << newPeerProp.position();
        }
        // acquireTxSet will return the set if it is available, or
        // spawn a request for it and return none/nullptr.  It will call
        // gotTxSet once it arrives
        if (auto set = adaptor_.acquireTxSet(newPeerProp.position()))
            gotTxSet(now_, *set);
    }

    return true;
}

void PopConsensus::checkSaveNextProposal(PeerPosition_t const& newPeerPos)
{
    Proposal_t const& newPeerProp = newPeerPos.proposal();
    JLOG(j_.info()) << "checkSaveNextProposal, newPeerPos.curSeq=" << newPeerProp.curLedgerSeq() << " newPeerPos.view=" << newPeerProp.view()
        << ", and we are on seq " << previousLedger_.seq() + 1 << " view " << view_;
    if ((newPeerProp.curLedgerSeq() > previousLedger_.seq() + 1) ||
        (newPeerProp.curLedgerSeq() == previousLedger_.seq() + 1 && newPeerProp.view() > view_))
    {
        auto curSeq = newPeerProp.curLedgerSeq();
        if (proposalCache_.find(curSeq) != proposalCache_.end())
        {
            /**
             * Only the first time for the same key will succeed,
             * Or Save the newest proposal from same key, which proposal's view great than the old one,
             * that's a question
             */
            if (proposalCache_[curSeq].find(newPeerPos.publicKey()) != proposalCache_[curSeq].end())
            {
                auto oldPeerProp = proposalCache_[curSeq].find(newPeerPos.publicKey())->second.proposal();
                JLOG(j_.info()) << "peerProposal curSeq=" << curSeq
                    << ", pubKey=" << newPeerPos.publicKey() << " exist, oldView=" << oldPeerProp.view()
                    << ", and this Proposal view=" << newPeerProp.view();
                if (oldPeerProp.view() < newPeerProp.view())
                {
                    proposalCache_[curSeq].erase(newPeerPos.publicKey());
                }
            }
            proposalCache_[curSeq].emplace(newPeerPos.publicKey(), newPeerPos);

            //If other nodes have reached a consensus for a newest ledger,we should acquire that ledger.
            int count = 0;
            for (auto iter : proposalCache_[curSeq])
            {
                if (iter.second.proposal().view() == view_)
                {
                    count++;
                }
            }
            if (count >= adaptor_.getQuorum())
            {
                adaptor_.acquireLedger(newPeerProp.prevLedger());
            }
        }
        else
        {
            std::map<PublicKey, RCLCxPeerPos> mapPos;
            mapPos.emplace(newPeerPos.publicKey(), newPeerPos);
            proposalCache_[curSeq] = mapPos;
        }

        JLOG(j_.info()) << "Position " << newPeerProp.position()
            << " of ledger " << newPeerProp.curLedgerSeq()
            << " from (PublicKey index)" << adaptor_.getPubIndex(newPeerPos.publicKey())
            << " added to cache.";
    }
}

bool PopConsensus::peerViewChange(
    std::shared_ptr<PeerImp>& peer,
    bool isTrusted,
    std::shared_ptr<protocol::TMConsensus> const& m)
{
    if (!isTrusted)
    {
        JLOG(j_.warn()) << "drop UNTRUSTED ViewChange";
        return false;
    }

    try
    {
        STViewChange::pointer viewChange;

        SerialIter sit(makeSlice(m->msg()));
        PublicKey const publicKey{ makeSlice(m->signerpubkey()) };

        viewChange = std::make_shared<STViewChange>(
            sit,
            publicKey);

        return peerViewChangeInternal(viewChange);
    }
    catch (std::exception const& e)
    {
        JLOG(j_.warn()) << "ViewChange: Exception, " << e.what();
        peer->charge(Resource::feeInvalidRequest);
    }

    return false;
}

bool PopConsensus::peerViewChangeInternal(STViewChange::ref viewChange)
{
    JLOG(j_.info()) << "Processing peer ViewChange toView=" << viewChange->toView()
        << ", PublicKey index=" << adaptor_.getPubIndex(viewChange->nodePublic());

    bool saved = viewChangeManager_.recvViewChange(viewChange);
    if (saved)
    {
        JLOG(j_.info()) << "ViewChange saved, current count of this view is "
            << viewChangeManager_.viewCount(viewChange->toView());

        checkChangeView(viewChange->toView());

        if (waitingForInit() && viewChange->prevSeq() > GENESIS_LEDGER_INDEX)
        {
            prevLedgerID_ = viewChange->prevHash();
            view_ = viewChange->toView() - 1;
            checkLedger();
        }
    }
    else if (previousLedger_.seq() == GENESIS_LEDGER_INDEX && viewChange->prevSeq() > GENESIS_LEDGER_INDEX)
    {
        JLOG(j_.warn()) << "touch inboundLedger for " << viewChange->prevHash();
        adaptor_.touchAcquringLedger(viewChange->prevHash());
    }

    return true;
}

void PopConsensus::checkChangeView(uint64_t toView)
{
    if (phase_ == ConsensusPhase::accepted)
    {
        return;
    }

    if (toView_ == toView)
    {
        if (viewChangeManager_.haveConsensus(toView, view_, prevLedgerID_, adaptor_.getQuorum()))
        {
            onViewChange(toView);
        }
    }
    else
    {
        /* If number of toView can met ,we need to check:
           1. if previousLedgerSeq < ViewChange.previousLedgerSeq ?handleWrong ledger
           2. if previousLedgerSeq == ViewChange.previousLedgerSeq ?change our view_ to new view.
        */
        auto ret = viewChangeManager_.shouldTriggerViewChange(toView, previousLedger_, adaptor_.getQuorum());
        if (std::get<0>(ret))
        {
            if (previousLedger_.seq() < std::get<1>(ret))
            {
                JLOG(j_.info()) << "View changed fulfilled in other nodes: " << toView << ", and we need ledger:" << std::get<1>(ret);
                handleWrongLedger(std::get<2>(ret));
            }
            else if (previousLedger_.seq() == std::get<1>(ret))
            {
                JLOG(j_.info()) << "We have the newest ledger, change view to " << view_;
                onViewChange(toView);
            }
        }
    }
}

void PopConsensus::onViewChange(uint64_t toView)
{
    ScopedLockType sl(lock_);

    JLOG(j_.info()) << "View change to " << toView;

    view_ = toView;
    consensusTime_ = utcTime();
    phase_ = ConsensusPhase::open;
    result_.reset();
    acquired_.clear();
    rawCloseTimes_.peers.clear();
    rawCloseTimes_.self = {};
    txSetCached_.clear();
    txSetVoted_.clear();
    transactions_.clear();
    setID_.reset();
    leaderFailed_ = false;
    extraTimeOut_ = false;
    timeOutCount_ = 0;

    //clear avoid
    adaptor_.clearPoolAvoid(previousLedger_.seq());

    viewChangeManager_.onViewChanged(view_);
    if (bWaitingInit_)
    {
        if (mode_.get() != ConsensusMode::wrongLedger)
        {
            adaptor_.onViewChanged(bWaitingInit_, previousLedger_);
            bWaitingInit_ = false;
        }
    }
    else
    {
        adaptor_.onViewChanged(bWaitingInit_, previousLedger_);
    }

    if (mode_.get() == ConsensusMode::proposing)
    {
        checkCache();
    }
}

bool PopConsensus::peerValidation(
    std::shared_ptr<PeerImp>& peer,
    bool isTrusted,
    std::shared_ptr<protocol::TMConsensus> const& m)
{
    if (m->msg().size() < 50)
    {
        JLOG(j_.warn()) << "Validation: Too small";
        peer->charge(Resource::feeInvalidRequest);
        return false;
    }

    try
    {
        STValidation::pointer val;
        {
            SerialIter sit(makeSlice(m->msg()));
            PublicKey const publicKey{ makeSlice(m->signerpubkey()) };
            val = std::make_shared<STValidation>(
                std::ref(sit),
                publicKey,
                [&](PublicKey const& pk) {
                    return calcNodeID(adaptor_.getMasterKey(pk));
                });
            val->setSeen(adaptor_.closeTime());
            if (isTrusted)
            {
                val->setTrusted();
            }
        }

        return adaptor_.peerValidation(peer, val);
    }
    catch (std::exception const& e)
    {
        JLOG(j_.warn()) << "Validation: Exception, " << e.what();
        peer->charge(Resource::feeInvalidRequest);
    }

    return false;
}


}