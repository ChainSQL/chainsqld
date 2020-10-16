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

#ifndef RIPPLE_CONSENSUS_HOTSTUFF_H
#define RIPPLE_CONSENSUS_HOTSTUFF_H

#include <vector>
#include <memory>

#include <peersafe/consensus/hotstuff/impl/RoundManager.h>

#include <ripple/basics/Log.h>
#include <ripple/core/JobQueue.h>

namespace ripple { namespace hotstuff {

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

class Hotstuff {
public:
	using pointer = std::shared_ptr<Hotstuff>;

	class Builder {
	public:
		Builder(
			boost::asio::io_service& ios,
			const beast::Journal& journal);

		Builder& setRecoverData(const RecoverData& recover_data);
		Builder& setAuthor(const Author& author);
		Builder& setCommandManager(CommandManager* cm);
		Builder& setProposerElection(ProposerElection* proposer_election);
		Builder& setStateCompute(StateCompute* state_compute);
		Builder& setValidatorVerifier(ValidatorVerifier* verifier);
		Builder& setNetWork(NetWork* network);

		Hotstuff::pointer build();

	private:
		boost::asio::io_service* io_service_;
		beast::Journal journal_;
		RecoverData recover_data_;
		Author author_;
		CommandManager* command_manager_;
		ProposerElection* proposer_election_;
		StateCompute* state_compute_;
		ValidatorVerifier* verifier_;
		NetWork* network_;
	};

    ~Hotstuff();

    int start();
    void stop();

    int handleProposal(
        const ripple::hotstuff::Block& proposal,
        const ripple::hotstuff::SyncInfo& sync_info);

    int handleVote(
        const ripple::hotstuff::Vote& vote,
        const ripple::hotstuff::SyncInfo& sync_info);

private:
    Hotstuff(
		boost::asio::io_service& ios,
        const beast::Journal& journal,
		const RecoverData& recover_data,
		const Author& author,
		CommandManager* cm,
		ProposerElection* proposer_election,
		StateCompute* state_compute,
		ValidatorVerifier* verifier,
		NetWork* network);

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