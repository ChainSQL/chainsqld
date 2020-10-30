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

#ifndef RIPPLE_SERIALIZATION_NETCLOCK_H
#define RIPPLE_SERIALIZATION_NETCLOCK_H

#include <peersafe/serialization/Serialization.h>
#include <ripple/basics/chrono.h>

namespace ripple {

template<class Archive>
void save(
    Archive & ar, 
    const ripple::NetClock::time_point& time_point, 
    unsigned int /*version*/) {
	uint32_t tm = time_point.time_since_epoch().count();
	ar & tm;
}

template<class Archive>
void load(
    Archive & ar, 
    ripple::NetClock::time_point& time_point, 
    unsigned int /*version*/) {
	uint32_t tm;
	ar & tm;
	time_point = NetClock::time_point{ NetClock::duration{tm}};
}
RIPPE_SERIALIZATION_SPLIT_FREE(ripple::NetClock::time_point);

template<class Archive>
void save(
	Archive & ar,
	const ripple::NetClock::duration& duration,
	unsigned int /*version*/) {
	uint32_t d = duration.count();
	ar & d;
}

template<class Archive>
void load(
	Archive & ar,
	ripple::NetClock::duration& duration,
	unsigned int /*version*/) {
	uint32_t d;
	ar & d;
	duration = ripple::NetClock::duration{ d };
}
RIPPE_SERIALIZATION_SPLIT_FREE(ripple::NetClock::duration);

} // namespace ripple

#endif // RIPPLE_SERIALIZATION_NETCLOCK_H