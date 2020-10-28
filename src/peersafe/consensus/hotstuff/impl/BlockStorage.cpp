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

#include <algorithm>

#include <peersafe/consensus/hotstuff/impl/BlockStorage.h>
#include <peersafe/consensus/hotstuff/impl/StateCompute.h>
#include <peersafe/consensus/hotstuff/impl/EpochChange.h>
#include <peersafe/consensus/hotstuff/impl/NetWork.h>


namespace ripple { namespace hotstuff {

BlockStorage::BlockStorage(StateCompute* state_compute) 
: state_compute_(state_compute)
, genesis_block_id_()
, cache_blocks_()
, highest_quorum_cert_()
, highest_commit_cert_()
, highest_timeout_cert_()
, committed_round_(0) {

}

BlockStorage::BlockStorage(
	StateCompute* state_compute,
	const Block& genesis_block)
: state_compute_(state_compute)
, genesis_block_id_()
, cache_blocks_()
, highest_quorum_cert_()
, highest_commit_cert_()
, highest_timeout_cert_() 
, committed_round_(0) {
	updateCeritificates(genesis_block);
}

BlockStorage::~BlockStorage() {
}

void BlockStorage::updateCeritificates(const Block& block) {
	committed_round_ = block.block_data().round;
	genesis_block_id_  = block.id();
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
		executed_block.state_compute_result.parent_ledger_info = block.block_data().quorum_cert.certified_block().ledger_info;
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

int BlockStorage::addCerts(const SyncInfo& sync_info, NetWork* network) {
	insertQuorumCert(sync_info.HighestCommitCert(), network);
	insertQuorumCert(sync_info.HighestQuorumCert(), network);
	return 0;
}

int BlockStorage::insertQuorumCert(const QuorumCertificate& quorumCert, NetWork* network) {
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
	
	Round commtiting_round = quorumCert.ledger_info().ledger_info.commit_info.round;
	if (committed_round_ < commtiting_round) {
		commit(quorumCert.ledger_info());

		if (quorumCert.endsEpoch()) {
			EpochChange epoch_change;
			epoch_change.ledger_info = quorumCert.ledger_info().ledger_info.commit_info.ledger_info;
			epoch_change.epoch = quorumCert.ledger_info().ledger_info.commit_info.next_epoch_state->epoch;
			network->broadcast(epoch_change);
		}

		gcBlocks(
			quorumCert.ledger_info().ledger_info.commit_info.epoch,
			quorumCert.ledger_info().ledger_info.commit_info.round);
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

void BlockStorage::commit(const LedgerInfoWithSignatures& ledger_info_with_sigs) {
	HashValue block_hash = ledger_info_with_sigs.ledger_info.commit_info.id;
	if (block_hash.isZero() 
		&& ledger_info_with_sigs.ledger_info.commit_info.empty() == false) {
		// handle genesis block info
		block_hash = genesis_block_id_;
	}

	if (block_hash.isZero())
		return;

	ExecutedBlock executed_block;
	if (blockOf(block_hash, executed_block)) {
		state_compute_->commit(executed_block.block);
		committed_round_ = executed_block.block.block_data().round;
	}
}

void BlockStorage::gcBlocks(Epoch epoch, Round round) {
	// remove all blocks which are older epoch than current epoch
	for (auto it = cache_blocks_.begin(); it != cache_blocks_.end();) {
		if (it->second.block.block_data().epoch < epoch) {
			it = cache_blocks_.erase(it);
		}
		else {
			it++;
		}
	}

	Round end_round = round - 10;
	if (end_round <= 0)
		return;

	Round begin_round = round - 10 - 50;
	if (begin_round < 0)
		return;

	for (Round r = begin_round; r != end_round; r++) {
		for (auto it = cache_blocks_.begin(); it != cache_blocks_.end();) {
			if (it->second.block.block_data().epoch == epoch
				&& it->second.block.block_data().round == r) {
				it = cache_blocks_.erase(it);
				break;
			}
			else {
				it++;
			}
		}
	}
}

} // hotstuff
} // ripple