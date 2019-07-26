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

#ifndef RIPPLE_APP_CONSENSUS_PCLCONSENSUS_H_INCLUDED
#define RIPPLE_APP_CONSENSUS_PCLCONSENSUS_H_INCLUDED

#include <ripple/basics/Log.h>
#include <ripple/basics/chrono.h>
#include <ripple/beast/utility/Journal.h>
#include <ripple/consensus/ConsensusProposal.h>
#include <ripple/consensus/ConsensusParms.h>
#include <ripple/consensus/ConsensusTypes.h>
#include <ripple/consensus/LedgerTiming.h>
#include <ripple/json/json_writer.h>
#include <peersafe/app/util/Common.h>
#include <peersafe/app/misc/TxPool.h>
#include <peersafe/app/consensus/PConsensusParams.h>
#include <peersafe/app/consensus/ConsensusBase.h>

namespace ripple {

class PublicKey;

template <class Adaptor>
class PConsensus : public ConsensusBase<Adaptor>
{
	using Ledger_t = typename Adaptor::Ledger_t;
	using TxSet_t = typename Adaptor::TxSet_t;
	using NodeID_t = typename Adaptor::NodeID_t;
	using PeerPosition_t = typename Adaptor::PeerPosition_t;
	using Proposal_t = ConsensusProposal<
		NodeID_t,
		typename Ledger_t::ID,
		typename TxSet_t::ID>;

    using Result = ConsensusResult<Adaptor>;
	// Helper class to ensure adaptor is notified whenever the ConsensusMode
	// changes
	class MonitoredMode
	{
		ConsensusMode mode_;

	public:
		MonitoredMode(ConsensusMode m) : mode_{ m }
		{
		}
		ConsensusMode
			get() const
		{
			return mode_;
		}

		void
			set(ConsensusMode mode, Adaptor& a)
		{
			a.onModeChange(mode_, mode);
			mode_ = mode;
		}
	};

public:
	//! Clock type for measuring time within the consensus code
	using clock_type = beast::abstract_clock<std::chrono::steady_clock>;

	/** Constructor.

		@param clock The clock used to internally sample consensus progress
		@param adaptor The instance of the adaptor class
		@param j The journal to log debug output
	*/
	PConsensus(clock_type const& clock, Adaptor& adaptor, beast::Journal j);
	/** Kick-off the next round of consensus.

		Called by the client code to start each round of consensus.

		@param now The network adjusted time
		@param prevLedgerID the ID of the last ledger
		@param prevLedger The last ledger
		@param proposing Whether we want to send proposals to peers this round.

		@note @b prevLedgerID is not required to the ID of @b prevLedger since
		the ID may be known locally before the contents of the ledger arrive
	*/
	void
		startRound(
			NetClock::time_point const& now,
			typename Ledger_t::ID const& prevLedgerID,
			Ledger_t prevLedger,
			bool proposing);

	/** Process a transaction set acquired from the network

		@param now The network adjusted time
		@param txSet the transaction set
	*/
	void
		gotTxSet(NetClock::time_point const& now, TxSet_t const& txSet);

	void
		simulate(
			NetClock::time_point const& now,
			boost::optional<std::chrono::milliseconds> consensusDelay)
	{}
	/** Call periodically to drive consensus forward.

		@param now The network adjusted time
	*/
	void
		timerEntry(NetClock::time_point const& now);
	/** A peer has proposed a new position, adjust our tracking.

		 @param now The network adjusted time
		 @param newProposal The new proposal from a peer
		 @return Whether we should do delayed relay of this proposal.
	 */
	bool
		peerProposal(
			NetClock::time_point const& now,
			PeerPosition_t const& newProposal);
	/** Get the previous ledger ID.

		The previous ledger is the last ledger seen by the consensus code and
		should correspond to the most recent validated ledger seen by this peer.

		@return ID of previous ledger
	*/
	typename Ledger_t::ID
		prevLedgerID() const
	{
		return prevLedgerID_;
	}

	/** Get the Json state of the consensus process.

		Called by the consensus_info RPC.

		@param full True if verbose response desired.
		@return     The Json state.
	*/
	Json::Value
		getJson(bool full) const;
private:
	void
		startRoundInternal(
			NetClock::time_point const& now,
			typename Ledger_t::ID const& prevLedgerID,
			Ledger_t const& prevLedger,
			ConsensusMode mode);

	/** Handle a replayed or a new peer proposal.
	*/
	bool
		peerProposalInternal(
			NetClock::time_point const& now,
			PeerPosition_t const& newProposal);

	// Change our view of the previous ledger
	void
		handleWrongLedger(typename Ledger_t::ID const& lgrId);

	/** Check if our previous ledger matches the network's.

		If the previous ledger differs, we are no longer in sync with
		the network and need to bow out/switch modes.
	*/
	void
		checkLedger();

	/** Handle tx-collecting phase.

		In the tx-collecting phase, the ledger is open as we wait for new
		transactions.  After enough time has elapsed, we will close the ledger,
		switch to the establish phase and start the consensus process.
	*/
	void
		phaseCollecting();

	/** Handle voting phase.

		In the voting phase, the ledger has closed and we work with peers
		to reach consensus. Update our position only on the timer, and in this
		phase.

		If we have consensus, move to the accepted phase.
	*/
	void
		phaseVoting();

	bool
		haveConsensus();

	/** If we should package a block
		The leader for this block or next block notified return true.
	*/
	bool shouldPack();

	bool isLeader(PublicKey const& pub);

	/** Is final condition reached for proposing.
		We should check:
		1. Is tx-count reached max and minBlockTime reached.
		2. Is maxBlockTime reached.
		3. Is this node the next leader and the above 2 conditions reached.
	*/
	bool finalCondReached(uint64 sinceLastClose);

	void appendTransactions(h256Set const& txSet);

	std::chrono::milliseconds timeSinceClose();
	uint64 timeSinceOpen();

	void leaveConsensus();
private:
	Adaptor& adaptor_;

	ConsensusPhase phase_{ ConsensusPhase::accepted };
	MonitoredMode mode_{ ConsensusMode::observing };

	bool firstRound_ = true;

	clock_type const& clock_;

	//-------------------------------------------------------------------------
	// Network time measurements of consensus progress

	// The current network adjusted time.  This is the network time the
	// ledger would close if it closed now
	NetClock::time_point now_;
	NetClock::time_point prevCloseTime_;
	NetClock::time_point closeTime_;
	uint64 openTime2_;

	//-------------------------------------------------------------------------
	// Non-peer (self) consensus data

	// Last validated ledger ID provided to consensus
	typename Ledger_t::ID prevLedgerID_;
	// Last validated ledger seen by consensus
	Ledger_t previousLedger_;

	// How long has this round been open
	ConsensusTimer openTime_;

	NetClock::duration closeResolution_ = ledgerDefaultTimeResolution;

	// Transaction Sets, indexed by hash of transaction tree
	hash_map<typename TxSet_t::ID, const TxSet_t> acquired_;

	// Tx set peers has voted(including self)
	hash_map<typename TxSet_t::ID, std::set<PublicKey>> txSetVoted_;
	hash_map<typename TxSet_t::ID, std::set<PublicKey>> txSetCached_;

	boost::optional<Result> result_;

	//current setID proposed by leader.
	boost::optional<typename TxSet_t::ID> setID_;

	ConsensusCloseTimes rawCloseTimes_;

	// Transaction hashes that have packaged in packaging block.
	std::vector<uint256> transactions_;

	// The number of proposers who participated in the last consensus round
	std::size_t prevProposers_ = 0;

	// The minimum block generation time(ms)
	unsigned minBlockTime_ = MinBlockTime;

	// The maximum block generation time(ms) even without transactions.
	unsigned maxBlockTime_ = MaxBlockTime;

	unsigned maxTxsInLedger_ = MaxTxsInLedger;

	// Journal for debugging
	beast::Journal j_;
};

template <class Adaptor>
PConsensus<Adaptor>::PConsensus(
	clock_type const& clock,
	Adaptor& adaptor,
	beast::Journal journal)
	: adaptor_(adaptor)
	, clock_(clock)
	, j_{ journal }
{
	JLOG(j_.debug()) << "Creating consensus object";
}

template <class Adaptor>
void
PConsensus<Adaptor>::startRound(
	NetClock::time_point const& now,
	typename Ledger_t::ID const& prevLedgerID,
	Ledger_t prevLedger,
	bool proposing)
{
	if (firstRound_)
	{
		prevCloseTime_ = prevLedger.closeTime();
		firstRound_ = false;
	}
	else
	{
		prevCloseTime_ = now;
	}

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

template <class Adaptor>
void
PConsensus<Adaptor>::startRoundInternal(
		NetClock::time_point const& now,
		typename Ledger_t::ID const& prevLedgerID,
		Ledger_t const& prevLedger,
		ConsensusMode mode)
{
	phase_ = ConsensusPhase::open;
	mode_.set(mode, adaptor_);
	now_ = now;
	closeTime_ = now;
	openTime2_ = utcTime();
	prevLedgerID_ = prevLedgerID;
	previousLedger_ = prevLedger;
	result_.reset();
	openTime_.reset(clock_.now());
	acquired_.clear();
	rawCloseTimes_.peers.clear();
	rawCloseTimes_.self = {};
	txSetCached_.clear();
	txSetVoted_.clear();
    transactions_.clear();
    setID_.reset();


	closeResolution_ = getNextLedgerTimeResolution(
		previousLedger_.closeTimeResolution(),
		previousLedger_.closeAgree(),
		previousLedger_.seq() + 1);

	//if (currPeerPositions_.size() > (prevProposers_ / 2))
	//{
	//	// We may be falling behind, don't wait for the timer
	//	// consider closing the ledger immediately
	//	timerEntry(now_);
	//}
}

template <class Adaptor>
bool
PConsensus<Adaptor>::peerProposal(
	NetClock::time_point const& now,
	PeerPosition_t const& newPeerPos)
{
	return peerProposalInternal(now, newPeerPos);
}

template <class Adaptor>
bool
PConsensus<Adaptor>::peerProposalInternal(
	NetClock::time_point const& now,
	PeerPosition_t const& newPeerPos)
{
	// Nothing to do for now if we are currently working on a ledger
	if (phase_ == ConsensusPhase::accepted)
		return false;

	now_ = now;

	Proposal_t const& newPeerProp = newPeerPos.proposal();

	//NodeID_t const& peerID = newPeerProp.nodeID();

	if (newPeerProp.prevLedger() != prevLedgerID_)
	{
		JLOG(j_.debug()) << "Got proposal for " << newPeerProp.prevLedger()
			<< " from "<< toBase58(TOKEN_NODE_PUBLIC, newPeerPos.publicKey()) << " but we are on " << prevLedgerID_;
		return false;
	}

	JLOG(j_.info()) << "Processing peer proposal " << newPeerProp.proposeSeq()
		<< "/" << newPeerProp.position();

	{
		PublicKey pub = newPeerPos.publicKey();
		auto newSetID = newPeerProp.position();
		auto iter = txSetVoted_.find(newSetID);
		if (iter != txSetVoted_.end())
		{
			JLOG(j_.info()) << "Got proposal for set from public :" << toBase58(TOKEN_NODE_PUBLIC, pub);
			iter->second.insert(pub);
		}
		else
		{
			if (isLeader(pub))
			{
				JLOG(j_.info()) << "Got proposal from leader, enter phase::establish.";

				txSetVoted_[newSetID] = std::set<PublicKey>{pub};
				setID_ = newSetID;
				rawCloseTimes_.self = now_;
				closeTime_ = newPeerProp.closeTime();

				if (txSetCached_.find(*setID_) != txSetCached_.end())
				{
					for (auto pub : txSetCached_[*setID_])
						txSetVoted_[newSetID].insert(pub);
					txSetCached_.erase(*setID_);
				}
                
				JLOG(j_.info()) << "voting for set:" << *setID_ << " " << txSetVoted_[*setID_].size();
				for (auto pub : txSetVoted_[*setID_])
				{
					JLOG(j_.info()) << "voting public for set:" << toBase58(TOKEN_NODE_PUBLIC, pub);
				}

                if (phase_ == ConsensusPhase::open)
                    phase_ = ConsensusPhase::establish;
			}
			else
			{
				JLOG(j_.info()) << "Got proposal for set from public " << toBase58(TOKEN_NODE_PUBLIC, pub) <<" and added to cache";
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
		// Record the close time estimate
		JLOG(j_.debug()) << "Peer reports close time as "
			<< newPeerProp.closeTime().time_since_epoch().count();
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

template <class Adaptor>
void
PConsensus<Adaptor>::gotTxSet(
	NetClock::time_point const& now,
	TxSet_t const& txSet)
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

	if (!isLeader(adaptor_.valPublic_))
	{
		if (setID_ && setID_ == id)
		{
			//update avoid if we got the right tx-set
			adaptor_.app_.getTxPool().updateAvoid(txSet);

			auto set = txSet.map_->snapShot(false);
			//this place has a txSet copy,what's the time it costs?
			result_.emplace(Result(
			std::move(set),
			RCLCxPeerPos::Proposal(
				prevLedgerID_,
				RCLCxPeerPos::Proposal::seqJoin,
				id,
				closeTime_,
				now,
				adaptor_.nodeID())) );

			txSetVoted_[*setID_].insert(adaptor_.valPublic_);

			result_->roundTime.reset(clock_.now());

			if (adaptor_.validating())
			{
				adaptor_.propose(result_->position);
			}			

			JLOG(j_.info()) << "voting for set:" << *setID_ <<" "<< txSetVoted_[*setID_].size();
			for (auto pub : txSetVoted_[*setID_])
			{
				JLOG(j_.info()) << "voting public for set:" << toBase58(TOKEN_NODE_PUBLIC, pub);
			}
		}


		JLOG(j_.info()) << "We are not leader gotTxSet,and proposing position:" << id;
	}
}

template <class Adaptor>
void
PConsensus<Adaptor>::timerEntry(NetClock::time_point const& now)
{
	// Nothing to do if we are currently working on a ledger
	if (phase_ == ConsensusPhase::accepted)
		return;

	now_ = now;

	// Check we are on the proper ledger (this may change phase_)
	checkLedger();

	if (phase_ == ConsensusPhase::open)
	{
		phaseCollecting();
	}
	else if (phase_ == ConsensusPhase::establish)
	{
		phaseVoting();
	}
}

template <class Adaptor>
void
PConsensus<Adaptor>::appendTransactions(h256Set const& txSet)
{
	for (auto const& trans : txSet)
		transactions_.push_back(trans);
}

template <class Adaptor>
std::chrono::milliseconds
PConsensus<Adaptor>::timeSinceClose()
{
	using namespace std::chrono;
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
	return sinceClose;
}

template <class Adaptor>
uint64
PConsensus<Adaptor>::timeSinceOpen()
{
	//openTime_.tick(clock_.now());
	//return openTime_.read();
	//using namespace std::chrono;
	return utcTime() - openTime2_;
}

template <class Adaptor>
void
PConsensus<Adaptor>::phaseCollecting()
{
	// it is shortly before ledger close time
	//bool anyTransactions = adaptor_.hasOpenTransactions();
	//auto proposersValidated = adaptor_.proposersValidated(prevLedgerID_);

	//auto sinceClose = timeSinceClose();
	auto sinceOpen = timeSinceOpen();

	JLOG(j_.info()) << "phaseCollecting time sinceOpen:" << sinceOpen <<"ms";

	// Decide if we should propose a tx-set
	if (shouldPack())
	{
		int tx_count = transactions_.size();

		//if time for this block's tx-set reached
		bool bTimeReached = sinceOpen.count() >= maxBlockTime_;
		if (tx_count < maxTxsInLedger_ && !bTimeReached)
		{
			appendTransactions(adaptor_.app_.getTxPool().topTransactions(maxTxsInLedger_ - tx_count));
		}

		if (finalCondReached(sinceOpen))
		{
			/** 
			1. construct result_ 
			2. propose position
			3. add position to self
			*/

			phase_ = ConsensusPhase::establish;
			rawCloseTimes_.self = now_;

			result_.emplace(adaptor_.onCollectFinish(previousLedger_, transactions_, now_, mode_.get()));
			result_->roundTime.reset(clock_.now());
			setID_ = result_->set.id();

			// Share the newly created transaction set if we haven't already
			// received it from a peer
			if (acquired_.emplace(*setID_, result_->set).second)
				adaptor_.relay(result_->set);

			if (mode_.get() == ConsensusMode::proposing)
				adaptor_.propose(result_->position);

			txSetVoted_[*setID_] = std::set<PublicKey>{ adaptor_.valPublic_ };

			JLOG(j_.info()) << "We are leader,proposing position:" << *setID_;
		}
	}
	else
	{
		//in case we are not leader,the proposal leader should propose not received,
		// but other nodes have accepted the ledger of this sequence
		int minVal = adaptor_.app_.validators().quorum();
		auto currentFinished = adaptor_.proposersFinished(prevLedgerID_);
		if (currentFinished >= minVal)
		{
			result_.emplace(adaptor_.onCollectFinish(previousLedger_, transactions_, now_, mode_.get()));
			result_->roundTime.reset(clock_.now());
			rawCloseTimes_.self = now_;
			phase_ = ConsensusPhase::establish;
			JLOG(j_.warn()) << "Enter establish without receiving leader proposal!";
		}
	}
}

template <class Adaptor>
void
PConsensus<Adaptor>::phaseVoting()
{
	// can only establish consensus if we already took a stance
	assert(result_);

	using namespace std::chrono;
	//ConsensusParms const & parms = adaptor_.parms();

	result_->roundTime.tick(clock_.now());
	//result_->proposers = currPeerPositions_.size();

	JLOG(j_.info()) << "phaseVoting roundTime:" << result_->roundTime.read().count();

	// Give everyone a chance to take an initial position
	if (timeSinceOpen().count() < maxBlockTime_)
		return;

	//Here deal with abnormal case:other peers may not receive the proposal
	if (isLeader(adaptor_.valPublic_) && (result_->roundTime.read().count() / (2 * maxBlockTime_)) == 1)
	{
		result_->position.changePosition(*setID_, result_->position.closeTime(), now_);
		adaptor_.propose(result_->position);
		JLOG(j_.warn()) << "We are leader and reProposing position" ;
	}

	// Nothing to do if we don't have consensus.
	if (!haveConsensus())
		return;

	prevProposers_ = txSetVoted_[*setID_].size();

	phase_ = ConsensusPhase::accepted;
	adaptor_.onAccept(
		*result_,
		previousLedger_,
		closeResolution_,
		rawCloseTimes_,
		mode_.get(),
		getJson(true));
}

template <class Adaptor>
bool
PConsensus<Adaptor>::haveConsensus()
{
	// Must have a stance if we are checking for consensus
	if (!result_)
		return false;

	int agreed = txSetVoted_[*setID_].size();
	int minVal = adaptor_.app_.validators().quorum();
	auto currentFinished = adaptor_.proposersFinished(prevLedgerID_);

	JLOG(j_.debug()) << "Checking for TX consensus: agree=" << agreed;
	JLOG(j_.debug()) << "Checking for TX consensus: currentFinished=" << currentFinished;

	// Determine if we actually have consensus or not
	if (agreed >= minVal)
	{
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
template <class Adaptor>
bool PConsensus<Adaptor>::shouldPack()
{
	return isLeader(adaptor_.valPublic_);
}

template <class Adaptor>
bool PConsensus<Adaptor>::isLeader(PublicKey const& pub)
{
	auto const& validators = adaptor_.app_.validators().validators();
	LedgerIndex currentLedgerIndex = previousLedger_.seq() + 1;
	int view = 0;
	int leader_idx = (view + currentLedgerIndex) % validators.size();
	assert(validators.size() > leader_idx);
	return pub == validators[leader_idx];
}


/** Is final condition reached for proposing.
	We should check:
	1. Is tx-count reached max and minBlockTime reached.
	2. Is maxBlockTime reached.
	3. Is this node the next leader and the above 2 conditions reached.
*/
template <class Adaptor>
bool PConsensus<Adaptor>::finalCondReached(uint64 sinceLastClose)
{
	if (transactions_.size() >= maxTxsInLedger_/2 && sinceLastClose >= minBlockTime_)
		return true;
	if (sinceLastClose >= maxBlockTime_)
		return true;
	return false;
}

template <class Adaptor>
Json::Value
PConsensus<Adaptor>::getJson(bool full) const
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
		ret["synched"] = false;

	ret["phase"] = to_string(phase_);
	if (phase_ == ConsensusPhase::open)
	{
		ret["transaction_count"] = static_cast<int>(transactions_.size());
	}

	if (full)
	{
		if (result_)
			ret["current_ms"] =
			static_cast<Int>(result_->roundTime.read().count());
		ret["close_resolution"] = static_cast<Int>(closeResolution_.count());
		//ret["have_time_consensus"] = haveCloseTimeConsensus_;
		ret["previous_proposers"] = static_cast<Int>(prevProposers_);

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

// Handle a change in the prior ledger during a consensus round
template <class Adaptor>
void
PConsensus<Adaptor>::handleWrongLedger(typename Ledger_t::ID const& lgrId)
{
	assert(lgrId != prevLedgerID_ || previousLedger_.id() != lgrId);

	// Stop proposing because we are out of sync
	leaveConsensus();

	// First time switching to this ledger
	if (prevLedgerID_ != lgrId)
	{
		prevLedgerID_ = lgrId;

		//// Clear out state
		//if (result_)
		//{
		//	result_->disputes.clear();
		//	result_->compares.clear();
		//}

		//currPeerPositions_.clear();
		rawCloseTimes_.peers.clear();
		//deadNodes_.clear();

		//// Get back in sync, this will also recreate disputes
		//playbackProposals();
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

template <class Adaptor>
void
PConsensus<Adaptor>::leaveConsensus()
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

template <class Adaptor>
void
PConsensus<Adaptor>::checkLedger()
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

}
#endif