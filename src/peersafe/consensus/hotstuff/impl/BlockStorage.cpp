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


BlockStorage::BlockStorage(
	beast::Journal journal,
	StateCompute* state_compute) 
: journal_(journal)
, state_compute_(state_compute)
, genesis_block_id_()
, cache_blocks_mutex_()
, cache_blocks_()
, quorum_cert_mutex_()
, highest_quorum_cert_()
, highest_commit_cert_()
, highest_timeout_cert_()
, committed_round_(0)
, sync_executed_block_handler_(nullptr) {

}

BlockStorage::BlockStorage(
	beast::Journal journal,
	StateCompute* state_compute,
	const Block& genesis_block)
: journal_(journal)
, state_compute_(state_compute)
, genesis_block_id_()
, cache_blocks_mutex_()
, cache_blocks_()
, quorum_cert_mutex_()
, highest_quorum_cert_()
, highest_commit_cert_()
, highest_timeout_cert_() 
, committed_round_(0)
, sync_executed_block_handler_(nullptr) {
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
	{
		std::lock_guard<std::mutex> lock(cache_blocks_mutex_);
		if (blockOf(block.id(), executed_block)) {
			return executed_block;
		}
	}

	if (state_compute_->compute(block, executed_block.state_compute_result)) {
		executed_block.block = block;
		std::lock_guard<std::mutex> lock(cache_blocks_mutex_);
		cache_blocks_.emplace(std::make_pair(block.id(), executed_block));
        JLOG(debugLog().info()) << "store block: " << block.id();
	}

	return executed_block;
}

bool BlockStorage::safetyBlockOf(
	const HashValue& hash, 
	ExecutedBlock& block) {
	std::lock_guard<std::mutex> lock(cache_blocks_mutex_);
	return blockOf(hash, block);
}

// 通过 block hash 获取 block，如果本地没有函数返�?false
bool BlockStorage::blockOf(const HashValue& hash, ExecutedBlock& block) const {
    auto it = cache_blocks_.find(hash);
    if(it == cache_blocks_.end()) {
        return false;
    }
    
    block = it->second;
    return true;
}

bool BlockStorage::exepectBlock(
	const HashValue& hash, 
	const Author& author,
	ExecutedBlock& block) {

	std::lock_guard<std::mutex> lock(cache_blocks_mutex_);

	if (blockOf(hash, block))
		return true;

	if (state_compute_->syncBlock(hash, author, block)) {
		cache_blocks_.emplace(std::make_pair(block.block.id(), block));
        JLOG(debugLog().info()) << "Store an executed block after sync: " << hash;
		handleSyncBlockResult(hash, block);
		return true;
	}
	return false;
}

int BlockStorage::addCerts(
	const SyncInfo& sync_info, 
	const Author& author,
	NetWork* network) {
	insertQuorumCert(sync_info.HighestCommitCert(), author, network);
	insertQuorumCert(sync_info.HighestQuorumCert(), author, network);
	return 0;
}

int BlockStorage::insertQuorumCert(
	const QuorumCertificate& quorumCert, 
	const Author& author,
	NetWork* network) {
	ExecutedBlock executed_block;
	HashValue expected_block_id = const_cast<BlockInfo&>(quorumCert.certified_block()).id;

	if (exepectBlock(expected_block_id, author, executed_block) == false)
		return 1;

	{
		std::lock_guard<std::mutex> lock(quorum_cert_mutex_);
		if (executed_block.block.block_data().round > HighestQuorumCert().certified_block().round) {
			highest_quorum_cert_ = quorumCert;
		}

		if (highest_commit_cert_.ledger_info().ledger_info.commit_info.round < quorumCert.ledger_info().ledger_info.commit_info.round) {
			highest_commit_cert_ = quorumCert;
		}
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
	std::lock_guard<std::mutex> lock(quorum_cert_mutex_);
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

void BlockStorage::handleSyncBlockResult(
	const HashValue& hash, 
	const ExecutedBlock& block) {
	if (sync_executed_block_handler_)
		sync_executed_block_handler_(hash, block);
}

void BlockStorage::gcBlocks(Epoch epoch, Round round) {
	{
		std::lock_guard<std::mutex> lock(cache_blocks_mutex_);
		// remove all blocks which are older epoch than current epoch
		for (auto it = cache_blocks_.begin(); it != cache_blocks_.end();) {
			if (it->second.block.block_data().epoch < epoch) {
				it = cache_blocks_.erase(it);
			}
			else {
				it++;
			}
		}
	}

	Round end_round = round - 10;
	if (end_round <= 0)
		return;

	Round begin_round = round - 10 - 50;
	if (begin_round < 0)
		return;

	{
		std::lock_guard<std::mutex> lock(cache_blocks_mutex_);
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
}

} // hotstuff
} // ripple