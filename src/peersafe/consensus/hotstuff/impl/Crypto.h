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

#ifndef RIPPLE_CONSENSUS_HOTSTUFF_CRYPTO_H
#define RIPPLE_CONSENSUS_HOTSTUFF_CRYPTO_H

#include <map>

#include <peersafe/consensus/hotstuff/impl/Types.h>

namespace ripple { namespace hotstuff {

struct PartialSig {
	ReplicaID ID;
	Signature sig;
};

class PartialCert {
public:
	PartialCert();
	~PartialCert();

	struct PartialSig partialSig;
	BlockHash blockHash;
};

class QuorumCert {
public:
	using key = ReplicaID;
	using value = PartialSig;

	QuorumCert();
	QuorumCert(const BlockHash& hash);
	~QuorumCert();

	bool addPartiSig(const PartialCert& cert);
	std::size_t sizeOfSig() const;
	const BlockHash& hash() const;
	const std::map<key, value>& sigs() const;

	ripple::Blob toBytes() const;

private:
	std::map<key, value> sigs_;
	BlockHash blockHash_;
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_CRYPTO_H