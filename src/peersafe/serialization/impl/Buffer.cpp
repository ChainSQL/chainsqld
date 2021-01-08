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


#include <peersafe/serialization/Buffer.h>

namespace ripple {

boost::archive::text_oarchive&
operator<<(boost::archive::text_oarchive& os, ripple::Buffer const& buffer) {
	std::string s((const char*)buffer.data(), buffer.size());
	os << s;
	return os;
}

boost::archive::text_iarchive&
operator>>(boost::archive::text_iarchive& is, ripple::Buffer& buffer) {
	std::string s;
	is >> s;
	buffer = ripple::Buffer(s.data(), s.size());
	return is;
}


} // namespace ripple

