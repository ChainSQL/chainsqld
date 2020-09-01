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

#ifndef RIPPLE_CONSENSUS_HOTSTUFF_H
#define RIPPLE_CONSENSUS_HOTSTUFF_H

#include <peersafe/consensus/hotstuff/impl/Block.h>
#include <peersafe/consensus/hotstuff/impl/HotstuffCore.h>

namespace ripple { namespace hotstuff {

class Pacemaker;

class Sender {
public:
    virtual ~Sender() {}

    virtual void proposal(ReplicaID id, const Block& block) = 0;
    virtual void vote(ReplicaID id, const PartialCert& cert) = 0;
protected:
    Sender() {}
};

class Hotstuff {
public: 
    Hotstuff(
        ReplicaID id, 
        Sender* sender,
        Storage* storage,
        Executor* executor,
        Pacemaker* pacemaker);

    ~Hotstuff();

    ReplicaID id() const {
        return id_;
    }

    void Propose();

    void handlePropose(const Block& block);
    void handleVote(const PartialCert& cert);
private:
    void broadCast(const Block& block);

    ReplicaID id_;
    HotstuffCore hotstuff_core_;
    Sender* sender_;
    Pacemaker* pacemaker_;
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_H