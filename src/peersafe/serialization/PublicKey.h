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

#ifndef RIPPLE_SERIALIZATION_PUBLICKEY_H
#define RIPPLE_SERIALIZATION_PUBLICKEY_H

#include <peersafe/serialization/Serialization.h>
#include <ripple/protocol/PublicKey.h>

#include <ripple/basics/Slice.h>

namespace ripple {

boost::archive::text_oarchive&
operator<<(boost::archive::text_oarchive& os, ripple::PublicKey const& pk) {
	std::string s((const char*)pk.data(), pk.size());
	os << s;
	return os;
}

boost::archive::text_iarchive&
operator>>(boost::archive::text_iarchive& is, ripple::PublicKey & pk) {
	std::string s;
	is >> s;
	pk = ripple::PublicKey(ripple::Slice(s.data(), s.size()));
	return is;
}

template<class Archive>
void serialize(
    Archive& ar,
    ripple::PublicKey & pk,
    const unsigned int /*version*/) {
    ar & pk;
}

} // namespace ripple

#endif // RIPPLE_SERIALIZATION_PUBLICKEY_H