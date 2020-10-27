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

#include <map>

#include <boost/optional.hpp>

#include <ripple/ledger/ReadView.h>

#include <peersafe/consensus/hotstuff/impl/Types.h>
#include <peersafe/consensus/hotstuff/impl/BlockInfo.h>

namespace ripple {
namespace hotstuff {

class ValidatorVerifier {
public:
    virtual ~ValidatorVerifier() {};
    
	//virtual boost::optional<PublicKey> getPublicKey(const Author& author) = 0;
	virtual const Author& Self() const = 0;
	virtual bool signature(const uint256& digest, Signature& signature) = 0;
	virtual const bool verifySignature(
		const Author& author, 
		const Signature& signature, 
		const uint256& digest) const = 0;
	virtual const bool verifyLedgerInfo(
		const BlockInfo& commit_info,
		const HashValue& consensus_data_hash,
		const std::map<Author,Signature>& signatures) = 0;

	virtual const bool checkVotingPower(const std::map<Author, Signature>& signatures) const = 0;

protected:
    ValidatorVerifier(){}
};    
    
} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_VALIDATOR_VERIFIER_H