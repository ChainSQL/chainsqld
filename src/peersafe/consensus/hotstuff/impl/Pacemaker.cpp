
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

#include <iostream>
#include <functional>
#include <cmath>

#include <peersafe/consensus/hotstuff/Pacemaker.h>

namespace ripple { namespace hotstuff {

FixedLeader::FixedLeader(const ReplicaID& leader)
: leader_(leader)
, hotstuff_(nullptr) {

}

FixedLeader::~FixedLeader() {

}

ReplicaID FixedLeader::GetLeader(int /*view*/) {
	return leader_;
}

void FixedLeader::init(Hotstuff* hotstuff, Signal* signal) {
	assert(hotstuff);
	assert(signal);

	hotstuff_ = hotstuff;
	if(signal) {
		signal->doOnEmitEvent(
			std::bind(&FixedLeader::onHandleEmitEvent, this, std::placeholders::_1));
	}
}

void FixedLeader::run() {
	beat();
}

void FixedLeader::beat() {
	if(hotstuff_ == nullptr)
		return;

	if(hotstuff_->id() == GetLeader(0))
		hotstuff_->propose();
}

void FixedLeader::onHandleEmitEvent(const Event &event) {
	if(event.type == Event::QCFinish) {
		beat();
	}
}

/////////////////////////////////////////////////////////////
// RoundRobinLeader
/////////////////////////////////////////////////////////////

RoundRobinLeader::RoundRobinLeader(boost::asio::io_service* io_service) 
: hotstuff_(nullptr)
, io_service_(io_service)
, dummy_block_timer_(*io_service_)
, last_beat_(0)
, running_(false) {

}

RoundRobinLeader::~RoundRobinLeader() {
	assert(running_ == false);
}

ReplicaID RoundRobinLeader::GetLeader(int view) {
	if(hotstuff_ == nullptr)
		return -1;	// invalid replicaid

	std::size_t term = static_cast<std::size_t>(
		std::ceil(static_cast<float>(view) / static_cast<float>(hotstuff_->config().view_change)));
	std::size_t size = hotstuff_->config().leader_schedule.size();
	return hotstuff_->config().leader_schedule[term % size];
}

void RoundRobinLeader::init(Hotstuff* hotstuff, Signal* signal) {
	hotstuff_ = hotstuff;
	if(signal) {
		signal->doOnEmitEvent(
			std::bind(&RoundRobinLeader::onHandleEmitEvent, this, std::placeholders::_1)
		);
	}
}

void RoundRobinLeader::run() {
	running_ = true;
	// setup timer for generate dummy blocks
	setUpDummyBlockTimer();

	if(hotstuff_->id() == GetLeader(1)) {
		hotstuff_->propose();
	}
}

void RoundRobinLeader::stop() {
	running_ = false;
	// cancel timer
	closeDummyBlockTimer();
}

void RoundRobinLeader::beat() {
	if(hotstuff_ == nullptr)
		return;

	int nextView = hotstuff_->Height();
	if(hotstuff_->id() == GetLeader(nextView)
		&& last_beat_ < nextView
		&& nextView >= hotstuff_->votedBlock().height) {
			last_beat_ = nextView;
			hotstuff_->propose();
	}
}

void RoundRobinLeader::onHandleEmitEvent(const Event& event) {
	if(running_ == false)
		return;
	if(event.type == Event::QCFinish) {
		beat();
	} else if (event.type == Event::ReceiveNewView) {
		//std::cout
		//	<< hotstuff_->id() << " receive a newView, begin beat("
		//	<< hotstuff_->Height() << ")"
		//	<< std::endl;
		beat();
	} else if(event.type == Event::ReceiveProposal) {
		closeDummyBlockTimer();
		setUpDummyBlockTimer();
	}
}

void RoundRobinLeader::setUpDummyBlockTimer() {
	dummy_block_timer_.expires_from_now(std::chrono::seconds(hotstuff_->config().timeout));
	dummy_block_timer_.async_wait(
		std::bind(&RoundRobinLeader::generateDummyBlock, this, std::placeholders::_1)
	);
}

void RoundRobinLeader::closeDummyBlockTimer() {
	dummy_block_timer_.cancel();
}

void RoundRobinLeader::generateDummyBlock(const boost::system::error_code& ec) {
	if(ec) {
		return;
	}

	// remove current leader from schedule
	int newHeight = hotstuff_->Height() + 1;
	//ReplicaID current_leader = GetLeader(newHeight);

	// create a dummy block
	Block leaf = hotstuff_->CreateLeaf(
		hotstuff_->leaf(), 
		Command(), 
		QuorumCert(), 
		newHeight
	);
	leaf.hash = Block::blockHash(leaf);
	hotstuff_->setLeaf(leaf);

	// send new-view to next leader
	hotstuff_->nextSyncNewView(newHeight);

	//std::cout
	//	<< hotstuff_->id()
	//	<< " generate a dummy block(" << newHeight
	//	<< ") because replica(" << current_leader << ") created block failed."
	//	<< std::endl;

	setUpDummyBlockTimer();
}

} // namespace hotstuff
} // namespace ripple