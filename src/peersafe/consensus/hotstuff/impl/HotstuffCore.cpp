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
#include <peersafe/consensus/hotstuff/impl/BlockStorage.h>

namespace ripple { namespace hotstuff {

HotstuffCore::HotstuffCore(
	const beast::Journal& journal,
	StateCompute* state_compute,
	EpochState* epoch_state)
: safety_data_()
, journal_(journal)
, state_compute_(state_compute)
, epoch_state_(epoch_state) {
}

HotstuffCore::~HotstuffCore() {
}

void HotstuffCore::Initialize(Epoch epoch, Round round) {
	safety_data_.epoch = epoch;
	safety_data_.last_voted_round = round;
	safety_data_.preferred_round = round;
	safety_data_.last_vote = boost::optional<Vote>();
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

bool HotstuffCore::ConstructAndSignVote(const ExecutedBlock& executed_block, Vote& vote) {
	const Block& proposed_block = executed_block.block;
	if (VerifyEpoch(proposed_block.block_data().epoch) == false) {
		JLOG(journal_.error())
			<< "Construct And Signed vote:"
			<< "miss epoch";
		return false;
	}

	if (VerifyQC(proposed_block.block_data().quorum_cert) == false) {
		JLOG(journal_.error())
			<< "Construct And Signed vote:"
			<< "invalid quorum certificate";
		return false;
	}
	
	HashValue id = proposed_block.id();

	if (proposed_block.signature()
		&& epoch_state_->verifier->verifySignature(
			proposed_block.block_data().author(),
			proposed_block.signature().get(),
			id) == false) {
		JLOG(journal_.error())
			<< "Construct And Signed vote:"
			<< "invalid block' signature";
		return false;
	}

	if (VerifyAndUpdatePreferredRound(proposed_block.block_data().quorum_cert) == false)
		return false;
	// if already voted on this round, send back the previous vote.
	if (safety_data_.last_vote) {
		if (safety_data_.last_vote->vote_data().proposed().round == proposed_block.block_data().round) {
			safety_data_.last_voted_round = proposed_block.block_data().round;
			vote = safety_data_.last_vote.get();
			return true;
		}
	}

	if (VerifyAndUpdateLastVoteRound(proposed_block.block_data().round) == false)
		return false;

	auto ret = ExtensionCheck(executed_block);
	if (std::get<0>(ret) == false)
		return false;
	VoteData vote_data = std::get<1>(ret);
	LedgerInfoWithSignatures::LedgerInfo ledger_info = ConstructLedgerInfo(proposed_block);

	Signature signature;
	HashValue hash = vote_data.hash();
	if (epoch_state_->verifier->signature(hash, signature) == false)
		return false;

	ledger_info.consensus_data_hash = hash;
	vote = Vote::New(epoch_state_->verifier->Self(), vote_data, ledger_info, signature);

	safety_data_.last_vote = vote;
	return true;
}

bool HotstuffCore::VerifyAuthor(const Author& author) {
	return author == epoch_state_->verifier->Self();
}

bool HotstuffCore::VerifyEpoch(const Epoch epoch) {
	return epoch_state_->epoch == epoch;
}

bool HotstuffCore::VerifyQC(const QuorumCertificate& qc) {
	const QuorumCertificate::Signatures& signatures = qc.signatures();
	VoteData voteData = qc.vote_data();
	HashValue hash = voteData.hash();
	for (auto it = signatures.begin(); it != signatures.end(); it++) {
		if (epoch_state_->verifier->verifySignature(
			it->first,
			it->second,
			hash) == false)
			return false;
	}
	return true;
}

bool HotstuffCore::VerifyAndUpdatePreferredRound(const QuorumCertificate& qc) {
	Round preferred_round = safety_data_.preferred_round;
	Round one_chain_round = qc.certified_block().round;
	Round two_chain_round = qc.parent_block().round;

	if (one_chain_round < preferred_round) {
		JLOG(journal_.error())
			<< "Incorrect Preferred round:"
			<< "one chain round " << one_chain_round
			<< " is less than preferred round " << preferred_round;
		return false;
	}

	if (two_chain_round > preferred_round) {
		safety_data_.preferred_round = two_chain_round;
	}
	else if (two_chain_round < preferred_round) {
		JLOG(journal_.info())
			<< "2-chain round " << two_chain_round
			<< " is lower than preferred round " << preferred_round
			<< " but 1-chain round " << one_chain_round
			<< " is higher.";
	}

	return true;
}

bool HotstuffCore::VerifyAndUpdateLastVoteRound(Round round) {
	Round last_voted_round = safety_data_.last_voted_round;
	if (round > last_voted_round) {
		safety_data_.last_voted_round = round;
	}
	else {
		JLOG(journal_.error())
			<< "Incorrect last voted round: "
			<< "last voted round is " << last_voted_round
			<< " but current round is " << round;
		return false;
	}
	return true;
}

std::tuple<bool, VoteData> HotstuffCore::ExtensionCheck(const ExecutedBlock& executed_block) {
	Block proposed_block = executed_block.block;
	if (state_compute_->verify(executed_block.state_compute_result) == false) {
		JLOG(journal_.error())
			<< "extension check falled."
			<< "Round is " << executed_block.block.block_data().round;
		HashValue zero_hash = HashValue();
		BlockInfo empyt_block_info(zero_hash);
		return std::make_tuple(false, VoteData::New(empyt_block_info, empyt_block_info));
	}

	return std::make_tuple(
		true,
		VoteData::New(
			proposed_block.gen_block_info(
				executed_block.state_compute_result.ledger_info, 
				executed_block.state_compute_result.epoch_state),
			proposed_block.block_data().quorum_cert.certified_block())
	);
}

/// Produces a LedgerInfo that either commits a block based upon the 3-chain
/// commit rule or an empty LedgerInfo for no commit. The 3-chain commit rule is: B0 and its
/// prefixes can be committed if there exist certified blocks B1 and B2 that satisfy:
/// 1) B0 <- B1 <- B2 <--
/// 2) round(B0) + 1 = round(B1), and
/// 3) round(B1) + 1 = round(B2).
LedgerInfoWithSignatures::LedgerInfo 
HotstuffCore::ConstructLedgerInfo(const Block& proposed_block) {
	Round block2 = proposed_block.block_data().round;
	Round block1 = proposed_block.block_data().quorum_cert.certified_block().round;
	Round block0 = proposed_block.block_data().quorum_cert.parent_block().round;
	
	bool commit = ((block0 + 1) == block1 && (block1 + 1) == block2);
	HashValue zero_hash;
	LedgerInfoWithSignatures::LedgerInfo ledger_info{ BlockInfo(zero_hash), zero_hash };
	if (commit) {
		ledger_info.commit_info = proposed_block.block_data().quorum_cert.parent_block();
	}
	
	return ledger_info;
}

} // namespace hotstuff
} // namespace ripple