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
		beast::Journal journal,
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
	int ProcessProposal(const Block& proposal, const Round& shift = 0);

	int ProcessVote(const Vote& vote, const SyncInfo& sync_info, const Round& shift = 0);

	// Get an expected block
	bool expectBlock(
		const HashValue& block_id,
		ExecutedBlock& executed_block);
private:
	friend class ripple::test::Hotstuff_test;
	friend class Hotstuff;

	int ProcessVote(const Vote& vote, const Round& shift = 0);
	bool ExecuteAndVote(const Block& proposal, Vote& vote);
	int ProcessNewRoundEvent(const NewRoundEvent& event, const Round& shift);
	boost::optional<Block> GenerateProposal(const NewRoundEvent& event);
	bool EnsureRoundAndSyncUp(
		Round round,
		const SyncInfo& sync_info,
		const Author& author);
	int SyncUp(
		const SyncInfo& sync_info,
		const Author& author);
	int ProcessCertificates(const Round& shift = 0);
	int NewQCAggregated(
		const QuorumCertificate& quorumCert,
		const Author& author,
		const Round& shift);
	int NewTCAggregated(const TimeoutCertificate& timeoutCert);
	
	void ProcessLocalTimeout(const boost::system::error_code& ec, Round round);
	void UseNilBlockProcessLocalTimeout(const Round& round);
	void NotUseNilBlockProcessLocalTimeout(const Round& round);
	bool IsValidProposer(const Author& author, const Round& round);
	bool IsValidProposal(const Block& proposal, const Round& shift = 0);

	void HandleSyncBlockResult(const HashValue& hash, const ExecutedBlock& block);

	bool unsafetyExpectBlock(
		const HashValue& block_id,
		ExecutedBlock& executed_block);

	beast::Journal journal_;
	Config config_;
	BlockStorage* block_store_;
	RoundState* round_state_;
	HotstuffCore* hotstuff_core_;
	ProposalGenerator* proposal_generator_;
	ProposerElection* proposer_election_;
	NetWork* network_;

	bool stop_;
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_ROUND_MANAGER_H