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

#include <peersafe/consensus/hotstuff/impl/QuorumCert.h>

namespace ripple {
namespace hotstuff {

//////////////////////////////////////////////////////////////////////////////////
// class LedgerInfoWithSignatures
//////////////////////////////////////////////////////////////////////////////////

void LedgerInfoWithSignatures::addSignature(const Author& author, const Signature& signature) {
	signatures[author] = signature;
}

bool LedgerInfoWithSignatures::Verify(const ValidatorVerifier* validator) {
	return validator->verifyLedgerInfo(
		ledger_info.commit_info, 
		ledger_info.consensus_data_hash, 
		signatures);
}

//////////////////////////////////////////////////////////////////////////////////
// class QuorumCertificate
//////////////////////////////////////////////////////////////////////////////////

QuorumCertificate::QuorumCertificate()
: vote_data_(VoteData::New(BlockInfo(ZeroHash()), BlockInfo(ZeroHash())))
, signed_ledger_info_(LedgerInfoWithSignatures::LedgerInfo{BlockInfo(ZeroHash()), ZeroHash()}) {

}

QuorumCertificate::QuorumCertificate(
	const VoteData& vote_data,
	const LedgerInfoWithSignatures& signed_ledger_info)
: vote_data_(vote_data)
, signed_ledger_info_(signed_ledger_info) {

}

QuorumCertificate::~QuorumCertificate() {

}

bool QuorumCertificate::Verify(const ValidatorVerifier* validator) {
	HashValue vote_hash = vote_data_.hash();
	if (ledger_info().ledger_info.consensus_data_hash != vote_hash)
		return false;

	// Genesis's QC is implicitly agreed upon, it doesn't have real signatures.
	// If someone sends us a QC on a fake genesis, it'll fail to insert into BlockStore
	// because of the round constraint.
	if (certified_block().round == 0) {
		if (const_cast<BlockInfo&>(parent_block()).id != const_cast<BlockInfo&>(certified_block()).id) {
			std::cerr
				<< "Genesis QC has inconsistent parent block with certified block"
				<< std::endl;
			return false;
		}

		if (const_cast<BlockInfo&>(certified_block()).id != ledger_info().ledger_info.commit_info.id) {
			std::cerr
				<< "Genesis QC has inconsistent commit block with certified block"
				<< std::endl;
			return false;
		}
		
		if (ledger_info().signatures.empty() == false) {
			std::cerr
				<< "Genesis QC should not carry signatures"
				<< std::endl;
			return false;
		}
		return true;
	}

	if (ledger_info().Verify(validator) == false)
		return false;

	if (vote_data_.Verify() == false) {
		return false;
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////
// TimeoutCertificate
///////////////////////////////////////////////////////////////////////////////////////

TimeoutCertificate::TimeoutCertificate(const Timeout& timeout)
: timeout_(timeout)
, signatures_() {

}

TimeoutCertificate::~TimeoutCertificate() {

}

void TimeoutCertificate::addSignature(const Author& author, const Signature& signature) {
	if (signatures_.find(author) == signatures_.end()) {
		signatures_.emplace(std::make_pair(author, signature));
	}
}

bool TimeoutCertificate::Verify(const ValidatorVerifier* validator) {
	if (signatures_.empty()) {
		assert(signatures_.empty() == false);
		return false;
	}

	using beast::hash_append;
	ripple::sha512_half_hasher h;
	hash_append(h, timeout_.epoch);
	hash_append(h, timeout_.round);
	HashValue hash = static_cast<typename sha512_half_hasher::result_type>(h);
	ripple::Slice message(hash.data(), hash.size());

	for (auto it = signatures_.begin(); it != signatures_.end(); it++) {
		if (validator->verifySignature(it->first, it->second, message) == false)
			return false;
	}
	return true;
}

} // namespace hotstuff
} // namespace ripple