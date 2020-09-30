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

#ifndef RIPPLE_CONSENSUS_HOTSTUFF_BLOCKINFO_H
#define RIPPLE_CONSENSUS_HOTSTUFF_BLOCKINFO_H

#include <boost/optional.hpp>

#include <peersafe/consensus/hotstuff/impl/Types.h>
#include <peersafe/consensus/hotstuff/impl/EpochState.h>

namespace ripple {
namespace hotstuff {

class BlockInfo {
public:
	/// Epoch number corresponds to the set of validators that are active for this block.
	Epoch epoch;
	/// The round of a block is an internal monotonically increasing counter used by Consensus
	/// protocol.
	Round round;
	/// The identifier (hash) of the block.
	BlockHash id;
	/// The accumulator root hash after executing this block.
	BlockHash executed_state_id;
	/// The version of the latest transaction after executing this block.
	Version version;
	/// The timestamp this block was proposed by a proposer.
	int64_t timestamp_usecs;
	/// An optional field containing the next epoch info
	boost::optional<EpochState> next_epoch_state;

	static BlockHash BlockInfo::hash(const BlockInfo& block_info) {
		using beast::hash_append;
		ripple::sha512_half_hasher h;
		hash_append(h, block_info.epoch);
		hash_append(h, block_info.round);
		hash_append(h, block_info.hash);
		hash_append(h, block_info.executed_state_id);
		hash_append(h, block_info.version);
		hash_append(h, block_info.timestamp_usecs);
		return static_cast<typename	sha512_half_hasher::result_type>(h);
	}

	BlockInfo()
	: epoch(0)
	, round(0)
	, id()
	, executed_state_id()
	, version(0)
	, timestamp_usecs(0)
	, next_epoch_state() {

	}
};
    
} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_BLOCKINFO_H