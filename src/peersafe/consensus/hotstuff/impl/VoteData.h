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

#ifndef RIPPLE_CONSENSUS_HOTSTUFF_VOTEDATA_H
#define RIPPLE_CONSENSUS_HOTSTUFF_VOTEDATA_H

#include <peersafe/consensus/hotstuff/impl/BlockInfo.h>

namespace ripple {
namespace hotstuff {

class VoteData { 
public:
	~VoteData();

	static VoteData
    New(const BlockInfo& proposed, const BlockInfo& parent, std::size_t tc);

	BlockInfo& proposed() {
		return proposed_;
	}

	const BlockInfo& proposed() const {
		return proposed_;
	}

	BlockInfo& parent() {
		return parent_;
	}

	const BlockInfo& parent() const {
		return parent_;
	}

    std::size_t&
    tc()
    {
        return tc_;
    }

    std::size_t
    tc() const
    {
        return tc_;
    }

	HashValue hash() const;

	bool Verify() const;

	// friend class ripple::Serialization;
    // only for serialization
    VoteData()
        : proposed_(ZeroHash())
        , parent_(ZeroHash())
        , tc_(0)
    {
    }  // only for constructing in Serilization

private:
	VoteData(
		const BlockInfo& proposed,
		const BlockInfo& parent,
        std::size_t tc);

	/// Contains all the block information needed for voting for the proposed round.
	BlockInfo proposed_;
	/// Contains all the block information for the block the proposal is extending.
	BlockInfo parent_;
    /// timeout count
    std::size_t tc_;
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_VOTEDATA_H