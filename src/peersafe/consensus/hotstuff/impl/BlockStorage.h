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

#include <map>

#include <ripple/ledger/ReadView.h>

#include <peersafe/consensus/hotstuff/impl/HotstuffCore.h>
#include <peersafe/consensus/hotstuff/impl/Block.h>
#include <peersafe/consensus/hotstuff/impl/BlockStorage.h>
#include <peersafe/consensus/hotstuff/impl/SyncInfo.h>

namespace ripple { namespace hotstuff {

struct ExecutedBlock {
	Block block;
	ripple::LedgerInfo state_compute_result;

	ExecutedBlock() 
	: block(Block::empty())
	, state_compute_result() {
	}
};

class StateCompute;

class BlockStorage {
public:
    //BlockStorage();
	BlockStorage(
		StateCompute* state_compute,
		const QuorumCertificate& highest_quorum_cert, 
		const QuorumCertificate& highest_commit_cert);
	BlockStorage(
		StateCompute* state_compute,
		const Block& genesis_block);
    ~BlockStorage();

    // for blocks
    //bool addBlock(const Block& block);
	ExecutedBlock executeAndAddBlock(const Block& block);
    // 基于区块高度释放不再需要用到的区块
    //void gcBlocks(const Block& block);
    // 通过 block hash 获取 block，如果本地没有函数返回 false
    bool blockOf(const HashValue& hash, ExecutedBlock& block) const;
    // 通过 block hash 获取 block, 如果本地没有则需要从网络同步
    bool expectBlock(const HashValue& hash, ExecutedBlock& block);

	const QuorumCertificate& HighestQuorumCert() const {
		return highest_quorum_cert_;
	}

	const QuorumCertificate& HighestCommitCert() const {
		return highest_commit_cert_;
	}

	SyncInfo sync_info() {
		return SyncInfo(HighestQuorumCert(), HighestCommitCert());
	}

	int addCerts(const SyncInfo& sync_info);

	int insertQuorumCert(const QuorumCertificate& quorumCeret);

	// 目前主要功能是 commit 共识过的 block
	int saveVote(const Vote& vote);

private:
    //void recurseGCBlocks(const Block& block);
	StateCompute* state_compute_;
    std::map<HashValue, ExecutedBlock> cache_blocks_;

	QuorumCertificate highest_quorum_cert_;
	QuorumCertificate highest_commit_cert_;
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_BLOCKSTORAGE_H