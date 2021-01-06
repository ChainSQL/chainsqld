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
//=============================================================================

#include <peersafe/serialization/hotstuff/EpochChange.h>

namespace ripple { namespace hotstuff {

HashValue EpochChange::hash(const EpochChange& epoch_change) {
	assert(epoch_change.signature.size() == 0);
	using beast::hash_append;
	ripple::sha512_half_hasher h;
	ripple::Buffer s = ripple::serialization::serialize(epoch_change);
	hash_append(h, s);
	return static_cast<typename	sha512_half_hasher::result_type>(h);
}

const bool EpochChange::verify(ValidatorVerifier* verifier) const {
	EpochChange epoch_change;
	epoch_change.ledger_info = ledger_info;
	epoch_change.author = author;
	epoch_change.epoch = epoch;
	epoch_change.round = round;

	return verifier->verifySignature(author, signature, EpochChange::hash(epoch_change));
}

} // hotstuff
} // ripple