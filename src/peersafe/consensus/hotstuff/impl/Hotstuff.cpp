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

#include <peersafe/consensus/hotstuff/Hotstuff.h>
#include <peersafe/consensus/hotstuff/impl/Block.h>

namespace ripple { namespace hotstuff {

Hotstuff::Hotstuff(
	boost::asio::io_service& ios,
    const beast::Journal& journal,
	const Author& author,
	CommandManager* cm,
	ProposerElection* proposer_election,
	StateCompute* state_compute,
	ValidatorVerifier* verifier,
	NetWork* network)
: init_vote_data_(VoteData::New(BlockInfo(ZeroHash()), BlockInfo(ZeroHash())))
, init_ledgerinfo_(LedgerInfoWithSignatures::LedgerInfo{ init_vote_data_.proposed(), init_vote_data_.hash() })
, storage_(
	state_compute, 
	QuorumCertificate(init_vote_data_, init_ledgerinfo_),
	QuorumCertificate(init_vote_data_, init_ledgerinfo_))
, epoch_state_()
, round_state_(&ios)
, proposal_generator_(cm, &storage_, author)
, hotstuff_core_(journal, state_compute, &epoch_state_)
, round_manager_(nullptr) {

	epoch_state_.epoch = 0;
	epoch_state_.verifier = verifier;

	round_manager_ = new RoundManager(
		&storage_, 
		&round_state_, 
		&hotstuff_core_,
		&proposal_generator_,
		proposer_election,
		network);
}

Hotstuff::~Hotstuff() {
    delete round_manager_;
}

int Hotstuff::start() {
	return round_manager_->start();
}

int Hotstuff::handleProposal(
	const ripple::hotstuff::Block& proposal,
	const ripple::hotstuff::SyncInfo& sync_info) {
	return round_manager_->ProcessProposal(proposal, sync_info);
}

int Hotstuff::handleVote(
	const ripple::hotstuff::Vote& vote,
	const ripple::hotstuff::SyncInfo& sync_info) {
	return round_manager_->ProcessVote(vote, sync_info);
}


} // namespace hotstuff
} // namespace ripple