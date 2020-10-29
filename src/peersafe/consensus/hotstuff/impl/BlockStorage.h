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

class StateCompute;
class NetWork;

class BlockStorage {
public:
    //BlockStorage();
	BlockStorage(
		const beast::Journal& journal,
		StateCompute* state_compute);
	BlockStorage(
		const beast::Journal& journal,
		StateCompute* state_compute,
		const Block& genesis_block);
    ~BlockStorage();

	void updateCeritificates(const Block& block);

    // for blocks
    //bool addBlock(const Block& block);
	ExecutedBlock executeAndAddBlock(const Block& block);
    // 通过 block hash 获取 block，如果本地没有函数返回 false
    bool blockOf(const HashValue& hash, ExecutedBlock& block) const;

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
			journal_,
			HighestQuorumCert(), 
			HighestCommitCert(), 
			HighestTimeoutCert());
	}

	int addCerts(const SyncInfo& sync_info, NetWork* network);

	int insertQuorumCert(const QuorumCertificate& quorumCeret, NetWork* network);
	int insertTimeoutCert(const TimeoutCertificate& timeoutCeret);

	// 目前主要功能是 commit 共识过的 block
	//int saveVote(const Vote& vote);
	StateCompute* state_compute() {
		return state_compute_;
	}

private:
	void commit(const LedgerInfoWithSignatures& ledger_info_with_sigs);
	void gcBlocks(Epoch epoch, Round round);
    //void recurseGCBlocks(const Block& block);

	const beast::Journal* journal_;
	StateCompute* state_compute_;
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