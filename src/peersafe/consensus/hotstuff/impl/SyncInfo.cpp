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

#include <peersafe/consensus/hotstuff/impl/SyncInfo.h>

namespace ripple {
namespace hotstuff {

SyncInfo::SyncInfo(
	const QuorumCertificate& highest_qc_cert,
	const QuorumCertificate& highest_commit_cert,
	const boost::optional<TimeoutCertificate> highest_timeout_cert)
: highest_quorum_cert_(highest_qc_cert)
, highest_commit_cert_()
, highest_timeout_cert_() {
	if (highest_qc_cert.certified_block().round != highest_commit_cert.certified_block().round) {
		highest_commit_cert_ = boost::optional<QuorumCertificate>();
	}
	else {
		highest_commit_cert_ = highest_commit_cert;
	}

	// No need to include HTC if it's lower than HQC
	if (highest_timeout_cert
		&& highest_timeout_cert->timeout().round > highest_qc_cert.certified_block().round) {
		highest_timeout_cert_ = highest_timeout_cert;
	}
}

const Round SyncInfo::HighestRound() const {
	if (highest_timeout_cert_) {
		return std::max(
			highest_quorum_cert_.certified_block().round,
			highest_timeout_cert_->timeout().round);
	}
	return highest_quorum_cert_.certified_block().round;
}

const QuorumCertificate& SyncInfo::HighestQuorumCert() const {
	return highest_quorum_cert_;
}

const QuorumCertificate SyncInfo::HighestCommitCert() const {
	if (highest_commit_cert_)
		return highest_commit_cert_.get();
	return highest_quorum_cert_;
}

const TimeoutCertificate SyncInfo::HighestTimeoutCert() const {
	return highest_timeout_cert_.get_value_or(TimeoutCertificate(Timeout{0, 0}));
}

const bool SyncInfo::hasNewerCertificate(const SyncInfo& sync_info) const {
	if (highest_quorum_cert_.certified_block().round > sync_info.highest_quorum_cert_.certified_block().round
		|| HighestCommitCert().ledger_info().ledger_info.commit_info.round > sync_info.HighestCommitCert().ledger_info().ledger_info.commit_info.round
		|| HighestTimeoutCert().timeout().round > sync_info.HighestTimeoutCert().timeout().round)
		return true;
	return false;
}

bool SyncInfo::Verify(ValidatorVerifier* validator) {
	Epoch epoch = highest_quorum_cert_.certified_block().epoch;
	if (epoch != HighestCommitCert().certified_block().epoch)
		return false;

	if (highest_timeout_cert_
		&& epoch != highest_timeout_cert_->timeout().epoch)
		return false;

	if (highest_quorum_cert_.certified_block().round < HighestCommitCert().certified_block().round)
		return false;

	if (highest_quorum_cert_.Verify(validator) == false)
		return false;

	//if (highest_commit_cert_ && highest_commit_cert_->Verify(validator) == false)
	//	return false;

	if (highest_timeout_cert_ && highest_timeout_cert_->Verify(validator) == false)
		return false;
	
	return true;
}
    
} // namespace hotstuff
} // namespace ripple