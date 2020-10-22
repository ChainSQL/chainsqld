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

#ifndef RIPPLE_CONSENSUS_HOTSTUFF_SERIALIZATION_H
#define RIPPLE_CONSENSUS_HOTSTUFF_SERIALIZATION_H

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include <boost/serialization/split_free.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/optional.hpp>

#include <ripple/ledger/ReadView.h>

#include <ripple/basics/Buffer.h>
#include <ripple/basics/base_uint.h>

namespace ripple {
//namespace hotstuff {

class Serialization {
public:
	template<class T>
	static ripple::Buffer serialize(const T& t) {
		std::ostringstream os;
		boost::archive::text_oarchive oa(os);

		oa << t;

		std::string s = os.str();
		return ripple::Buffer((const void*)s.data(), s.size());
	}

	template<class T>
	static T deserialize(const ripple::Buffer& serilization) {
		std::string s((const char*)serilization.data(), serilization.size());
		std::istringstream is(s);
		boost::archive::text_iarchive ia(is);
		T t;
		ia >> t;
		return t;
	}
};

#define	RIPPE_SERIALIZATION_SPLIT_FREE(T)       \
template<class Archive>                         \
inline void serialize(                          \
        Archive & ar,                               \
        T & t,                                      \
        const unsigned int file_version             \
) {                                              \
        boost::serialization::split_free(ar, t, file_version);            \
}

// serialize uint256
template<class Archive>
void save(Archive & ar, const ripple::base_uint<256>& h, unsigned int /*version*/) {
	std::string id((const char*)h.data(), h.size());
	ar & id;
}

// deserialize uint256
template<class Archive>
void load(Archive & ar, ripple::base_uint<256>& h, unsigned int /*version*/) {
	std::string id;
	ar & id;
	h = ripple::uint256::fromVoid(id.data());
}
RIPPE_SERIALIZATION_SPLIT_FREE(ripple::base_uint<256>);

// serialize & deserialize LedgerInfo 
template<class Archive>
void serialize(Archive& ar, ripple::LedgerInfo& ledger_info, const unsigned int /*version*/) {
	ar & ledger_info.seq;
	ar & ledger_info.hash;
	ar & ledger_info.txHash;
	ar & ledger_info.accountHash;
	ar & ledger_info.parentHash;
}

// serialize ripple::Buffer
template<class Archive>
void save(Archive & ar, const ripple::Buffer& buffer, unsigned int /*version*/) {
	std::string buf((const char*)buffer.data(), buffer.size());
	ar & buf;
}

// deserialize ripple::Buffer
template<class Archive>
void load(Archive & ar, ripple::Buffer& buffer, unsigned int /*version*/) {
	std::string buf;
	ar & buf;
	buffer = ripple::Buffer(buf.data(), buf.size());
}
RIPPE_SERIALIZATION_SPLIT_FREE(ripple::Buffer);

//} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_SERIALIZATION_H