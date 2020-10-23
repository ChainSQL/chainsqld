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

#include <ripple/ledger/ReadView.h>

#include <peersafe/consensus/hotstuff/impl/QuorumCert.h>
#include <peersafe/consensus/hotstuff/impl/EpochState.h>

//#include <ripple/core/Serialization.h>

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

    ripple::LedgerInfo& getLedgerInfo()
    {
        return quorum_cert.ledger_info().ledger_info.commit_info.ledger_info;
    }

    ripple::LedgerInfo const& getLedgerInfo() const
    {
        return quorum_cert.ledger_info().ledger_info.commit_info.ledger_info;
    }

	static HashValue hash(const BlockData& block_data);

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
    Block();
    ~Block();

	static Block empty();
	static Block nil_block(Round round, const QuorumCertificate& hqc);
	static Block new_from_block_data(
		const BlockData& block_data, 
		ValidatorVerifier* verifier);
	static Block new_genesis_block(const ripple::LedgerInfo& ledger_info);

	const HashValue& id() const {
		return id_;
	}

	const BlockData& block_data() const {
		return block_data_;
	}

	BlockData& block_data() {
		return block_data_;
	}

    ripple::LedgerInfo const& getLedgerInfo() const
    {
        return block_data_.getLedgerInfo();
    }

	const boost::optional<Signature>& signature() const {
		return signature_;
	}

	BlockInfo gen_block_info(
		const ripple::LedgerInfo& ledger_info,
		const boost::optional<EpochState>& next_epoch_state);


	// only for serialize
	HashValue& id() {
		return id_;
	}

	boost::optional<Signature>& signature() {
		return signature_;
	}

	//friend class ripple::Serialization;
	// only for serialization
	Block();

private:
	HashValue id_;
	BlockData block_data_;
	boost::optional<Signature> signature_;
};

/////////////////////////////////////////////////////////////////////////////////////
//// serialize & deserialize
/////////////////////////////////////////////////////////////////////////////////////
//template<class Archive>
//void serialize(Archive& ar, BlockData::Payload& payload, const unsigned int /*version*/) {
//	// note, version is always the latest when saving
//	ar & payload.cmd;
//	ar & payload.author;
//}
//
//template<class Archive>
//void serialize(Archive& ar, BlockData& block_data , const unsigned int /*version*/) {
//	// note, version is always the latest when saving
//	ar & block_data.epoch;
//	ar & block_data.round;
//	ar & block_data.timestamp_usecs;
//	ar & block_data.quorum_cert;
//	ar & block_data.block_type;
//	ar & block_data.payload;
//}
//
//template<class Archive>
//void serialize(Archive& ar, Block& block, const unsigned int /*version*/) {
//	ar & block.id();
//	ar & block.block_data();
//	ar & block.signature();
//}

} // namespace hotstuff 
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_BLOCK_H