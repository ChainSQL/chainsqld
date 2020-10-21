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

#ifndef RIPPLE_CONSENSUS_HOTSTUFF_EXECUTEBLOCK_H
#define RIPPLE_CONSENSUS_HOTSTUFF_EXECUTEBLOCK_H


#include <peersafe/consensus/hotstuff/impl/StateCompute.h>

namespace ripple {
namespace hotstuff {


struct ExecutedBlock {
	Block block;
	StateComputeResult state_compute_result;

	ExecutedBlock() 
	: block(Block::empty())
	, state_compute_result() {
	}
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_EXECUTEBLOCK_H