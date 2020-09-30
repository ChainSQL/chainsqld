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
    const beast::Journal& journal,
	BlockStorage* storage,
	EpochState* epoch_state,
	RoundState* round_state,
	ProposalGenerator* proposal_generator,
	ProposerElection* proposer_election,
	NetWork* network)
: hotstuff_core_(journal, epoch_state)
, round_manager_(nullptr) {

	round_manager_ = new RoundManager(
		storage, 
		round_state, 
		&hotstuff_core_,
		proposal_generator,
		proposer_election,
		network);
}

Hotstuff::~Hotstuff() {
    delete round_manager_;
}

int Hotstuff::start() {
	return round_manager_->start();
}

} // namespace hotstuff
} // namespace ripple