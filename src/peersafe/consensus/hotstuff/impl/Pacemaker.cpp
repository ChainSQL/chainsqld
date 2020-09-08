
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

#include <peersafe/consensus/hotstuff/Pacemaker.h>

namespace ripple { namespace hotstuff {

FixedLeader::FixedLeader(const ReplicaID& leader)
: leader_(leader)
, hotstuff_(nullptr) {

}

FixedLeader::~FixedLeader() {

}

ReplicaID FixedLeader::GetLeader(int /*height*/) {
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

} // namespace hotstuff
} // namespace ripple