//------------------------------------------------------------------------------
/*
 This file is part of chainsqld: https://github.com/chainsql/chainsqld
 Copyright (c) 2016-2018 Peersafe Technology Co., Ltd.
 
	chainsqld is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.
 
	chainsqld is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
 */
//==============================================================================

#ifndef PEERSAFE_HOTSTUFF_CONSENSUS_H_INCLUDE
#define PEERSAFE_HOTSTUFF_CONSENSUS_H_INCLUDE

#include <vector>

#include <peersafe/consensus/ConsensusBase.h>
#include <peersafe/consensus/hotstuff/HotstuffAdaptor.h>
#include <peersafe/consensus/hotstuff/impl/RoundManager.h>

#include <ripple/basics/Log.h>
#include <ripple/core/JobQueue.h>

namespace ripple { namespace hotstuff {

//class Sender {
//public:
//    virtual ~Sender() {}
//
//    virtual void proposal(const ReplicaID& id, const Block& block) = 0;
//    //virtual void vote(const ReplicaID& id, const PartialCert& cert) = 0;
//    //virtual void newView(const ReplicaID& id, const QuorumCert& qc) = 0;
//protected:
//    Sender() {}
//};

//struct Config {
//    // self id
//    ReplicaID id;
//    // change a new leader per view_change
//    int view_change;
//    // schedule for electing a new leader
//    std::vector<ReplicaID> leader_schedule;
//    // generate a dummy block after timeout (seconds)
//    int timeout;
//    // commands batch size
//    int cmd_batch_size;
//
//    Config()
//    : id(0)
//    , view_change(1)
//    , leader_schedule()
//    , timeout(60)
//    , cmd_batch_size(100) {
//
//    }
//};

class Hotstuff final
    : public ripple::ConsensusBase
    , public StateCompute
    , public CommandManager
    , public ValidatorVerifier
{
public:
    Hotstuff(ripple::Adaptor& adaptor, clock_type const& clock, beast::Journal j);

    ~Hotstuff();

public:

    void startRound(
        NetClock::time_point const& now,
        typename Ledger_t::ID const& prevLedgerID,
        Ledger_t prevLedger,
        hash_set<NodeID> const& nowUntrusted,
        bool proposing) override final;

    /** Call periodically to drive consensus forward.

        @param now The network adjusted time
    */
    void timerEntry(NetClock::time_point const& now) override final;

    bool peerConsensusMessage(
        std::shared_ptr<PeerImp>& peer,
        bool isTrusted,
        std::shared_ptr<protocol::TMConsensus> const& m) override final;

    /** Process a transaction set acquired from the network

        @param now The network adjusted time
        @param txSet the transaction set
    */
    void gotTxSet(NetClock::time_point const& now, TxSet_t const& txSet) override final;

    /** Get the Json state of the consensus process.

        Called by the consensus_info RPC.

        @param full True if verbose response desired.
        @return     The Json state.
    */
    Json::Value getJson(bool full) const override final;

    // Overwrite CommandManager extract interface.
    void extract(ripple::hotstuff::Command& cmd) override final;

    // Overwrite StateCompute interfaces.
    bool compute(const Block& block, ripple::LedgerInfo& ledger_info) override final;
    bool verify(const ripple::LedgerInfo& ledger_info, const ripple::LedgerInfo& parent_ledger_info) override final;
    int commit(const Block& block) override final;

    // Overwrite ValidatorVerifier interfaces.
    const Author& Self() const override final;
    bool signature(const ripple::Slice& message, Signature& signature) override final;
    bool verifySignature(const Author& author, const Signature& signature, const ripple::Slice& message) override final;
    const bool verifyLedgerInfo(
        const BlockInfo& commit_info,
        const HashValue& consensus_data_hash,
        const std::map<Author, Signature>& signatures) const override final;
    const bool checkVotingPower(const std::map<Author, Signature>& signatures) const override final;

private:
    int handleProposal(
        const ripple::hotstuff::Block& proposal,
        const ripple::hotstuff::SyncInfo& sync_info);

    int handleVote(
        const ripple::hotstuff::Vote& vote,
        const ripple::hotstuff::SyncInfo& sync_info);

private:
    ripple::HotstuffAdaptor& adaptor_;

    VoteData init_vote_data_;
    LedgerInfoWithSignatures init_ledgerinfo_;
    BlockStorage storage_;
    EpochState epoch_state_;
    RoundState round_state_;
    ProposalGenerator proposal_generator_;

	HotstuffCore hotstuff_core_;
	RoundManager* round_manager_;
};

} // namespace hotstuff
} // namespace ripple

#endif // PEERSAFE_HOTSTUFF_CONSENSUS_H_INCLUDE