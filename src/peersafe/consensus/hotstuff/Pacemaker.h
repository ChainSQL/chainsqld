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

#ifndef RIPPLE_CONSENSUS_HOTSTUFF_PACEMAKER_H
#define RIPPLE_CONSENSUS_HOTSTUFF_PACEMAKER_H

#include <peersafe/consensus/hotstuff/Hotstuff.h>
#include <peersafe/consensus/hotstuff/impl/Block.h>

namespace ripple { namespace hotstuff {

class Hotstuff;

class Pacemaker {
public: 
	virtual ~Pacemaker(){};

	virtual ReplicaID GetLeader(int height) = 0;
	virtual void init(Hotstuff* hotstuff, Signal* signal) = 0;

protected:
	Pacemaker() {}
};

class FixedLeader : public Pacemaker {
public:
	FixedLeader(const ReplicaID& leader);
	virtual ~FixedLeader();

	ReplicaID GetLeader(int height) override;
	void init(Hotstuff* hotstuff, Signal* signal) override;

	void run();

private:
	void beat();
	void onHandleEmitEvent(const Event& event);

	ReplicaID leader_;
	Hotstuff* hotstuff_;
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_PACEMAKER_H