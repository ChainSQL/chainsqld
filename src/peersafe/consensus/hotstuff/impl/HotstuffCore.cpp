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

#include <functional>

#include <peersafe/consensus/hotstuff/impl/HotstuffCore.h>

namespace ripple { namespace hotstuff {

HotstuffCore::HotstuffCore(const beast::Journal& journal, EpochState* epoch_state)
: safety_data_()
, journal_(journal)
, epoch_state_(epoch_state) {
}

HotstuffCore::~HotstuffCore() {
}

Block HotstuffCore::SignProposal(const BlockData& proposal) {
	// verify author
	if (VerifyAuthor(proposal.author()) == false) {
		JLOG(journal_.error())
			<< "InvalideProposal: "
			<< "miss match author " << proposal.author();
		return Block::empty();
	}
	// verify epoch
	if (VerifyEpoch(proposal.epoch) == false) {
		JLOG(journal_.error())
			<< "InvalideProposal: "
			<< "miss match epoch" << proposal.epoch;
		return Block::empty();
	}

	if (proposal.round <= safety_data_.last_voted_round) {
		JLOG(journal_.error())
			<< "InvalideProposal: "
			<< "Proposal round " << proposal.round
			<< " is not higher that last voted round " << safety_data_.last_voted_round;
		return Block::empty();
	}

	// verify qc
	if (VerifyQC(proposal.quorum_cert) == false) {
		JLOG(journal_.error())
			<< "InvalideProposal: "
			<< "miss match qc";
		return Block::empty();
	}
	
	return Block::new_from_block_data(proposal, epoch_state_->verifier);
}

bool HotstuffCore::VerifyAuthor(const Author& author) {
	return true;
}

bool HotstuffCore::VerifyEpoch(const Epoch epoch) {
	return epoch_state_->epoch == epoch;
}

bool HotstuffCore::VerifyQC(const QuorumCertificate& qc) {
	const QuorumCertificate::Signatures& signatures = qc.signatures();
	VoteData voteData = qc.vote_data();
	BlockHash hash = voteData.hash();
	for (auto it = signatures.begin(); it != signatures.end(); it++) {
		if (epoch_state_->verifier->verifySignature(
			it->first,
			it->second,
			ripple::Slice(hash.data(), hash.size())) == false)
			return false;
	}
	return true;
}

} // namespace hotstuff
} // namespace ripple