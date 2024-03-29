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

#ifndef RIPPLE_CONSENSUS_HOTSTUFF_BLOCKSTORAGE_H
#define RIPPLE_CONSENSUS_HOTSTUFF_BLOCKSTORAGE_H

#include <atomic>
#include <mutex>
#include <map>

#include <peersafe/consensus/hotstuff/impl/HotstuffCore.h>
#include <peersafe/consensus/hotstuff/impl/ExecuteBlock.h>
#include <peersafe/consensus/hotstuff/impl/SyncInfo.h>

namespace ripple { namespace hotstuff {

class ValidatorVerifier;
class ProposerElection;
class StateCompute;
class NetWork;

class BlockStorage {
public:
	using AsyncBlockHandler = StateCompute::AsyncCompletedHander;

    //BlockStorage();
	BlockStorage(
		beast::Journal journal,
		StateCompute* state_compute);
	BlockStorage(
		beast::Journal journal,
		StateCompute* state_compute,
		const Block& genesis_block);
    ~BlockStorage();

	void updateCeritificates(const Block& block);

    // for blocks
    //bool addBlock(const Block& block);
	ExecutedBlock executeAndAddBlock(const Block& block);
	
	// add an executed block
	void addExecutedBlock(const ExecutedBlock& executed_block);

	bool existsBlock(const HashValue& hash);
	// Get an expected block safely from local
	bool safetyBlockOf(const HashValue& hash, ExecutedBlock& block);
	// Get an executed block by hash, 
	// if it dosen't exists in local then return directly
    bool blockOf(const HashValue& hash, ExecutedBlock& block) const;
	// Get an executed block by hash,if dosen't exists in local 
	// then getting it from network
    bool expectBlock(
		const HashValue& hash, 
		const Author& author,
		ExecutedBlock& block);

	// Get an executed block by hash from network 
	// and then process the block in `handler`
	void asyncExpectBlock(
		const HashValue& hash,
		const Author& author,
		AsyncBlockHandler handler);

	const QuorumCertificate& HighestQuorumCert() const {
		return highest_quorum_cert_;
	}

	const QuorumCertificate& HighestCommitCert() const {
		return highest_commit_cert_;
	}

	const boost::optional<TimeoutCertificate>& HighestTimeoutCert() const {
		return highest_timeout_cert_;
	}

	SyncInfo sync_info() {
		std::lock_guard<std::mutex> lock(quorum_cert_mutex_);
		return SyncInfo(
			HighestQuorumCert(), 
			HighestCommitCert(), 
			HighestTimeoutCert());
	}

	int addCerts(
		const SyncInfo& sync_info, 
		const Author& author,
		NetWork* network);

	int insertQuorumCert(
		const QuorumCertificate& quorumCeret, 
		const Author& author,
		NetWork* network);
	int insertTimeoutCert(const TimeoutCertificate& timeoutCeret);

	// 目前主要功能是 commit 共识过的 block
	//int saveVote(const Vote& vote);
	StateCompute* state_compute() {
		return state_compute_;
	}

	void verifier(ValidatorVerifier* verifier) {
		verifier_ = verifier;
	}

	void proposerElection(ProposerElection* proposer_election) {
		proposer_election_ = proposer_election;
	}
	
private:
	void updateQuorumCert(const Round round, const QuorumCertificate& quorumCert);
	void preCommit(const QuorumCertificate& quorumCert, NetWork* network);
	void commit(const LedgerInfoWithSignatures& ledger_info_with_sigs);
	void gcBlocks(Epoch epoch, Round round);

	beast::Journal journal_;
	StateCompute* state_compute_;
	ValidatorVerifier* verifier_;
	ProposerElection* proposer_election_;
	HashValue genesis_block_id_;
	std::mutex cache_blocks_mutex_;
    std::map<HashValue, ExecutedBlock> cache_blocks_;

	std::mutex quorum_cert_mutex_;
	QuorumCertificate highest_quorum_cert_;
	QuorumCertificate highest_commit_cert_;
	boost::optional<TimeoutCertificate> highest_timeout_cert_;
	std::atomic<Round> committed_round_;
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_BLOCKSTORAGE_H