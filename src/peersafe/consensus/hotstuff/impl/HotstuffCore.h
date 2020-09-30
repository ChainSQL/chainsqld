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

#ifndef RIPPLE_CONSENSUS_HOTSTUFF_CORE_H
#define RIPPLE_CONSENSUS_HOTSTUFF_CORE_H

#include <boost/optional.hpp>

#include <peersafe/consensus/hotstuff/impl/Block.h>
#include <peersafe/consensus/hotstuff/impl/Vote.h>
#include <peersafe/consensus/hotstuff/impl/QuorumCert.h>
#include <peersafe/consensus/hotstuff/impl/EpochState.h>

#include <ripple/basics/Log.h>
#include <ripple/core/JobQueue.h>

namespace ripple { namespace hotstuff {

struct SafetyData {
	Epoch epoch;
	Round last_voted_round;
	Round preferred_round;
	boost::optional<Vote> last_vote;

	SafetyData()
	: epoch(0)
	, last_voted_round(0)
	, preferred_round(0)
	, last_vote() {

	}
};

class HotstuffCore {
public: 
	HotstuffCore(const beast::Journal& journal, EpochState* epoch_state);
    ~HotstuffCore();

	Block SignProposal(const BlockData& proposal);
private:
	bool VerifyAuthor(const Author& author);
	bool VerifyEpoch(const Epoch epoch);
	bool VerifyQC(const QuorumCertificate& qc);

	SafetyData safety_data_;
	beast::Journal journal_;
	EpochState* epoch_state_;

};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_CORE_H