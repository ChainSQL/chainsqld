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

#ifndef RIPPLE_CONSENSUS_HOTSTUFF_ROUND_MANAGER_H
#define RIPPLE_CONSENSUS_HOTSTUFF_ROUND_MANAGER_H

#include <peersafe/consensus/hotstuff/impl/Config.h>
#include <peersafe/consensus/hotstuff/impl/HotstuffCore.h>
#include <peersafe/consensus/hotstuff/impl/RecoverData.h>
#include <peersafe/consensus/hotstuff/impl/RoundState.h>
#include <peersafe/consensus/hotstuff/impl/ProposerElection.h>
#include <peersafe/consensus/hotstuff/impl/ProposalGenerator.h>
#include <peersafe/consensus/hotstuff/impl/StateCompute.h>
#include <peersafe/consensus/hotstuff/impl/NetWork.h>

namespace ripple {
// only for test case
namespace test {
	class Hotstuff_test;
} // namespace test
namespace hotstuff {

class Hotstuff;
class RoundManager {
public:
	RoundManager(
		const beast::Journal& journal,
		const Config& config,
		BlockStorage* block_store,
		RoundState* round_state,
		HotstuffCore* hotstuff_core,
		ProposalGenerator* proposal_generator,
		ProposerElection* proposer_election,
		NetWork* network);
	~RoundManager();
	
	int start();
	void stop();

	bool CheckProposal(const Block& proposal, const SyncInfo& sync_info);
	int ProcessProposal(const Block& proposal);

	int ProcessVote(const Vote& vote, const SyncInfo& sync_info);
private:
	friend class ripple::test::Hotstuff_test;
	int ProcessVote(const Vote& vote);
	bool ExecuteAndVote(const Block& proposal, Vote& vote);
	int ProcessNewRoundEvent(const NewRoundEvent& event);
	boost::optional<Block> GenerateProposal(const NewRoundEvent& event);
	bool EnsureRoundAndSyncUp(
		Round round,
		const SyncInfo& sync_info,
		const Author& author);
	int SyncUp(
		const SyncInfo& sync_info,
		const Author& author);
	int ProcessCertificates();
	int NewQCAggregated(const QuorumCertificate& quorumCert);
	int NewTCAggregated(const TimeoutCertificate& timeoutCert);
	
	void ProcessLocalTimeout(const boost::system::error_code& ec, Round round);
	void UseNilBlockProcessLocalTimeout(const Round& round);
	void NotUseNilBlockProcessLocalTimeout(const Round& round);
	bool IsValidProposer(const Author& author, const Round& round);
	bool IsValidProposal(const Block& proposal);
	void ShiftCurrentRoundToNextHeader();
	bool HasShiftCurrentRoundToNextHeader();
	void ResetShiftCurrentRoundToCurrentHeader();

	const beast::Journal& journal_;
	Config config_;
	BlockStorage* block_store_;
	RoundState* round_state_;
	HotstuffCore* hotstuff_core_;
	ProposalGenerator* proposal_generator_;
	ProposerElection* proposer_election_;
	NetWork* network_;

	bool stop_;
	
	// 加入当前 current_round 的 leader 没有出块或是异常
	// 下一个 next_round = current_round + 1 的 leader 可以接管此轮次 current_round
	Round shift_round_to_next_leader_;
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_ROUND_MANAGER_H