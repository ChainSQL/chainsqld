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

template <class Adaptor>
class PConsensus
{
	using Ledger_t = typename Adaptor::Ledger_t;
	using TxSet_t = typename Adaptor::TxSet_t;
	using NodeID_t = typename Adaptor::NodeID_t;
	using Tx_t = typename TxSet_t::Tx;
	using PeerPosition_t = typename Adaptor::PeerPosition_t;
	using Proposal_t = ConsensusProposal<
		NodeID_t,
		typename Ledger_t::ID,
		typename TxSet_t::ID>;

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
private:
	void
		startRoundInternal(
			NetClock::time_point const& now,
			typename Ledger_t::ID const& prevLedgerID,
			Ledger_t const& prevLedger,
			ConsensusMode mode);

	// Change our view of the previous ledger
	void
		handleWrongLedger(typename Ledger_t::ID const& lgrId);

	/** Check if our previous ledger matches the network's.

		If the previous ledger differs, we are no longer in sync with
		the network and need to bow out/switch modes.
	*/
	void
		checkLedger();

	/** If we radically changed our consensus context for some reason,
		we need to replay recent proposals so that they're not lost.
	*/
	void
		playbackProposals();

	// Revoke our outstanding proposal, if any, and cease proposing
	// until this round ends.
	void
		leaveConsensus();

	/** Call periodically to drive consensus forward.

		@param now The network adjusted time
	*/
	void
		timerEntry(NetClock::time_point const& now);
private:
	Adaptor& adaptor_;

	ConsensusPhase phase_{ ConsensusPhase::accepted };
	MonitoredMode mode_{ ConsensusMode::observing };

	clock_type const& clock_;

	//-------------------------------------------------------------------------
	// Network time measurements of consensus progress

	// The current network adjusted time.  This is the network time the
	// ledger would close if it closed now
	NetClock::time_point now_;
	NetClock::time_point prevCloseTime_;

	//-------------------------------------------------------------------------
	// Non-peer (self) consensus data

	// Last validated ledger ID provided to consensus
	typename Ledger_t::ID prevLedgerID_;
	// Last validated ledger seen by consensus
	Ledger_t previousLedger_;

	// The number of proposers who participated in the last consensus round
	std::size_t prevProposers_ = 0;

	// nodes that have bowed out of this consensus process
	hash_set<NodeID_t> deadNodes_;

	// Journal for debugging
	beast::Journal j_;
};

#endif