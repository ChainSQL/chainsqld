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

#ifndef RIPPLE_SERIALIZATION_H
#define RIPPLE_SERIALIZATION_H

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include <boost/serialization/split_free.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/optional.hpp>

#include <ripple/basics/Buffer.h>

namespace ripple { namespace serialization {

template<class T>
ripple::Buffer serialize(const T& t) {
	std::ostringstream os;
	boost::archive::text_oarchive oa(os);

	oa << t;

	std::string s = os.str();
	return ripple::Buffer((const void*)s.data(), s.size());
}

template<class T>
T deserialize(const ripple::Buffer& serilization) {
	std::string s((const char*)serilization.data(), serilization.size());
	std::istringstream is(s);
	boost::archive::text_iarchive ia(is);
	T t;
	ia >> t;
	return t;
}

#define	RIPPE_SERIALIZATION_SPLIT_FREE(T)       \
template<class Archive>                         \
inline void serialize(                          \
        Archive & ar,                               \
        T & t,                                      \
        const unsigned int file_version             \
) {                                              \
        boost::serialization::split_free(ar, t, file_version);            \
}

} // namespace serialization
} // namespace ripple

#endif // RIPPLE_SERIALIZATION_H