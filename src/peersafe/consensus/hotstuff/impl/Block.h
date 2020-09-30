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

#ifndef RIPPLE_CONSENSUS_HOTSTUFF_BLOCK_H
#define RIPPLE_CONSENSUS_HOTSTUFF_BLOCK_H

#include <boost/optional.hpp>

//#include <peersafe/consensus/hotstuff/impl/Crypto.h>
#include <peersafe/consensus/hotstuff/impl/QuorumCert.h>
#include <peersafe/consensus/hotstuff/impl/EpochState.h>

namespace ripple { namespace hotstuff {

class BlockData {
public:
	enum BlockType {
		Proposal, 
		NilBlock, 
		Genesis
	};

	struct Payload {
		Command cmd;
		Author author;
		
		Payload()
		: cmd()
		, author() {}
	};

	/// Epoch number corresponds to the set of validators that are active for this block.
	Epoch epoch;
	/// The round of a block is an internal monotonically increasing counter used by Consensus
	/// protocol.
	Round round;
	int64_t timestamp_usecs;
	/// Contains the quorum certified ancestor and whether the quorum certified ancestor was
	/// voted on successfully
	QuorumCertificate quorum_cert;
	/// type of block
	BlockType block_type;
	/// payload is valid when block_type is Proposal
	boost::optional<Payload> payload;

	static Author NONEAUTHOR;
	const Author& author() const {
		if (payload) {
			return payload->author;
		}
		return  BlockData::NONEAUTHOR;
	}

	static BlockHash hash(const BlockData& block_data);

	BlockData()
	: epoch(0)
	, round(0)
	, timestamp_usecs(0)
	, quorum_cert()
	, block_type(NilBlock)
	, payload() {
	}
};


class Block {
public: 
    ~Block();

	static Block empty();
	static Block new_from_block_data(
		const BlockData& block_data, 
		ValidatorVerifier* verifier);

	const BlockHash& id() const {
		return id_;
	}

	const BlockData& block_data() const {
		return block_data_;
	}

	const boost::optional<Signature>& signature() const {
		return signature_;
	}
private:
    Block();

	BlockHash id_;
	BlockData block_data_;
	boost::optional<Signature> signature_;
};



} // namespace hotstuff 
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_BLOCK_H