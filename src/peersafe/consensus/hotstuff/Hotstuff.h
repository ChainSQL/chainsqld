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

namespace ripple { 

// only for test case
namespace test {
	class Hotstuff_test;
} // namespace test

namespace hotstuff {

class Hotstuff {
public:
	using pointer = std::shared_ptr<Hotstuff>;

	class Builder {
	public:
		Builder(
			boost::asio::io_service& ios,
			const beast::Journal& journal);

		Builder& setConfig(const Config& config);
		Builder& setCommandManager(CommandManager* cm);
		Builder& setProposerElection(ProposerElection* proposer_election);
		Builder& setStateCompute(StateCompute* state_compute);
		Builder& setNetWork(NetWork* network);

		Hotstuff::pointer build();

	private:
		boost::asio::io_service* io_service_;
		beast::Journal journal_;
		Config config_;
		CommandManager* command_manager_;
		ProposerElection* proposer_election_;
		StateCompute* state_compute_;
		NetWork* network_;
	};

    ~Hotstuff();

	int start(const RecoverData& recover_data);
	void stop();
	
	bool CheckProposal(
		const ripple::hotstuff::Block& proposal,
		const ripple::hotstuff::SyncInfo& sync_info);
	int handleProposal(const ripple::hotstuff::Block& proposal);

	int handleVote(
		const ripple::hotstuff::Vote& vote,
		const ripple::hotstuff::SyncInfo& sync_info);

private:
    Hotstuff(
		boost::asio::io_service& ios,
        const beast::Journal& journal,
		const Config config,
		CommandManager* cm,
		ProposerElection* proposer_election,
		StateCompute* state_compute,
		NetWork* network);

	friend class ripple::test::Hotstuff_test;
	
	beast::Journal journal_;
	Config config_;
	BlockStorage storage_;
	EpochState epoch_state_;
	RoundState round_state_;
	ProposalGenerator proposal_generator_;
	ProposerElection* proposer_election_;
	NetWork* network_;
	HotstuffCore hotstuff_core_;
	RoundManager* round_manager_;
};

} // namespace hotstuff
} // namespace ripple

#endif // PEERSAFE_HOTSTUFF_CONSENSUS_H_INCLUDE