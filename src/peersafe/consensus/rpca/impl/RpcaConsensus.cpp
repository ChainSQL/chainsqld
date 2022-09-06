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
#include <peersafe/consensus/rpca/RpcaConsensus.h>


namespace ripple {

bool
shouldCloseLedger(
    bool anyTransactions,
    std::size_t prevProposers,
    std::size_t proposersClosed,
    std::size_t proposersValidated,
    std::chrono::milliseconds prevRoundTime,
    std::chrono::milliseconds
        timeSincePrevClose,              // Time since last ledger's close time
    std::chrono::milliseconds openTime,  // Time waiting to close this ledger
    std::chrono::milliseconds idleInterval,
    RpcaConsensusParms const& parms,
    beast::Journal j)
{
    using namespace std::chrono_literals;
    if ((prevRoundTime < -1s) || (prevRoundTime > 10min) ||
        (timeSincePrevClose > 10min))
    {
        // These are unexpected cases, we just close the ledger
        JLOG(j.warn()) << "shouldCloseLedger Trans="
                       << (anyTransactions ? "yes" : "no")
                       << " Prop: " << prevProposers << "/" << proposersClosed
                       << " Secs: " << timeSincePrevClose.count()
                       << " (last: " << prevRoundTime.count() << ")";
        return true;
    }

    if ((proposersClosed + proposersValidated) > (prevProposers / 2))
    {
        // If more than half of the network has closed, we close
        JLOG(j.trace()) << "Others have closed";
        return true;
    }

    if (!anyTransactions)
    {
        // Only close at the end of the idle interval
        return timeSincePrevClose >= idleInterval;  // normal idle
    }

    // Preserve minimum ledger open time
    if (openTime < parms.ledgerMIN_CLOSE)
    {
        JLOG(j.debug()) << "Must wait minimum time before closing";
        return false;
    }

    // Don't let this ledger close more than twice as fast as the previous
    // ledger reached consensus so that slower validators can slow down
    // the network
    if (openTime < (prevRoundTime / 2))
    {
        JLOG(j.debug()) << "Ledger has not been open long enough";
        return false;
    }

    // Close the ledger
    return true;
}

bool
checkConsensusReached(
    std::size_t agreeing,
    std::size_t total,
    bool count_self,
    std::size_t minConsensusPct)
{
    // If we are alone, we have a consensus
    if (total == 0)
        return true;

    if (count_self)
    {
        ++agreeing;
        ++total;
    }

    std::size_t currentPercentage = (agreeing * 100) / total;

    return currentPercentage >= minConsensusPct;
}

ConsensusState
checkConsensus(
    std::size_t prevProposers,
    std::size_t currentProposers,
    std::size_t currentAgree,
    std::size_t currentFinished,
    std::chrono::milliseconds previousAgreeTime,
    std::chrono::milliseconds currentAgreeTime,
    RpcaConsensusParms const& parms,
    bool proposing,
    beast::Journal j)
{
    JLOG(j.trace()) << "checkConsensus: prop=" << currentProposers << "/"
                    << prevProposers << " agree=" << currentAgree
                    << " validated=" << currentFinished
                    << " time=" << currentAgreeTime.count() << "/"
                    << previousAgreeTime.count();

    if (currentAgreeTime <= parms.ledgerMIN_CONSENSUS)
        return ConsensusState::No;

    if (currentProposers < (prevProposers * 3 / 4))
    {
        // Less than 3/4 of the last ledger's proposers are present; don't
        // rush: we may need more time.
        if (currentAgreeTime < (previousAgreeTime + parms.ledgerMIN_CONSENSUS))
        {
            JLOG(j.trace()) << "too fast, not enough proposers";
            return ConsensusState::No;
        }
    }

    // Have we, together with the nodes on our UNL list, reached the threshold
    // to declare consensus?
    if (checkConsensusReached(
            currentAgree, currentProposers, proposing, parms.minCONSENSUS_PCT))
    {
        JLOG(j.debug()) << "normal consensus";
        return ConsensusState::Yes;
    }

    // Have sufficient nodes on our UNL list moved on and reached the threshold
    // to declare consensus?
    if (checkConsensusReached(
            currentFinished, currentProposers, false, parms.minCONSENSUS_PCT))
    {
        JLOG(j.warn()) << "We see no consensus, but 80% of nodes have moved on";
        return ConsensusState::MovedOn;
    }

    // no consensus yet
    JLOG(j.trace()) << "no consensus";
    return ConsensusState::No;
}

// -------------------------------------------------------------------
// Public member functions

RpcaConsensus::RpcaConsensus(
    Adaptor& adaptor,
    clock_type const& clock,
    beast::Journal journal)
    : ConsensusBase(clock, journal), adaptor_(*(RpcaAdaptor*)(&adaptor))
{
    JLOG(j_.info()) << "Creating RPCA consensus object";
}

void
RpcaConsensus::startRound(
    NetClock::time_point const& now,
    typename Ledger_t::ID const& prevLedgerID,
    Ledger_t prevLedger,
    hash_set<NodeID> const& nowUntrusted,
    bool proposing)
{
    if (firstRound_)
    {
        // take our initial view of closeTime_ from the seed ledger
        prevRoundTime_ = adaptor_.parms().ledgerIDLE_INTERVAL;
        prevCloseTime_ = prevLedger.closeTime();
        firstRound_ = false;
    }
    else
    {
        prevCloseTime_ = rawCloseTimes_.self;
    }

    for (NodeID_t const& n : nowUntrusted)
        recentPeerPositions_.erase(n);

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

void
RpcaConsensus::timerEntry(NetClock::time_point const& now)
{
    // Nothing to do if we are currently working on a ledger
    if (phase_ == ConsensusPhase::accepted)
        return;

    now_ = now;

    // Check we are on the proper ledger (this may change phase_)
    checkLedger();

    if (phase_ == ConsensusPhase::open)
    {
        phaseOpen();
    }
    else if (phase_ == ConsensusPhase::establish)
    {
        phaseEstablish();
    }
}

bool
RpcaConsensus::peerConsensusMessage(
    std::shared_ptr<PeerImp>& peer,
    bool isTrusted,
    std::shared_ptr<protocol::TMConsensus> const& m)
{
    switch (m->msgtype())
    {
        case ConsensusMessageType::mtPROPOSESET:
            return peerProposal(peer, isTrusted, m);
        case ConsensusMessageType::mtVALIDATION:
            return peerValidation(peer, isTrusted, m);
        default:
            break;
    }

    return false;
}

void
RpcaConsensus::gotTxSet(NetClock::time_point const& now, TxSet_t const& txSet)
{
    // Nothing to do if we've finished work on a ledger
    if (phase_ == ConsensusPhase::accepted)
        return;

    now_ = now;

    auto id = txSet.id();

    // If we've already processed this transaction set since requesting
    // it from the network, there is nothing to do now
    if (!acquired_.emplace(id, txSet).second)
        return;

    if (!result_)
    {
        JLOG(j_.debug()) << "Not creating disputes: no position yet.";
    }
    else
    {
        // Our position is added to acquired_ as soon as we create it,
        // so this txSet must differ
        assert(id != result_->position.position());
        bool any = false;
        for (auto const& [nodeId, peerPos] : currPeerPositions_)
        {
            if (peerPos.proposal().position() == id)
            {
                updateDisputes(nodeId, txSet);
                any = true;
            }
        }

        if (!any)
        {
            JLOG(j_.warn())
                << "By the time we got " << id << " no peers were proposing it";
        }
    }
}

Json::Value
RpcaConsensus::getJson(bool full) const
{
    using std::to_string;
    using Int = Json::Value::Int;

    Json::Value ret(Json::objectValue);

    ret["type"] = "rpca";

    ret["proposing"] = (mode_.get() == ConsensusMode::proposing);
    ret["proposers"] = static_cast<int>(currPeerPositions_.size());

    if (mode_.get() != ConsensusMode::wrongLedger)
    {
        ret["synched"] = true;
        ret["ledger_seq"] =
            static_cast<std::uint32_t>(previousLedger_.seq()) + 1;
        ret["close_granularity"] = static_cast<Int>(closeResolution_.count());
    }
    else
        ret["synched"] = false;

    ret["phase"] = to_string(phase_);

    if (result_ && !result_->disputes.empty() && !full)
        ret["disputes"] = static_cast<Int>(result_->disputes.size());

    if (result_)
        ret["our_position"] = result_->position.getJson();

    if (full)
    {
        if (result_)
            ret["current_ms"] =
                static_cast<Int>(result_->roundTime.read().count());
        ret["converge_percent"] = convergePercent_;
        ret["close_resolution"] = static_cast<Int>(closeResolution_.count());
        ret["have_time_consensus"] = haveCloseTimeConsensus_;
        ret["previous_proposers"] = static_cast<Int>(prevProposers_);
        ret["previous_mseconds"] = static_cast<Int>(prevRoundTime_.count());

        if (!currPeerPositions_.empty())
        {
            Json::Value ppj(Json::objectValue);

            for (auto const& [nodeId, peerPos] : currPeerPositions_)
            {
                ppj[to_string(nodeId)] = peerPos.getJson();
            }
            ret["peer_positions"] = std::move(ppj);
        }

        if (!acquired_.empty())
        {
            Json::Value acq(Json::arrayValue);
            for (auto const& at : acquired_)
            {
                acq.append(to_string(at.first));
            }
            ret["acquired"] = std::move(acq);
        }

        if (result_ && !result_->disputes.empty())
        {
            Json::Value dsj(Json::objectValue);
            for (auto const& [txId, dispute] : result_->disputes)
            {
                dsj[to_string(txId)] = dispute.getJson();
            }
            ret["disputes"] = std::move(dsj);
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

        if (!deadNodes_.empty())
        {
            Json::Value dnj(Json::arrayValue);
            for (auto const& dn : deadNodes_)
            {
                dnj.append(to_string(dn));
            }
            ret["dead_nodes"] = std::move(dnj);
        }
    }

    return ret;
}

std::chrono::milliseconds 
RpcaConsensus::getConsensusTimeOut() const
{
    return std::chrono::milliseconds{0};
}

void
RpcaConsensus::simulate(
    NetClock::time_point const& now,
    boost::optional<std::chrono::milliseconds> consensusDelay)
{
    using namespace std::chrono_literals;
    JLOG(j_.info()) << "Simulating consensus";
    now_ = now;
    closeLedger();
    result_->roundTime.tick(consensusDelay.value_or(100ms));
    result_->proposers = prevProposers_ = currPeerPositions_.size();
    prevRoundTime_ = result_->roundTime.read();
    phase_ = ConsensusPhase::accepted;
    adaptor_.onForceAccept(
        *result_,
        previousLedger_,
        closeResolution_,
        rawCloseTimes_,
        mode_.get(),
        getJson(true));
    JLOG(j_.info()) << "Simulation complete";
}

// -------------------------------------------------------------------
// Private member functions

void
RpcaConsensus::startRoundInternal(
    NetClock::time_point const& now,
    typename Ledger_t::ID const& prevLedgerID,
    Ledger_t const& prevLedger,
    ConsensusMode mode)
{
    phase_ = ConsensusPhase::open;
    mode_.set(mode, adaptor_);
    now_ = now;
    prevLedgerID_ = prevLedgerID;
    previousLedger_ = prevLedger;
    result_.reset();
    convergePercent_ = 0;
    haveCloseTimeConsensus_ = false;
    openTime_.reset(clock_.now());
    currPeerPositions_.clear();
    acquired_.clear();
    rawCloseTimes_.peers.clear();
    rawCloseTimes_.self = {};
    deadNodes_.clear();

    closeResolution_ = getNextLedgerTimeResolution(
        previousLedger_.closeTimeResolution(),
        previousLedger_.closeAgree(),
        previousLedger_.seq() + typename Ledger_t::Seq{1});

    playbackProposals();
    if (currPeerPositions_.size() > (prevProposers_ / 2))
    {
        // We may be falling behind, don't wait for the timer
        // consider closing the ledger immediately
        timerEntry(now_);
    }
}

void
RpcaConsensus::playbackProposals()
{
    for (auto const& it : recentPeerPositions_)
    {
        for (auto const& pos : it.second)
        {
            if (pos.proposal().prevLedger() == prevLedgerID_)
            {
                if (peerProposalInternal(now_, pos))
                    adaptor_.share(pos);
            }
        }
    }
}

void
RpcaConsensus::checkLedger()
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
        JLOG(j_.warn()) << Json::Compact{previousLedger_.getJson()};
        JLOG(j_.debug()) << "State on consensus change "
                         << Json::Compact{getJson(true)};
        handleWrongLedger(netLgr);
    }
    else if (previousLedger_.id() != prevLedgerID_)
        handleWrongLedger(netLgr);
}

// Handle a change in the prior ledger during a consensus round
void
RpcaConsensus::handleWrongLedger(typename Ledger_t::ID const& lgrId)
{
    assert(lgrId != prevLedgerID_ || previousLedger_.id() != lgrId);

    // Stop proposing because we are out of sync
    leaveConsensus();

    // First time switching to this ledger
    if (prevLedgerID_ != lgrId)
    {
        prevLedgerID_ = lgrId;

        // Clear out state
        if (result_)
        {
            result_->disputes.clear();
            result_->compares.clear();
        }

        currPeerPositions_.clear();
        rawCloseTimes_.peers.clear();
        deadNodes_.clear();

        // Get back in sync, this will also recreate disputes
        playbackProposals();
    }

    if (previousLedger_.id() == prevLedgerID_)
        return;

    // we need to switch the ledger we're working from
    if (auto newLedger = adaptor_.acquireLedger(prevLedgerID_))
    {
        JLOG(j_.info()) << "Have the consensus ledger " << prevLedgerID_;
        startRoundInternal(
            now_, lgrId, *newLedger, ConsensusMode::switchedLedger);
    }
    else
    {
        mode_.set(ConsensusMode::wrongLedger, adaptor_);
    }
}

void
RpcaConsensus::leaveConsensus()
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

void
RpcaConsensus::phaseOpen()
{
    using namespace std::chrono;

    // it is shortly before ledger close time
    bool anyTransactions = adaptor_.hasOpenTransactions();
    auto proposersClosed = currPeerPositions_.size();
    auto proposersValidated = adaptor_.proposersValidated(prevLedgerID_);

    openTime_.tick(clock_.now());

    // This computes how long since last ledger's close time
    milliseconds sinceClose;
    {
        bool previousCloseCorrect =
            (mode_.get() != ConsensusMode::wrongLedger) &&
            previousLedger_.closeAgree() &&
            (previousLedger_.closeTime() !=
             (previousLedger_.parentCloseTime() + 1s));

        auto lastCloseTime = previousCloseCorrect
            ? previousLedger_.closeTime()  // use consensus timing
            : prevCloseTime_;              // use the time we saw internally

        if (now_ >= lastCloseTime)
            sinceClose = duration_cast<milliseconds>(now_ - lastCloseTime);
        else
            sinceClose = -duration_cast<milliseconds>(lastCloseTime - now_);
    }

    auto const idleInterval = std::max<milliseconds>(
        adaptor_.parms().ledgerIDLE_INTERVAL,
        2 * previousLedger_.closeTimeResolution());

    // Decide if we should close the ledger
    if (shouldCloseLedger(
            anyTransactions,
            prevProposers_,
            proposersClosed,
            proposersValidated,
            prevRoundTime_,
            sinceClose,
            openTime_.read(),
            idleInterval,
            adaptor_.parms(),
            j_))
    {
        closeLedger();
    }
}

void
RpcaConsensus::closeLedger()
{
    // We should not be closing if we already have a position
    assert(!result_);

    phase_ = ConsensusPhase::establish;
    rawCloseTimes_.self = now_;

    result_.emplace(adaptor_.onClose(previousLedger_, now_, mode_.get()));
    result_->roundTime.reset(clock_.now());
    // Share the newly created transaction set if we haven't already
    // received it from a peer
    if (acquired_.emplace(result_->txns.id(), result_->txns).second)
        adaptor_.RpcaPopAdaptor::Adaptor::share(result_->txns);

    if (mode_.get() == ConsensusMode::proposing)
        adaptor_.propose(result_->position);

    // Create disputes with any peer positions we have transactions for
    for (auto const& pit : currPeerPositions_)
    {
        auto const& pos = pit.second.proposal().position();
        auto const it = acquired_.find(pos);
        if (it != acquired_.end())
        {
            createDisputes(it->second);
        }
    }
}

void
RpcaConsensus::createDisputes(TxSet_t const& o)
{
    // Cannot create disputes without our stance
    assert(result_);

    // Only create disputes if this is a new set
    if (!result_->compares.emplace(o.id()).second)
        return;

    // Nothing to dispute if we agree
    if (result_->txns.id() == o.id())
        return;

    JLOG(j_.debug()) << "createDisputes " << result_->txns.id() << " to "
                     << o.id();

    auto differences = result_->txns.compare(o);

    int dc = 0;

    for (auto& id : differences)
    {
        ++dc;
        // create disputed transactions (from the ledger that has them)
        assert(
            (id.second && result_->txns.find(id.first) && !o.find(id.first)) ||
            (!id.second && !result_->txns.find(id.first) && o.find(id.first)));

        Tx_t tx = id.second ? *result_->txns.find(id.first) : *o.find(id.first);
        auto txID = tx.id();

        if (result_->disputes.find(txID) != result_->disputes.end())
            continue;

        JLOG(j_.debug()) << "Transaction " << txID << " is disputed";

        typename Result::Dispute_t dtx{
            tx,
            result_->txns.exists(txID),
            std::max(prevProposers_, currPeerPositions_.size()),
            j_};

        // Update all of the available peer's votes on the disputed transaction
        for (auto const& pit : currPeerPositions_)
        {
            Proposal_t const& peerProp = pit.second.proposal();
            auto const cit = acquired_.find(peerProp.position());
            if (cit != acquired_.end())
                dtx.setVote(pit.first, cit->second.exists(txID));
        }
        adaptor_.share(dtx.tx());

        result_->disputes.emplace(txID, std::move(dtx));
    }
    JLOG(j_.debug()) << dc << " differences found";
}

void
RpcaConsensus::phaseEstablish()
{
    // can only establish consensus if we already took a stance
    assert(result_);

    using namespace std::chrono;
    RpcaConsensusParms const& parms = adaptor_.parms();

    result_->roundTime.tick(clock_.now());
    result_->proposers = currPeerPositions_.size();

    convergePercent_ = result_->roundTime.read() * 100 /
        std::max<milliseconds>(prevRoundTime_, parms.avMIN_CONSENSUS_TIME);

    // Give everyone a chance to take an initial position
    if (result_->roundTime.read() < parms.ledgerMIN_CONSENSUS)
        return;

    updateOurPositions();

    // Nothing to do if too many laggards or we don't have consensus.
    if (shouldPause() || !haveConsensus())
        return;

    if (!haveCloseTimeConsensus_)
    {
        JLOG(j_.info()) << "We have TX consensus but not CT consensus";
        return;
    }

    JLOG(j_.info()) << "Converge cutoff (" << currPeerPositions_.size()
                    << " participants)";
    adaptor_.updateOperatingMode(currPeerPositions_.size());
    prevProposers_ = currPeerPositions_.size();
    prevRoundTime_ = result_->roundTime.read();
    phase_ = ConsensusPhase::accepted;
    adaptor_.onAccept(
        *result_,
        previousLedger_,
        closeResolution_,
        rawCloseTimes_,
        mode_.get(),
        getJson(true));
}

void
RpcaConsensus::updateOurPositions()
{
    // We must have a position if we are updating it
    assert(result_);

    RpcaConsensusParms const& parms = adaptor_.parms();

    // Compute a cutoff time
    auto const peerCutoff = now_ - parms.proposeFRESHNESS;
    auto const ourCutoff = now_ - parms.proposeINTERVAL;

    // Verify freshness of peer positions and compute close times
    std::map<NetClock::time_point, int> closeTimeVotes;
    {
        auto it = currPeerPositions_.begin();
        while (it != currPeerPositions_.end())
        {
            Proposal_t const& peerProp = it->second.proposal();
            if (peerProp.isStale(peerCutoff))
            {
                // peer's proposal is stale, so remove it
                NodeID_t const& peerID = peerProp.nodeID();
                JLOG(j_.warn()) << "Removing stale proposal from " << peerID;
                for (auto& dt : result_->disputes)
                    dt.second.unVote(peerID);
                it = currPeerPositions_.erase(it);
            }
            else
            {
                // proposal is still fresh
                ++closeTimeVotes[asCloseTime(peerProp.closeTime())];
                ++it;
            }
        }
    }

    // This will stay unseated unless there are any changes
    boost::optional<TxSet_t> ourNewSet;

    // Update votes on disputed transactions
    {
        boost::optional<typename TxSet_t::MutableTxSet> mutableSet;
        for (auto& [txId, dispute] : result_->disputes)
        {
            // Because the threshold for inclusion increases,
            //  time can change our position on a dispute
            if (dispute.updateVote(
                    convergePercent_,
                    mode_.get() == ConsensusMode::proposing,
                    parms))
            {
                if (!mutableSet)
                    mutableSet.emplace(result_->txns);

                if (dispute.getOurVote())
                {
                    // now a yes
                    mutableSet->insert(dispute.tx());
                }
                else
                {
                    // now a no
                    mutableSet->erase(txId);
                }
            }
        }

        if (mutableSet)
            ourNewSet.emplace(std::move(*mutableSet));
    }

    NetClock::time_point consensusCloseTime = {};
    haveCloseTimeConsensus_ = false;

    if (currPeerPositions_.empty())
    {
        // no other times
        haveCloseTimeConsensus_ = true;
        consensusCloseTime = asCloseTime(result_->position.closeTime());
    }
    else
    {
        int neededWeight;

        if (convergePercent_ < parms.avMID_CONSENSUS_TIME)
            neededWeight = parms.avINIT_CONSENSUS_PCT;
        else if (convergePercent_ < parms.avLATE_CONSENSUS_TIME)
            neededWeight = parms.avMID_CONSENSUS_PCT;
        else if (convergePercent_ < parms.avSTUCK_CONSENSUS_TIME)
            neededWeight = parms.avLATE_CONSENSUS_PCT;
        else
            neededWeight = parms.avSTUCK_CONSENSUS_PCT;

        int participants = currPeerPositions_.size();
        if (mode_.get() == ConsensusMode::proposing)
        {
            ++closeTimeVotes[asCloseTime(result_->position.closeTime())];
            ++participants;
        }

        // Threshold for non-zero vote
        int threshVote = participantsNeeded(participants, neededWeight);

        // Threshold to declare consensus
        int const threshConsensus =
            participantsNeeded(participants, parms.avCT_CONSENSUS_PCT);

        JLOG(j_.info()) << "Proposers:" << currPeerPositions_.size()
                        << " nw:" << neededWeight << " thrV:" << threshVote
                        << " thrC:" << threshConsensus;

        for (auto const& [t, v] : closeTimeVotes)
        {
            JLOG(j_.debug())
                << "CCTime: seq "
                << static_cast<std::uint32_t>(previousLedger_.seq()) + 1 << ": "
                << t.time_since_epoch().count() << " has " << v << ", "
                << threshVote << " required";

            if (v >= threshVote)
            {
                // A close time has enough votes for us to try to agree
                consensusCloseTime = t;
                threshVote = v;

                if (threshVote >= threshConsensus)
                    haveCloseTimeConsensus_ = true;
            }
        }

        if (!haveCloseTimeConsensus_)
        {
            JLOG(j_.debug())
                << "No CT consensus:"
                << " Proposers:" << currPeerPositions_.size()
                << " Mode:" << to_string(mode_.get())
                << " Thresh:" << threshConsensus
                << " Pos:" << consensusCloseTime.time_since_epoch().count();
        }
    }

    if (!ourNewSet &&
        ((consensusCloseTime != asCloseTime(result_->position.closeTime())) ||
         result_->position.isStale(ourCutoff)))
    {
        // close time changed or our position is stale
        ourNewSet.emplace(result_->txns);
    }

    if (ourNewSet)
    {
        auto newID = ourNewSet->id();

        result_->txns = std::move(*ourNewSet);

        JLOG(j_.info()) << "Position change: CTime "
                        << consensusCloseTime.time_since_epoch().count()
                        << ", tx " << newID;

        result_->position.changePosition(newID, consensusCloseTime, now_);

        // Share our new transaction set and update disputes
        // if we haven't already received it
        if (acquired_.emplace(newID, result_->txns).second)
        {
            if (!result_->position.isBowOut())
                adaptor_.RpcaPopAdaptor::Adaptor::share(result_->txns);

            for (auto const& [nodeId, peerPos] : currPeerPositions_)
            {
                Proposal_t const& p = peerPos.proposal();
                if (p.position() == newID)
                    updateDisputes(nodeId, result_->txns);
            }
        }

        // Share our new position if we are still participating this round
        if (!result_->position.isBowOut() &&
            (mode_.get() == ConsensusMode::proposing))
            adaptor_.propose(result_->position);
    }
}

bool
RpcaConsensus::shouldPause() const
{
    auto const& parms = adaptor_.parms();
    std::uint32_t const ahead(
        previousLedger_.seq() -
        std::min(adaptor_.getValidLedgerIndex(), previousLedger_.seq()));
    auto quorumKeys = adaptor_.getQuorumKeys();
    auto const& quorum = quorumKeys.first;
    auto& trustedKeys = quorumKeys.second;
    std::size_t const totalValidators = trustedKeys.size();
    std::size_t laggards =
        adaptor_.laggards(previousLedger_.seq(), trustedKeys);
    std::size_t const offline = trustedKeys.size();

    std::stringstream vars;
    vars << " (working seq: " << previousLedger_.seq() << ", "
         << "validated seq: " << adaptor_.getValidLedgerIndex() << ", "
         << "am validator: " << adaptor_.validator() << ", "
         << "have validated: " << adaptor_.haveValidated() << ", "
         << "roundTime: " << result_->roundTime.read().count() << ", "
         << "max consensus time: " << parms.ledgerMAX_CONSENSUS.count() << ", "
         << "validators: " << totalValidators << ", "
         << "laggards: " << laggards << ", "
         << "offline: " << offline << ", "
         << "quorum: " << quorum << ")";

    if (!ahead || !laggards || !totalValidators || !adaptor_.validator() ||
        !adaptor_.haveValidated() ||
        result_->roundTime.read() > parms.ledgerMAX_CONSENSUS)
    {
        j_.debug() << "not pausing" << vars.str();
        return false;
    }

    bool willPause = false;

    /** Maximum phase with distinct thresholds to determine how
     *  many validators must be on our same ledger sequence number.
     *  The threshold for the 1st (0) phase is >= the minimum number that
     *  can achieve quorum. Threshold for the maximum phase is 100%
     *  of all trusted validators. Progression from min to max phase is
     *  simply linear. If there are 5 phases (maxPausePhase = 4)
     *  and minimum quorum is 80%, then thresholds progress as follows:
     *  0: >=80%
     *  1: >=85%
     *  2: >=90%
     *  3: >=95%
     *  4: =100%
     */
    constexpr static std::size_t maxPausePhase = 4;

    /**
     * No particular threshold guarantees consensus. Lower thresholds
     * are easier to achieve than higher, but higher thresholds are
     * more likely to reach consensus. Cycle through the phases if
     * lack of synchronization continues.
     *
     * Current information indicates that no phase is likely to be intrinsically
     * better than any other: the lower the threshold, the less likely that
     * up-to-date nodes will be able to reach consensus without the laggards.
     * But the higher the threshold, the longer the likely resulting pause.
     * 100% is slightly less desirable in the long run because the potential
     * of a single dawdling peer to slow down everything else. So if we
     * accept that no phase is any better than any other phase, but that
     * all of them will potentially enable us to arrive at consensus, cycling
     * through them seems to be appropriate. Further, if we do reach the
     * point of having to cycle back around, then it's likely that something
     * else out of the scope of this delay mechanism is wrong with the
     * network.
     */
    std::size_t const phase = (ahead - 1) % (maxPausePhase + 1);

    // validators that remain after the laggards() function are considered
    // offline, and should be considered as laggards for purposes of
    // evaluating whether the threshold for non-laggards has been reached.
    switch (phase)
    {
        case 0:
            // Laggards and offline shouldn't preclude consensus.
            if (laggards + offline > totalValidators - quorum)
                willPause = true;
            break;
        case maxPausePhase:
            // No tolerance.
            willPause = true;
            break;
        default:
            // Ensure that sufficient validators are known to be not lagging.
            // Their sufficiently most recent validation sequence was equal to
            // or greater than our own.
            //
            // The threshold is the amount required for quorum plus
            // the proportion of the remainder based on number of intermediate
            // phases between 0 and max.
            float const nonLaggards = totalValidators - (laggards + offline);
            float const quorumRatio =
                static_cast<float>(quorum) / totalValidators;
            float const allowedDissent = 1.0f - quorumRatio;
            float const phaseFactor = static_cast<float>(phase) / maxPausePhase;

            if (nonLaggards / totalValidators <
                quorumRatio + (allowedDissent * phaseFactor))
            {
                willPause = true;
            }
    }

    if (willPause)
        j_.warn() << "pausing" << vars.str();
    else
        j_.debug() << "not pausing" << vars.str();
    return willPause;
}

bool
RpcaConsensus::haveConsensus()
{
    // Must have a stance if we are checking for consensus
    assert(result_);

    // CHECKME: should possibly count unacquired TX sets as disagreeing
    int agree = 0, disagree = 0;

    auto ourPosition = result_->position.position();

    // Count number of agreements/disagreements with our position
    for (auto const& [nodeId, peerPos] : currPeerPositions_)
    {
        Proposal_t const& peerProp = peerPos.proposal();
        if (peerProp.position() == ourPosition)
        {
            ++agree;
        }
        else
        {
            using std::to_string;

            JLOG(j_.debug()) << to_string(nodeId) << " has "
                             << to_string(peerProp.position());
            ++disagree;
        }
    }
    auto currentFinished =
        adaptor_.proposersFinished(previousLedger_, prevLedgerID_);

    JLOG(j_.debug()) << "Checking for TX consensus: agree=" << agree
                     << ", disagree=" << disagree;

    // Determine if we actually have consensus or not
    result_->state = checkConsensus(
        prevProposers_,
        agree + disagree,
        agree,
        currentFinished,
        prevRoundTime_,
        result_->roundTime.read(),
        adaptor_.parms(),
        mode_.get() == ConsensusMode::proposing,
        j_);

    if (result_->state == ConsensusState::No)
        return false;

    // There is consensus, but we need to track if the network moved on
    // without us.
    if (result_->state == ConsensusState::MovedOn)
    {
        JLOG(j_.error()) << "Unable to reach consensus";
        JLOG(j_.error()) << Json::Compact{getJson(true)};
    }

    return true;
}

NetClock::time_point
RpcaConsensus::asCloseTime(NetClock::time_point raw) const
{
    return roundCloseTime(raw, closeResolution_);
}

void
RpcaConsensus::updateDisputes(NodeID_t const& node, TxSet_t const& other)
{
    // Cannot updateDisputes without our stance
    assert(result_);

    // Ensure we have created disputes against this set if we haven't seen
    // it before
    if (result_->compares.find(other.id()) == result_->compares.end())
        createDisputes(other);

    for (auto& it : result_->disputes)
    {
        auto& d = it.second;
        d.setVote(node, other.exists(d.tx().id()));
    }
}

bool
RpcaConsensus::peerProposal(
    std::shared_ptr<PeerImp>& peer,
    bool isTrusted,
    std::shared_ptr<protocol::TMConsensus> const& m)
{
    try
    {
        STProposeSet::pointer propose;

        SerialIter sit(makeSlice(m->msg()));
        PublicKey const publicKey{makeSlice(m->signerpubkey())};

        propose = std::make_shared<STProposeSet>(
            sit, adaptor_.closeTime(), calcNodeID(publicKey), publicKey);

        auto newPeerPos = RCLCxPeerPos(
            publicKey,
            makeSlice(m->signature()),
            consensusMessageUniqueId(*m),
            std::move(*propose));

        if (isTrusted)
        {
            NodeID_t const& peerID = newPeerPos.proposal().nodeID();

            // Always need to store recent positions
            {
                auto& props = recentPeerPositions_[peerID];

                if (props.size() >= 10)
                    props.pop_front();

                props.push_back(newPeerPos);
            }

            return peerProposalInternal(adaptor_.closeTime(), newPeerPos);
        }
        else
        {
            if (peer->cluster() ||
                prevLedgerID_ == newPeerPos.proposal().prevLedger())
            {
                // relay untrusted proposal
                JLOG(j_.trace()) << "relaying UNTRUSTED proposal";
                return true;
            }
            else
            {
                JLOG(j_.debug()) << "Not relaying UNTRUSTED proposal";
            }
        }
    }
    catch (std::exception const& e)
    {
        JLOG(j_.warn()) << "ProposeSet: Exception, " << e.what();
        peer->charge(Resource::feeInvalidRequest);
    }

    return false;
}

bool
RpcaConsensus::peerProposalInternal(
    NetClock::time_point const& now,
    PeerPosition_t const& newPeerPos)
{
    // Nothing to do for now if we are currently working on a ledger
    if (phase_ == ConsensusPhase::accepted)
        return false;

    now_ = now;

    Proposal_t const& newPeerProp = newPeerPos.proposal();

    NodeID_t const& peerID = newPeerProp.nodeID();

    if (newPeerProp.prevLedger() != prevLedgerID_)
    {
        JLOG(j_.debug()) << "Got proposal for " << newPeerProp.prevLedger()
                         << " but we are on " << prevLedgerID_;
        return false;
    }

    if (deadNodes_.find(peerID) != deadNodes_.end())
    {
        using std::to_string;
        JLOG(j_.info()) << "Position from dead node: " << to_string(peerID);
        return false;
    }

    {
        // update current position
        auto peerPosIt = currPeerPositions_.find(peerID);

        if (peerPosIt != currPeerPositions_.end())
        {
            if (newPeerProp.proposeSeq() <=
                peerPosIt->second.proposal().proposeSeq())
            {
                return false;
            }
        }

        if (newPeerProp.isBowOut())
        {
            using std::to_string;

            JLOG(j_.info()) << "Peer bows out: " << to_string(peerID);
            if (result_)
            {
                for (auto& it : result_->disputes)
                    it.second.unVote(peerID);
            }
            if (peerPosIt != currPeerPositions_.end())
                currPeerPositions_.erase(peerID);
            deadNodes_.insert(peerID);

            return true;
        }

        if (peerPosIt != currPeerPositions_.end())
            peerPosIt->second = newPeerPos;
        else
            currPeerPositions_.emplace(peerID, newPeerPos);
    }
    if (newPeerProp.isInitial())
    {
        // Record the close time estimate
        JLOG(j_.trace()) << "Peer reports close time as "
                         << newPeerProp.closeTime().time_since_epoch().count();
        ++rawCloseTimes_.peers[newPeerProp.closeTime()];
    }

    JLOG(j_.trace()) << "Processing peer proposal " << newPeerProp.proposeSeq()
                     << "/" << newPeerProp.position();

    {
        auto const ait = acquired_.find(newPeerProp.position());
        if (ait == acquired_.end())
        {
            // acquireTxSet will return the set if it is available, or
            // spawn a request for it and return none/nullptr.  It will call
            // gotTxSet once it arrives
            if (auto set = adaptor_.acquireTxSet(newPeerProp.position()))
                gotTxSet(now_, *set);
            else
                JLOG(j_.debug()) << "Don't have tx set for peer";
        }
        else if (result_)
        {
            updateDisputes(newPeerProp.nodeID(), ait->second);
        }
    }

    return true;
}

bool
RpcaConsensus::peerValidation(
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
            PublicKey const publicKey{makeSlice(m->signerpubkey())};
            val = std::make_shared<STValidation>(
                std::ref(sit), publicKey, [&](PublicKey const& pk) {
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

}  // namespace ripple
