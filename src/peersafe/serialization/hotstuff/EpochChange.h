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

#ifndef RIPPLE_SERIALIZATION_HOTSTUFF_EPOCHCHANGE_H
#define RIPPLE_SERIALIZATION_HOTSTUFF_EPOCHCHANGE_H

#include <peersafe/serialization/Serialization.h>
#include <peersafe/serialization/Buffer.h>
#include <peersafe/serialization/hotstuff/QuorumCert.h>

#include <peersafe/consensus/hotstuff/impl/EpochChange.h>

namespace ripple { namespace hotstuff {

template<class Archive>
void serialize(
    Archive& ar, 
    ripple::hotstuff::EpochChange& epoch_change, 
    const unsigned int /*version*/) {
	// note, version is always the latest when saving
	ar & epoch_change.ledger_info;
	ar & epoch_change.author;
	ar & epoch_change.epoch;
	ar & epoch_change.round;
	ar & epoch_change.signature;
}

} // namespace serialization
} // namespace ripple

#endif // RIPPLE_SERIALIZATION_HOTSTUFF_BLOCKINFO_H