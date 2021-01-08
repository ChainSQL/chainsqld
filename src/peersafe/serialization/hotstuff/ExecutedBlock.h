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

#ifndef RIPPLE_SERIALIZATION_HOTSTUFF_EXECUTEDBLOCK_H
#define RIPPLE_SERIALIZATION_HOTSTUFF_EXECUTEDBLOCK_H

#include <peersafe/serialization/Serialization.h>
#include <peersafe/serialization/hotstuff/Block.h>
#include <peersafe/serialization/hotstuff/StateCompute.h>

#include <peersafe/consensus/hotstuff/impl/ExecuteBlock.h>

namespace ripple { namespace hotstuff {

template<class Archive>
void serialize(
    Archive& ar, 
    ripple::hotstuff::ExecutedBlock& executed_block, 
    const unsigned int /*version*/) {
    ar & executed_block.block;
    ar & executed_block.state_compute_result;
}

} // namespace serialization
} // namespace ripple

#endif // RIPPLE_SERIALIZATION_HOTSTUFF_EXECUTEDBLOCK_H