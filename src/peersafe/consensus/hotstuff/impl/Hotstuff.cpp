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

Hotstuff::Builder::Builder(
	boost::asio::io_service& ios,
	const beast::Journal& journal)
: io_service_(&ios)
, journal_(journal)
, recover_data_()
, author_()
, command_manager_(nullptr)
, proposer_election_(nullptr)
, state_compute_(nullptr)
, verifier_(nullptr)
, network_(nullptr) {

}

Hotstuff::Builder& Hotstuff::Builder::setRecoverData(const RecoverData& recover_data) {
	recover_data_ = recover_data;
	return *this;
}

Hotstuff::Builder& Hotstuff::Builder::setAuthor(const Author& author) {
	author_ = author;
	return *this;
}

Hotstuff::Builder& Hotstuff::Builder::setCommandManager(CommandManager* cm) {
	command_manager_ = cm;
	return *this;
}

Hotstuff::Builder& Hotstuff::Builder::setProposerElection(ProposerElection* proposer_election) {
	proposer_election_ = proposer_election;
	return *this;
}

Hotstuff::Builder& Hotstuff::Builder::setStateCompute(StateCompute* state_compute) {
	state_compute_ = state_compute;
	return *this;
}

Hotstuff::Builder& Hotstuff::Builder::setValidatorVerifier(ValidatorVerifier* verifier) {
	verifier_ = verifier;
	return *this;
}

Hotstuff::Builder& Hotstuff::Builder::setNetWork(NetWork* network) {
	network_ = network;
	return *this;
}

Hotstuff::pointer Hotstuff::Builder::build() {
	Hotstuff::pointer hotstuff = nullptr;
	if (io_service_ == nullptr
		|| author_.empty()
		|| command_manager_ == nullptr
		|| proposer_election_ == nullptr
		|| state_compute_ == nullptr
		|| verifier_ == nullptr
		|| network_ == nullptr)
		return hotstuff;

	hotstuff = Hotstuff::pointer( new Hotstuff(
		*io_service_,
		journal_,
		recover_data_,
		author_,
		command_manager_,
		proposer_election_,
		state_compute_,
		verifier_,
		network_));

	return hotstuff;
}

Hotstuff::Hotstuff(
    boost::asio::io_service& ios,
    const beast::Journal& journal,
	const RecoverData& recover_data,
	const Author& author,
	CommandManager* cm,
	ProposerElection* proposer_election,
	StateCompute* state_compute,
	ValidatorVerifier* verifier,
	NetWork* network)
    : storage_(state_compute, Block::new_genesis_block(recover_data.init_ledger_info))
    , epoch_state_()
    , round_state_(&ios)
    , proposal_generator_(cm, &storage_, author)
    , hotstuff_core_(journal, state_compute, &epoch_state_)
    , round_manager_(nullptr)
{
    epoch_state_.epoch = 0;
    epoch_state_.verifier = verifier;

    round_manager_ = new RoundManager(
        journal,
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

void Hotstuff::stop() {
    round_manager_->stop();
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