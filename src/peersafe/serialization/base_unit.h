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

#ifndef RIPPLE_SERIALIZATION_BASE_UNIT_H
#define RIPPLE_SERIALIZATION_BASE_UNIT_H

#include <peersafe/serialization/Serialization.h>

#include <ripple/basics/base_uint.h>

namespace ripple {
// serialize uint256
template<class Archive>
void save(
    Archive & ar, 
    const ripple::base_uint<256>& h,
    unsigned int /*version*/) {
	std::string id((const char*)h.data(), h.size());
	ar & id;
}

// deserialize uint256
template<class Archive>
void load(
    Archive & ar, 
    ripple::base_uint<256>& h, 
    unsigned int /*version*/) {
	std::string id;
	ar & id;
	h = ripple::uint256::fromVoid(id.data());
}
RIPPE_SERIALIZATION_SPLIT_FREE(ripple::base_uint<256>);

} // namespace ripple

#endif // RIPPLE_SERIALIZATION_BASE_UNIT_H