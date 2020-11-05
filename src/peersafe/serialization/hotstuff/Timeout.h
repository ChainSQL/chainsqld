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

#ifndef RIPPLE_SERIALIZATION_HOTSTUFF_TIMEOUT_H
#define RIPPLE_SERIALIZATION_HOTSTUFF_TIMEOUT_H

#include <peersafe/serialization/Serialization.h>
#include <peersafe/serialization/hotstuff/BlockInfo.h>
#include <peersafe/serialization/hotstuff/VoteData.h>

#include <peersafe/consensus/hotstuff/impl/timeout.h>

namespace ripple { namespace hotstuff {

template<class Archive>
void serialize(
    Archive& ar, 
    Timeout& timeout, 
    const unsigned int /*version*/) {
	// note, version is always the latest when saving
	ar & timeout.epoch;
	ar & timeout.round;
	ar & timeout.offset;
}

} // namespace serialization
} // namespace ripple

#endif // RIPPLE_SERIALIZATION_HOTSTUFF_TIMEOUT_H