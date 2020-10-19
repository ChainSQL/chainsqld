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

#ifndef RIPPLE_CONSENSUS_HOTSTUFF_VOTE_H
#define RIPPLE_CONSENSUS_HOTSTUFF_VOTE_H

#include <boost/optional.hpp>

#include <peersafe/consensus/hotstuff/impl/Block.h>
#include <peersafe/consensus/hotstuff/impl/VoteData.h>
#include <peersafe/consensus/hotstuff/impl/timeout.h>

namespace ripple {
namespace hotstuff {

class Vote { 
public:
	Vote();
	~Vote();

	static Vote New(
		const Author author,
		const VoteData& vote_data,
		const LedgerInfoWithSignatures::LedgerInfo& ledger_info,
		const Signature& signature);

	VoteData& vote_data() {
		return vote_data_;
	}

	const VoteData& vote_data() const {
		return vote_data_;
	}

	Author& author() {
		return author_;
	}

	const Author& author() const {
		return author_;
	}

	Signature& signature() {
		return signature_;
	}

	const Signature& signature() const {
		return signature_;
	}

	void addTimeoutSignature(const Signature& signature) {
		timeout_signature_ = boost::optional<Signature>(signature);
	}

	boost::optional<Signature>& timeout_signature() {
		return timeout_signature_;
	}

	const boost::optional<Signature>& timeout_signature() const {
		return timeout_signature_;
	}

	const LedgerInfoWithSignatures::LedgerInfo& ledger_info() const {
		return ledger_info_;
	}

	const bool isTimeout() const;
	Timeout timeout() const;
private:
	/// The data of the vote
	VoteData vote_data_;
	/// The identity of the voter.
	Author author_;
	/// LedgerInfo of a block that is going to be committed in case this vote gathers QC.
	LedgerInfoWithSignatures::LedgerInfo ledger_info_;
	/// Signature of the LedgerInfo
	Signature signature_;
	/// The round signatures can be aggregated into a timeout certificate if present.
	boost::optional<Signature> timeout_signature_;
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_VOTE_H