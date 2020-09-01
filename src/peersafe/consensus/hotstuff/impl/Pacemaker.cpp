
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

#include <peersafe/consensus/hotstuff/Pacemaker.h>

namespace ripple { namespace hotstuff {

FixedLeader::FixedLeader()
: hotstuff_(nullptr) {

}

FixedLeader::~FixedLeader() {

}

ReplicaID FixedLeader::GetLeader(int height) {
	return 1;
}

void FixedLeader::init(Hotstuff* hotstuff) {
	hotstuff_ = hotstuff;
}

void FixedLeader::run() {
	if(hotstuff_ == nullptr)
		return;

	if(hotstuff_->id() == GetLeader(0))
		hotstuff_->Propose();
}

} // namespace hotstuff
} // namespace ripple