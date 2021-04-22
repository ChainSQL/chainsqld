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

#ifndef RIPPLE_CONSENSUS_HOTSTUFF_PROPOSER_ELECTION_H
#define RIPPLE_CONSENSUS_HOTSTUFF_PROPOSER_ELECTION_H

#include <peersafe/consensus/hotstuff/impl/Block.h>

namespace ripple {
namespace hotstuff {

class ProposerElection {
public:
	bool IsValidProposer(const Author& author, Round round) const {
		return GetValidProposer(round) == author;
	}

	bool
        IsValidProposal(const Block& block) const
        {
            if (block.block_data().block_type == BlockData::Proposal)
                return IsValidProposer(
                    block.block_data().payload->author,
                    block.block_data().round +
                        block.block_data().quorum_cert.vote_data().tc());
            else
                return false;
        }

	/// Return the valid proposer for a given round (this information can be
	/// used by e.g., voters for choosing the destinations for sending their votes to).
	virtual Author GetValidProposer(Round round) const = 0;

	virtual ~ProposerElection() {}
protected:
    ProposerElection() {}
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_PROPOSER_ELECTION_H