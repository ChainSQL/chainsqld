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

#ifndef RIPPLE_CONSENSUS_HOTSTUFF_TIMEOUT_H
#define RIPPLE_CONSENSUS_HOTSTUFF_TIMEOUT_H

#include <peersafe/consensus/hotstuff/impl/Types.h>
#include <peersafe/consensus/hotstuff/impl/ValidatorVerifier.h>

namespace ripple {
namespace hotstuff {

struct Timeout {
	Epoch epoch;
	Round round;

	bool sign(ValidatorVerifier* verifier, Signature& signature) {
		return verifier->signature(hash(), signature);
	}

	bool verify(
		const ValidatorVerifier* verifier, 
		const Author& author, 
		const Signature& signature) {
		return verifier->verifySignature(author, signature, hash());
	}

private:
	HashValue hash() {
		using beast::hash_append;
		ripple::sha512_half_hasher h;
		hash_append(h, epoch);
		hash_append(h, round);

		return static_cast<typename sha512_half_hasher::result_type>(h);
	}
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_TIMEOUT_H