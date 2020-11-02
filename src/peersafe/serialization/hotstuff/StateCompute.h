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

#ifndef RIPPLE_SERIALIZATION_HOTSTUFF_STATECOMPUTE_H
#define RIPPLE_SERIALIZATION_HOTSTUFF_STATECOMPUTE_H

#include <peersafe/serialization/Serialization.h>
#include <peersafe/serialization/LedgerInfo.h>
#include <peersafe/serialization/hotstuff/EpochState.h>

#include <peersafe/consensus/hotstuff/impl/StateCompute.h>

namespace ripple { namespace hotstuff {

template<class Archive>
void serialize(
    Archive& ar, 
    ripple::hotstuff::StateComputeResult& state_compute_result, 
    const unsigned int /*version*/) {
    ar & state_compute_result.ledger_info;
    ar & state_compute_result.parent_ledger_info;
    ar & state_compute_result.epoch_state;
}

} // namespace serialization
} // namespace ripple

#endif // RIPPLE_SERIALIZATION_HOTSTUFF_STATECOMPUTE_H