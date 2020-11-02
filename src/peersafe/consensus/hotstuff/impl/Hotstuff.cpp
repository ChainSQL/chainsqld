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
, config_()
, command_manager_(nullptr)
, proposer_election_(nullptr)
, state_compute_(nullptr)
, network_(nullptr) {

}

Hotstuff::Builder& Hotstuff::Builder::setConfig(const Config& config) {
	config_ = config;
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

Hotstuff::Builder& Hotstuff::Builder::setNetWork(NetWork* network) {
	network_ = network;
	return *this;
}

Hotstuff::pointer Hotstuff::Builder::build() {
	Hotstuff::pointer hotstuff = nullptr;
	if (io_service_ == nullptr
		|| config_.id.empty()
		|| config_.timeout <= 0
		|| command_manager_ == nullptr
		|| proposer_election_ == nullptr
		|| state_compute_ == nullptr
		|| network_ == nullptr)
		return hotstuff;

	hotstuff = Hotstuff::pointer( new Hotstuff(
		*io_service_,
		journal_,
		config_,
		command_manager_,
		proposer_election_,
		state_compute_,
		//verifier_,
		network_));

	return hotstuff;
}

Hotstuff::Hotstuff(
    boost::asio::io_service& ios,
    const beast::Journal& journal,
	const Config config,
	CommandManager* cm,
	ProposerElection* proposer_election,
	StateCompute* state_compute,
	NetWork* network)
: journal_(journal)
, config_(config)
, storage_(journal_, state_compute)
, epoch_state_()
, round_state_(&ios, journal_)
, proposal_generator_(journal_, cm, &storage_, config.id)
, proposer_election_(proposer_election)
, network_(network)
, hotstuff_core_(journal_, state_compute, &epoch_state_)
, round_manager_(nullptr) {

}

Hotstuff::~Hotstuff() {
    delete round_manager_;
}

int Hotstuff::start(const RecoverData& recover_data) {
	storage_.updateCeritificates(Block::new_genesis_block(
		recover_data.init_ledger_info, recover_data.epoch_state.epoch));
	epoch_state_ = recover_data.epoch_state;
	hotstuff_core_.Initialize(recover_data.epoch_state.epoch, 0);
	round_state_.reset();
	proposal_generator_.reset();

	if (round_manager_ == nullptr) {
		round_manager_ = new RoundManager(
			journal_,
			config_,
			&storage_,
			&round_state_,
			&hotstuff_core_,
			&proposal_generator_,
			proposer_election_,
			network_);
	}
	return round_manager_->start();
}

void Hotstuff::stop() {
    round_manager_->stop();
}

bool Hotstuff::CheckProposal(
	const ripple::hotstuff::Block& proposal,
	const ripple::hotstuff::SyncInfo& sync_info) {
	return round_manager_->CheckProposal(proposal, sync_info);
}

int Hotstuff::handleProposal(
	const ripple::hotstuff::Block& proposal, 
	const Round& shift /*= 0*/) {
	return round_manager_->ProcessProposal(proposal, shift);
}

int Hotstuff::handleVote(
	const ripple::hotstuff::Vote& vote,
	const ripple::hotstuff::SyncInfo& sync_info) {
	return round_manager_->ProcessVote(vote, sync_info);
}

// Exepect an executed block by block_id
bool Hotstuff::expectBlock(
	const HashValue& block_id,
	ExecutedBlock& executed_block) {
	return storage_.safetyBlockOf(block_id, executed_block);
}


} // namespace hotstuff
} // namespace ripple