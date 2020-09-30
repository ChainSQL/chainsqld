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

#ifndef RIPPLE_CONSENSUS_HOTSTUFF_VALIDATOR_VERIFIER_H
#define RIPPLE_CONSENSUS_HOTSTUFF_VALIDATOR_VERIFIER_H

#include <boost/optional.hpp>

#include <peersafe/consensus/hotstuff/impl/Types.h>

namespace ripple {
namespace hotstuff {

class ValidatorVerifier {
public:
    virtual ~ValidatorVerifier() {};
    
	//virtual boost::optional<PublicKey> getPublicKey(const Author& author) = 0;
	virtual bool signature(const Author& author, const ripple::Slice& message, Signature& signature) = 0;
	virtual bool verifySignature(const Author& author, const Signature& signature, const ripple::Slice& message) = 0;

protected:
    ValidatorVerifier(){}
};    
    
} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_VALIDATOR_VERIFIER_H