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

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>

#include <peersafe/consensus/hotstuff/Hotstuff.h>
#include <peersafe/consensus/hotstuff/impl/Block.h>

namespace ripple { namespace hotstuff {

class Hotstuff;

class Pacemaker {
public: 
	virtual ~Pacemaker(){};

	virtual ReplicaID GetLeader(int view) = 0;
	virtual void init(Hotstuff* hotstuff, Signal* signal) = 0;
	// 配置更新时候需要重置相关状态
	virtual void reset() = 0;

protected:
	Pacemaker() {}
};

class FixedLeader : public Pacemaker {
public:
	FixedLeader(const ReplicaID& leader);
	virtual ~FixedLeader();

	ReplicaID GetLeader(int view) override;
	void init(Hotstuff* hotstuff, Signal* signal) override;
	void reset() override {}

	void run();

private:
	void beat();
	void onHandleEmitEvent(const Event& event);

	ReplicaID leader_;
	Hotstuff* hotstuff_;
};

class RoundRobinLeader : public Pacemaker {
public:
	RoundRobinLeader(boost::asio::io_service* io_service);
	virtual ~RoundRobinLeader();

	ReplicaID GetLeader(int view) override;
	void init(Hotstuff* hotstuff, Signal* signal) override;
	void reset() override {
		reset_view_ = true;
	}

	void run();
	void stop();

private:
	void beat();
	void onHandleEmitEvent(const Event& event);

	void setUpDummyBlockTimer();
	void closeDummyBlockTimer();
	void generateDummyBlock(const boost::system::error_code& ec);

	Hotstuff* hotstuff_;
	boost::asio::io_service* io_service_;
	boost::asio::steady_timer dummy_block_timer_;
	int last_beat_;
	bool running_;
	bool reset_view_;
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_PACEMAKER_H