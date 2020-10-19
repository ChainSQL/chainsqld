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
//=============================================================================


#include <peersafe/consensus/hotstuff/impl/BlockStorage.h>
#include <peersafe/consensus/hotstuff/impl/StateCompute.h>


namespace ripple { namespace hotstuff {

BlockStorage::BlockStorage(StateCompute* state_compute) 
: state_compute_(state_compute)
, genesis_block_id_()
, cache_blocks_()
, highest_quorum_cert_()
, highest_commit_cert_()
, highest_timeout_cert_() {

}

BlockStorage::BlockStorage(
	StateCompute* state_compute,
	const Block& genesis_block)
: state_compute_(state_compute)
, genesis_block_id_()
, cache_blocks_()
, highest_quorum_cert_()
, highest_commit_cert_()
, highest_timeout_cert_() {
	updateCeritificates(genesis_block);
}

BlockStorage::~BlockStorage() {
}

void BlockStorage::updateCeritificates(const Block& block) {
	genesis_block_id_ = block.id();
	highest_quorum_cert_ = QuorumCertificate(block.block_data().quorum_cert);
	highest_commit_cert_  = QuorumCertificate(block.block_data().quorum_cert);
	executeAndAddBlock(block);
}

ExecutedBlock BlockStorage::executeAndAddBlock(const Block& block) {
	ExecutedBlock executed_block;
	if (blockOf(block.id(), executed_block)) {
		return executed_block;
	}

	if (state_compute_->compute(block, executed_block.state_compute_result)) {
		executed_block.block = block;
		cache_blocks_.emplace(std::make_pair(block.id(), executed_block));
	}

	return executed_block;
}

// 通过 block hash 获取 block，如果本地没有函数返回 false
bool BlockStorage::blockOf(const HashValue& hash, ExecutedBlock& block) const {
    auto it = cache_blocks_.find(hash);
    if(it == cache_blocks_.end()) {
        return false;
    }
    
    block = it->second;
    return true;
}

// 通过 block hash 获取 block, 如果本地没有则需要从网络同步
bool BlockStorage::expectBlock(const HashValue& hash, ExecutedBlock& block) {
    if(blockOf(hash, block))
        return true;
    return false;
}

int BlockStorage::addCerts(const SyncInfo& sync_info) {
	highest_quorum_cert_ = sync_info.HighestQuorumCert();
	if (highest_commit_cert_.certified_block().round < sync_info.HighestCommitCert().certified_block().round)
		highest_commit_cert_ = sync_info.HighestCommitCert();
	return 0;
}

int BlockStorage::insertQuorumCert(const QuorumCertificate& quorumCert) {
	ExecutedBlock executed_block;
	HashValue expected_block_id = const_cast<BlockInfo&>(quorumCert.certified_block()).id;
	if (blockOf(expected_block_id, executed_block) == false)
		return 1;

	if (executed_block.block.block_data().round > HighestQuorumCert().certified_block().round) {
		highest_quorum_cert_ = quorumCert;
	}

	if (highest_commit_cert_.ledger_info().ledger_info.commit_info.round < quorumCert.ledger_info().ledger_info.commit_info.round) {
		highest_commit_cert_ = quorumCert;
	}

	return 0;
}

int BlockStorage::insertTimeoutCert(const TimeoutCertificate& timeoutCeret) {
	if (highest_timeout_cert_) {
		if (highest_timeout_cert_->timeout().round < timeoutCeret.timeout().round) {
			highest_timeout_cert_ = timeoutCeret;
		}
	} else {
		highest_timeout_cert_ = timeoutCeret;
	}
	return 0;
}

int BlockStorage::saveVote(const Vote& vote) {
	HashValue block_hash = vote.ledger_info().commit_info.id;

	if (block_hash.isZero() 
		&& vote.ledger_info().commit_info.empty() == false) {
		// handle genesis block info
		block_hash = genesis_block_id_;
	}

	if (block_hash.isZero())
		return 1;

	ExecutedBlock executed_block;
	if (blockOf(block_hash, executed_block)) {
			state_compute_->commit(executed_block.block);
	}
	return 0;
}

} // hotstuff
} // ripple