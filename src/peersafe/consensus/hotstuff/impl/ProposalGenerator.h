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

#ifndef RIPPLE_CONSENSUS_HOTSTUFF_PROPOSAL_GENERATOR_H
#define RIPPLE_CONSENSUS_HOTSTUFF_PROPOSAL_GENERATOR_H

#include <atomic>

#include <boost/optional.hpp>

#include <peersafe/consensus/hotstuff/impl/Block.h>
#include <peersafe/consensus/hotstuff/impl/QuorumCert.h>
#include <peersafe/consensus/hotstuff/impl/BlockStorage.h>

namespace ripple { namespace hotstuff {

class ProposalGenerator {
public:
    ProposalGenerator(
		beast::Journal journal,
		CommandManager* cm, 
		BlockStorage* block_store,
		const Author& author);
    ~ProposalGenerator();

	boost::optional<Block> GenerateNilBlock(Round round);
	boost::optional<BlockData> Proposal(Round round);
	bool canExtract();

	const Author& author() const {
		return author_;
	}

	void reset() {
		last_round_generated_ = 0;
	}

private:
	boost::optional<QuorumCertificate> EnsureHighestQuorumCert(Round round);

	beast::Journal journal_;
	CommandManager* command_manager_;
	BlockStorage* block_store_;
	Author author_;
	std::atomic<Round> last_round_generated_;
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_PROPOSAL_GENERATOR_H