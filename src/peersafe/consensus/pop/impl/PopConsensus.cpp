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
    : ConsensusBase(clock_, journal)
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
            adaptor_.app_.getTxPool().removeTxs(
                newLedger->ledger_->txMap(),
                newLedger->ledger_->info().seq,
                newLedger->ledger_->info().parentHash);
            startRoundInternal(
                now_, prevLedgerID_, *newLedger, ConsensusMode::switchedLedger);
        }
        return;
    }

    //Long time no consensus reach,rollback to initial state.
    //What if 2 of 4 validate new ledger success, but other 2 of 4 not ,can roll back work,or is there such occasion?
    if (timeOutCount_ > adaptor_.parms().timeoutCOUNT_ROLLBACK)
    {
        auto valLedger = adaptor_.ledgerMaster_.getValidLedgerIndex();
        if (view_ > 0 || previousLedger_.seq() > valLedger)
        {
            JLOG(j_.warn()) << "There have been " << adaptor_.parms().timeoutCOUNT_ROLLBACK
                << " times of timeout, will rollback to validated ledger " << valLedger;
            if (auto oldLedger = adaptor_.ledgerMaster_.getValidatedLedger())
            {
                startRoundInternal(
                    now_, oldLedger->info().hash, oldLedger, ConsensusMode::switchedLedger);
                //Clear view-change cache after initial state.
                viewChangeManager_.clearCache();
                //Clear validation cache,in case "checkLedger move back to advanced ledger".
                adaptor_.app_.getValidations().flush();
            }
        }
    }

    if (phase_ == ConsensusPhase::open)
    {
        phaseCollecting();
    }

    checkTimeout();
}

bool PopConsensus::peerProposal(
    NetClock::time_point const& now,
    PeerPosition_t const& newPeerPos)
{
    return peerProposalInternal(now, newPeerPos);
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

    //check to see if final condition reached.
    if (result_)
    {
        checkVoting();
        return;
    }

    if (!isLeader(adaptor_.valPublic_))
    {
        //update avoid if we got the right tx-set
        if (adaptor_.validating())
            adaptor_.app_.getTxPool().updateAvoid(txSet, previousLedger_.seq());

        auto set = txSet.map_->snapShot(false);
        //this place has a txSet copy,what's the time it costs?
        result_.emplace(Result(
            std::move(set),
            RCLCxPeerPos::Proposal(
                prevLedgerID_,
                previousLedger_.seq() + 1,
                view_,
                RCLCxPeerPos::Proposal::seqJoin,
                id,
                closeTime_,
                now,
                adaptor_.nodeID())));

        if (phase_ == ConsensusPhase::open)
            phase_ = ConsensusPhase::establish;

        JLOG(j_.info()) << "gotTxSet time elapsed since receive set_id from leader:" << (now - rawCloseTimes_.self).count();

        if (adaptor_.validating())
        {
            txSetVoted_[*setID_].insert(adaptor_.valPublic_);
            adaptor_.propose(result_->position);

            JLOG(j_.info()) << "voting for set:" << *setID_ << " " << txSetVoted_[*setID_].size();
        }

        result_->roundTime.reset(proposalTime_);

        for (auto pub : txSetVoted_[*setID_])
        {
            JLOG(j_.info()) << "voting node for set:" << getPubIndex(pub);
        }
        JLOG(j_.info()) << "We are not leader gotTxSet,and proposing position:" << id;
    }
}

Json::Value PopConsensus::getJson(bool full) const
{
    using std::to_string;
    using Int = Json::Value::Int;

    Json::Value ret(Json::objectValue);

    ret["proposing"] = (mode_.get() == ConsensusMode::proposing);
    ret["proposers"] = static_cast<int>(txSetVoted_.size());

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
        ret["transaction_count"] = static_cast<int>(transactions_.size());
    }

    ret["tx_count_in_pool"] = static_cast<int>(adaptor_.app_.getTxPool().getTxCountInPool());

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

bool PopConsensus::peerViewChange(ViewChange const& change)
{
    bool ret = viewChangeManager_.recvViewChange(change);
    if (ret)
    {
        JLOG(j_.info()) << "peerViewChange viewChangeReq saved,toView=" <<
            change.toView() << ",nodeid=" << getPubIndex(change.nodePublic());
        checkChangeView(change.toView());
        if (waitingForInit() && change.prevSeq() > GENESIS_LEDGER_INDEX)
        {
            prevLedgerID_ = change.prevHash();
            view_ = change.toView() - 1;
            checkLedger();
        }
    }
    else if (previousLedger_.seq() == GENESIS_LEDGER_INDEX && change.prevSeq() > GENESIS_LEDGER_INDEX)
    {
        JLOG(j_.warn()) << "touch inboundLedger for " << change.prevHash();
        adaptor_.touchAcquringLedger(change.prevHash());
    }

    return true;
}

bool PopConsensus::waitingForInit()
{
    // This code is for initialization,wait 60 seconds for loading ledger before real start-mode.
    if (previousLedger_.seq() == GENESIS_LEDGER_INDEX &&
        timeSinceOpen() / 1000 < adaptor_.parms().initTIME.count())
    {
        return true;
    }
    return false;
}

// -------------------------------------------------------------------
// Private member functions

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
    if (previousLedger_.seq() == GENESIS_LEDGER_INDEX)
        return;

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
        adaptor_.app_.getTxPool().removeTxs(
            newLedger->ledger_->txMap(),
            newLedger->ledger_->info().seq,
            newLedger->ledger_->info().parentHash);
        startRoundInternal(
            now_, lgrId, *newLedger, ConsensusMode::switchedLedger);
    }
    else
    {
        mode_.set(ConsensusMode::wrongLedger, adaptor_);
    }
}

void PopConsensus::checkCache()
{
    std::uint32_t curSeq = previousLedger_.seq() + 1;
    if (adaptor_.proposalCache_.find(curSeq) != adaptor_.proposalCache_.end())
    {
        JLOG(j_.info()) << "Check peerProposalInternal after startRoundInternal";
        for (auto it = adaptor_.proposalCache_[curSeq].begin(); it != adaptor_.proposalCache_[curSeq].end(); it++)
        {
            if (peerProposalInternal(now_, it->second))
            {
                JLOG(j_.info()) << "Position " << it->second.proposal().position() << " from " << getPubIndex(it->first) << " success";
            }
        }
        auto iter = adaptor_.proposalCache_.begin();
        while (iter != adaptor_.proposalCache_.end())
        {
            /**
             * Maybe prosoal seq meet curSeq, but view is feture,
             * so don't remove propal which seq meet curSeq at this moment
             */
            if (iter->first < curSeq)
            {
                iter = adaptor_.proposalCache_.erase(iter);
            }
            else
            {
                iter++;
            }
        }
    }
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
            << " from node " << getPubIndex(newPeerPos.publicKey()) << " but we are on " << prevLedgerID_;
        return false;
    }

    if (newPeerProp.view() != view_)
    {
        checkSaveNextProposal(newPeerPos);
        JLOG(j_.info()) << "Got proposal for " << newPeerProp.prevLedger() << " view=" << newPeerProp.view()
            << " from node " << getPubIndex(newPeerPos.publicKey()) << " but we are on view_ " << view_;
        return false;
    }

    now_ = now;

    JLOG(j_.info()) << "Processing peer proposal " << newPeerProp.proposeSeq()
        << "/" << newPeerProp.position();

    {
        PublicKey pub = newPeerPos.publicKey();
        auto newSetID = newPeerProp.position();
        auto iter = txSetVoted_.find(newSetID);
        if (iter != txSetVoted_.end())
        {
            JLOG(j_.info()) << "Got proposal for set from public :" << getPubIndex(pub);
            iter->second.insert(pub);
        }
        else
        {
            if (isLeader(pub))
            {
                JLOG(j_.info()) << "Got proposal from leader,time since consensus:" << timeSinceConsensus() << "ms.";

                if (adaptor_.parms().omitEMPTY && newSetID == beast::zero)
                {
                    consensusTime_ = 0;
                    leaderFailed_ = true;

                    JLOG(j_.info()) << "Empty proposal from leader,will trigger view_change.";
                    return true;
                }

                txSetVoted_[newSetID] = std::set<PublicKey>{ pub };
                setID_ = newSetID;
                rawCloseTimes_.self = now_;
                closeTime_ = newPeerProp.closeTime();
                proposalTime_ = clock_.now();

                if (txSetCached_.find(*setID_) != txSetCached_.end())
                {
                    for (auto pub : txSetCached_[*setID_])
                        txSetVoted_[newSetID].insert(pub);
                    txSetCached_.erase(*setID_);
                }

                JLOG(j_.info()) << "voting for set:" << *setID_ << " " << txSetVoted_[*setID_].size();
                for (auto pub : txSetVoted_[*setID_])
                {
                    JLOG(j_.info()) << "voting public for set:" << getPubIndex(pub);
                }

                extraTimeOut_ = true;
                //if (phase_ == ConsensusPhase::open)
                //    phase_ = ConsensusPhase::establish;
            }
            else
            {
                JLOG(j_.info()) << "Got proposal for set from public " << getPubIndex(pub) << " and added to cache";
                if (txSetCached_.find(newSetID) != txSetCached_.end())
                {
                    txSetCached_[newSetID].insert(pub);
                }
                else
                {
                    txSetCached_[newSetID] = std::set<PublicKey>{ pub };
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
    if (shouldPack() && !result_)
    {
        if (!adaptor_.app_.getTxPool().isAvailable())
        {
            return;
        }

        int tx_count = transactions_.size();

        //if time for this block's tx-set reached
        bool bTimeReached = sinceOpen >= adaptor_.parms().maxBLOCK_TIME;
        if (tx_count < adaptor_.parms().maxTXS_IN_LEDGER && !bTimeReached)
        {
            appendTransactions(
                adaptor_.app_.getTxPool().topTransactions(
                    adaptor_.parms().maxTXS_IN_LEDGER - tx_count,
                    previousLedger_.seq() + 1));
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

            txSetVoted_[*setID_] = std::set<PublicKey>{ adaptor_.valPublic_ };

            phase_ = ConsensusPhase::establish;
            JLOG(j_.info()) << "We are leader,proposing position:" << *setID_;

            checkVoting();
        }
    }
    else
    {
        //in case we are not leader,the proposal leader should propose not received,
        // but other nodes have accepted the ledger of this sequence
        int minVal = adaptor_.app_.validators().quorum();
        auto currentFinished = adaptor_.proposersFinished(previousLedger_, prevLedgerID_);
        if (currentFinished >= minVal)
        {
            //result_.emplace(adaptor_.onCollectFinish(previousLedger_, transactions_, now_,view_, mode_.get()));
            //result_->roundTime.reset(clock_.now());
            //rawCloseTimes_.self = now_;
            //phase_ = ConsensusPhase::establish;
            JLOG(j_.warn()) << "Other nodes have enter establish phase for previous ledger " << previousLedger_.seq();
        }
    }
}

bool PopConsensus::checkChangeView(uint64_t toView)
{
    if (phase_ == ConsensusPhase::accepted)
    {
        return false;
    }
    if (toView_ == toView)
    {
        if (viewChangeManager_.checkChange(toView, view_, prevLedgerID_, adaptor_.app_.validators().quorum()))
        {
            view_ = toView;
            onViewChange();
            JLOG(j_.info()) << "View changed to " << view_;
            return true;
        }
    }
    else
    {
        /* If number of toView can met ,we need to check:
        1. if previousLedgerSeq < ViewChange.previousLedgerSeq ?handleWrong ledger
        2. if previousLedgerSeq == ViewChange.previousLedgerSeq ?change our view_ to new view.
        */
        auto ret = viewChangeManager_.shouldTriggerViewChange(toView, previousLedger_, adaptor_.app_.validators().quorum());
        if (std::get<0>(ret))
        {
            if (previousLedger_.seq() < std::get<1>(ret))
            {
                handleWrongLedger(std::get<2>(ret));
                JLOG(j_.info()) << "View changed fulfilled in other nodes: " << toView << ",and we need ledger:" << std::get<1>(ret);
            }
            else if (previousLedger_.seq() == std::get<1>(ret))
            {
                view_ = toView;
                onViewChange();
                JLOG(j_.info()) << "We have the newest ledger,change view to " << view_;
                return true;
            }
        }
    }

    return false;
}

void PopConsensus::onViewChange()
{
    ScopedLockType sl(lock_);

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
    adaptor_.app_.getTxPool().clearAvoid(previousLedger_.seq());

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

void PopConsensus::launchViewChange()
{
    toView_ = view_ + 1;
    consensusTime_ = utcTime();

    ViewChange change(previousLedger_.seq(), prevLedgerID_, adaptor_.valPublic_, toView_);
    adaptor_.sendViewChange(change);

    JLOG(j_.info()) << "checkTimeout sendViewChange,toView=" << toView_ << ",nodeId=" << getPubIndex(adaptor_.valPublic_)
        << ",prevLedgerSeq=" << previousLedger_.seq();

    peerViewChange(change);
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

bool PopConsensus::haveConsensus()
{
    // Must have a stance if we are checking for consensus
    if (!result_)
        return false;

    int agreed = txSetVoted_[*setID_].size();
    int minVal = adaptor_.app_.validators().quorum();
    auto currentFinished = previousLedger_.seq() == GENESIS_LEDGER_INDEX ? 0 :
        adaptor_.proposersFinished(previousLedger_, prevLedgerID_);

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

    //if (result_->state == ConsensusState::No)
    //	return false;

    //// There is consensus, but we need to track if the network moved on
    //// without us.
    //if (result_->state == ConsensusState::MovedOn)
    //{
    //	JLOG(j_.error()) << "Unable to reach consensus";
    //	JLOG(j_.error()) << getJson(true);
    //}

    //return true;
}

bool PopConsensus::shouldPack()
{
    return isLeader(adaptor_.valPublic_);
}

bool PopConsensus::isLeader(PublicKey const& pub, bool bNextLeader /* = false */)
{
    auto const& validators = adaptor_.app_.validators().validators();
    LedgerIndex currentLedgerIndex = previousLedger_.seq() + 1;
    if (bNextLeader)
    {
        currentLedgerIndex++;
    }
    assert(validators.size() > 0);
    int leader_idx = (view_ + currentLedgerIndex) % validators.size();
    return pub == validators[leader_idx];
}

int PopConsensus::getPubIndex(PublicKey const& pub)
{
    auto const& validators = adaptor_.app_.validators().validators();
    for (int i = 0; i < validators.size(); i++)
    {
        if (validators[i] == pub)
            return i + 1;
    }
    return 0;
}

/** Is final condition reached for proposing.
    We should check:
    1. Is maxBlockTime reached.
    2. Is tx-count reached max and max >=5000 and minBlockTime/2 reached.(There will be
       a time to reach tx-set consensus)
    3. If there are txs but not reach max-count,is the minBlockTime reached.
*/
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

void PopConsensus::appendTransactions(h256Set const& txSet)
{
    for (auto const& trans : txSet)
        transactions_.push_back(trans);
}

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

void PopConsensus::checkSaveNextProposal(PeerPosition_t const& newPeerPos)
{
    Proposal_t const& newPeerProp = newPeerPos.proposal();
    JLOG(j_.info()) << "checkSaveNextProposal, newPeerPos.curSeq=" << newPeerProp.curLedgerSeq() << " newPeerPos.view=" << newPeerProp.view()
        << ", and we are on seq " << previousLedger_.seq() + 1 << " view " << view_;
    if ((newPeerProp.curLedgerSeq() > previousLedger_.seq() + 1) ||
        (newPeerProp.curLedgerSeq() == previousLedger_.seq() + 1 && newPeerProp.view() > view_))
    {
        auto curSeq = newPeerProp.curLedgerSeq();
        if (adaptor_.proposalCache_.find(curSeq) != adaptor_.proposalCache_.end())
        {
            /**
             * Only the first time for the same key will succeed,
             * Or Save the newest proposal from same key, which proposal's view great than the old one,
             * that's a question
             */
#if 1
            if (adaptor_.proposalCache_[curSeq].find(newPeerPos.publicKey()) != adaptor_.proposalCache_[curSeq].end())
            {
                auto oldPeerProp = adaptor_.proposalCache_[curSeq].find(newPeerPos.publicKey())->second.proposal();
                JLOG(j_.info()) << "peerProposal curSeq=" << curSeq
                    << ", pubKey=" << newPeerPos.publicKey() << " exist, oldView=" << oldPeerProp.view()
                    << ", and this Proposal view=" << newPeerProp.view();
                if (oldPeerProp.view() < newPeerProp.view())
                {
                    adaptor_.proposalCache_[curSeq].erase(newPeerPos.publicKey());
                }
            }
#endif
            adaptor_.proposalCache_[curSeq].emplace(newPeerPos.publicKey(), newPeerPos);

            //If other nodes have reached a consensus for a newest ledger,we should acquire that ledger.
            int count = 0;
            for (auto iter : adaptor_.proposalCache_[curSeq])
            {
                if (iter.second.proposal().view() == view_)
                {
                    count++;
                }
            }
            if (count >= adaptor_.app_.validators().quorum())
            {
                adaptor_.acquireLedger(newPeerProp.prevLedger());
            }
        }
        else
        {
            std::map<PublicKey, RCLCxPeerPos> mapPos;
            mapPos.emplace(newPeerPos.publicKey(), newPeerPos);
            adaptor_.proposalCache_[curSeq] = mapPos;
        }

        JLOG(j_.info()) << "Position " << newPeerProp.position() << " of ledger " << newPeerProp.curLedgerSeq() << " from " << getPubIndex(newPeerPos.publicKey()) << " added to cache.";
    }
}


}