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

namespace ripple {
namespace hotstuff {

class QuorumCertificate {
public:
	using Signatures = std::map<Author, Signature>;

	QuorumCertificate();
	~QuorumCertificate();

	const VoteData& vote_data() const {
		return vote_data_;
	}

	Signatures& signatures() {
		return signatures_;
	}

	const Signatures& signatures() const {
		return signatures_;
	}

	const BlockInfo& certified_block() const {
		return vote_data_.proposed();
	}

	const BlockInfo& parent_block() const {
		return vote_data_.parent();
	}

private:
	VoteData vote_data_;
	Signatures signatures_;
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_QuorumCert_H