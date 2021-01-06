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

#ifndef RIPPLE_SERIALIZATION_HOTSTUFF_VOTE_H
#define RIPPLE_SERIALIZATION_HOTSTUFF_VOTE_H

#include <peersafe/serialization/Serialization.h>
#include <peersafe/serialization/PublicKey.h>
#include <peersafe/serialization/hotstuff/VoteData.h>

#include <peersafe/consensus/hotstuff/impl/Vote.h>

namespace ripple { namespace hotstuff {

template<class Archive>
void save(
    Archive& ar, 
    const ripple::hotstuff::Vote& vote, 
    unsigned int /*version*/) {

	ar & vote.vote_data();
	ar & vote.author();
	ar & vote.ledger_info();
	const Signature& sign = vote.signature();
	std::string s((const char*)sign.data(), sign.size());
	ar & s;
	ar & vote.timeout_signature();
}

template<class Archive>
void load(
    Archive& ar, 
    ripple::hotstuff::Vote& vote, 
    unsigned int /*version*/) {

	ar & vote.vote_data();
	ar & vote.author();
	ar & vote.ledger_info();

	std::string s;
	ar & s;
	Signature sign(s.data(), s.size());
	vote.signature() = sign;
	ar & vote.timeout_signature();
}
RIPPE_SERIALIZATION_SPLIT_FREE(ripple::hotstuff::Vote);

} // namespace serialization
} // namespace ripple

#endif // RIPPLE_SERIALIZATION_HOTSTUFF_VOTE_H