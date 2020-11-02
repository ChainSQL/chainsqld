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

#ifndef RIPPLE_CONSENSUS_HOTSTUFF_STATECOMPUTE_H
#define RIPPLE_CONSENSUS_HOTSTUFF_STATECOMPUTE_H

#include <boost/optional.hpp>

#include <ripple/ledger/ReadView.h>

#include <peersafe/consensus/hotstuff/impl/Block.h>

namespace ripple {
namespace hotstuff { 

struct StateComputeResult {
	/// ledgen info on application
	ripple::LedgerInfo ledger_info;
	/// parent ledgen info on application
	ripple::LedgerInfo parent_ledger_info;
	/// If set, this is the new epoch info that should be changed to if this block is committed
	boost::optional<EpochState> epoch_state;

	StateComputeResult()
		: ledger_info()
		, parent_ledger_info()
		, epoch_state() {

	}
};

class ExecutedBlock;
class StateCompute {
public:
	virtual ~StateCompute() {}
	
	virtual bool compute(const Block& block, StateComputeResult& state_compute_result) = 0;
	virtual bool verify(const Block& block, const StateComputeResult& state_compute_result) = 0;
	virtual int commit(const Block& block) = 0;
	// sync state
	virtual bool syncState(const BlockInfo& block_info) = 0;
	// sync a block
	virtual bool syncBlock(const HashValue& block_id, ExecutedBlock& executedBlock) = 0;

protected:
	StateCompute() {}
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_STATECOMPUTE_H