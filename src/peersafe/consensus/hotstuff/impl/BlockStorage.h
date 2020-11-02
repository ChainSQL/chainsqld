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

	// Get an expected block safely
	bool safetyBlockOf(const HashValue& hash, ExecutedBlock& block);

    // 閫氳繃 block hash 鑾峰彇 block锛屽鏋滄湰鍦伴渶瑕佷粠缃戠粶鍚屾
    bool exepectBlock(const HashValue& hash, ExecutedBlock& block) const;

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

	int addCerts(const SyncInfo& sync_info, NetWork* network);

	int insertQuorumCert(const QuorumCertificate& quorumCeret, NetWork* network);
	int insertTimeoutCert(const TimeoutCertificate& timeoutCeret);

	// 目前主要功能是 commit 共识过的 block
	//int saveVote(const Vote& vote);
	StateCompute* state_compute() {
		return state_compute_;
	}

private:
    // 閫氳繃 block hash 鑾峰彇 block锛屽鏋滄湰鍦版病鏈夊嚱鏁拌繑鍥?false
    bool blockOf(const HashValue& hash, ExecutedBlock& block) const;
	void commit(const LedgerInfoWithSignatures& ledger_info_with_sigs);
	void gcBlocks(Epoch epoch, Round round);
    //void recurseGCBlocks(const Block& block);

	beast::Journal journal_;
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