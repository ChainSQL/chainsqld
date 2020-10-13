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

#ifndef RIPPLE_CONSENSUS_HOTSTUFF_SYNCINFO_H
#define RIPPLE_CONSENSUS_HOTSTUFF_SYNCINFO_H

#include <boost/optional.hpp>

#include <peersafe/consensus/hotstuff/impl/QuorumCert.h>

namespace ripple {
namespace hotstuff {

class SyncInfo 
{
public:
	SyncInfo(
		const QuorumCertificate& highest_qc_cert, 
		const QuorumCertificate& highest_commit_cert)
	: highest_quorum_cert_(highest_qc_cert)
	, highest_commit_cert_(highest_commit_cert) {

	}

	const Round HighestRound() const {
		return highest_quorum_cert_.certified_block().round;
	}

	const QuorumCertificate& HighestQuorumCert() const {
		return highest_quorum_cert_;
	}

	const QuorumCertificate HighestCommitCert() const {
		if (highest_commit_cert_)
			return highest_commit_cert_.get();
		return highest_quorum_cert_;
	}

	const bool hasNewerCertificate(const SyncInfo& sync_info) const {
		if (highest_quorum_cert_.certified_block().round > sync_info.highest_quorum_cert_.certified_block().round
			|| HighestCommitCert().certified_block().round > sync_info.HighestCommitCert().certified_block().round)
			return true;
		return false;
	}

	bool Verify(const ValidatorVerifier* validator);

private:
	QuorumCertificate highest_quorum_cert_;
	boost::optional<QuorumCertificate> highest_commit_cert_;
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_SYNCINFO_H