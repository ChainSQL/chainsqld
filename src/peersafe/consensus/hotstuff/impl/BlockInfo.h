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

#include <ripple/ledger/ReadView.h>

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
	HashValue id;
	/// ledger info
	ripple::LedgerInfo ledger_info;
	/// The version of the latest transaction after executing this block.
	Version version;
	/// The timestamp this block was proposed by a proposer.
	int64_t timestamp_usecs;
	/// An optional field containing the next epoch info
	boost::optional<EpochState> next_epoch_state;

	BlockInfo(const HashValue& block_hash)
	: epoch(0)
	, round(0)
	, id(block_hash)
	, ledger_info()
	, version(0)
	, timestamp_usecs(0)
	, next_epoch_state() {

	}

	const bool empty() const {
		return id.isZero()
			&& ledger_info.txHash.isZero()
			&& ledger_info.hash.isZero()
			&& ledger_info.accountHash.isZero()
			&& ledger_info.parentHash.isZero()
			&& ledger_info.seq == 0;
	}

	const bool hasReconfiguration() const {
		if (next_epoch_state)
			return true;
		return false;
	}

//private:
	//friend class ripple::Serialization;
	// only for serialization
	BlockInfo() {}
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_BLOCKINFO_H