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

#ifndef RIPPLE_CONSENSUS_HOTSTUFF_QUORUMCERT_H
#define RIPPLE_CONSENSUS_HOTSTUFF_QUORUMCERT_H

#include <map>

#include <peersafe/consensus/hotstuff/impl/Types.h>
#include <peersafe/consensus/hotstuff/impl/VoteData.h>
#include <peersafe/consensus/hotstuff/impl/BlockInfo.h>
#include <peersafe/consensus/hotstuff/impl/ValidatorVerifier.h>
#include <peersafe/consensus/hotstuff/impl/timeout.h>

namespace ripple {
namespace hotstuff {

struct LedgerInfoWithSignatures {
	using Signatures = std::map<Author, Signature>;

	struct LedgerInfo {
		BlockInfo commit_info;
		HashValue consensus_data_hash;
	} ledger_info;
	Signatures signatures;

	void addSignature(const Author& author, const Signature& signature);
	bool Verify(ValidatorVerifier* validator);

	LedgerInfoWithSignatures(const LedgerInfo& li)
	: ledger_info(li)
	, signatures() {
	}
};

class QuorumCertificate {
public:
	using Signatures = std::map<Author, Signature>;

	QuorumCertificate();
	QuorumCertificate(
		const VoteData& vote_data, 
		const LedgerInfoWithSignatures& signed_ledger_info);
	~QuorumCertificate();

	const VoteData& vote_data() const {
		return vote_data_;
	}

	LedgerInfoWithSignatures::Signatures& signatures() {
		return signed_ledger_info_.signatures;
	}

	const LedgerInfoWithSignatures::Signatures& signatures() const {
		return signed_ledger_info_.signatures;
	}

	const BlockInfo& certified_block() const {
		return vote_data_.proposed();
	}

	BlockInfo& certified_block() {
		return vote_data_.proposed();
	}

	const BlockInfo& parent_block() const {
		return vote_data_.parent();
	}

	BlockInfo& parent_block() {
		return vote_data_.parent();
	}

	const LedgerInfoWithSignatures& ledger_info() const {
		return signed_ledger_info_;
	};

	LedgerInfoWithSignatures& ledger_info() {
		return signed_ledger_info_;
	}

	const bool endsEpoch() const {
		if (signed_ledger_info_.ledger_info.commit_info.next_epoch_state)
			return true;
		return false;
	}

	bool Verify(ValidatorVerifier* validator);

private:
	VoteData vote_data_;
	LedgerInfoWithSignatures signed_ledger_info_;
};

class TimeoutCertificate {
public:
	using Signatures = std::map<Author, Signature>;
	TimeoutCertificate(const Timeout& timeout);
	~TimeoutCertificate();

	const Timeout& timeout() const {
		return timeout_;
	}

	const Signatures& signatures() const {
		return signatures_;
	}

	void addSignature(const Author& author, const Signature& signature);
	bool Verify(const ValidatorVerifier* validator);

private:
	Timeout timeout_;
	Signatures signatures_;
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_QuorumCert_H