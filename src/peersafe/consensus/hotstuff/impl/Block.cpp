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

#include <vector>

#include <peersafe/consensus/hotstuff/impl/Block.h>

namespace ripple { namespace hotstuff {

Author BlockData::NONEAUTHOR = Author();


HashValue BlockData::hash(const BlockData& block_data) {
	using beast::hash_append;
	ripple::sha512_half_hasher h;
	hash_append(h, block_data.epoch);
	hash_append(h, block_data.round);
	hash_append(h, block_data.timestamp_msecs);
	hash_append(h, block_data.block_type);

	if (block_data.block_type == BlockData::Proposal
		&& block_data.payload) {
		hash_append(h, block_data.payload->cmd);
        hash_append(h, block_data.getLedgerInfo().hash);
	}
    else // for Genesis or NilBlock
    {
        hash_append(h, block_data.getLedgerInfo().hash);
    }

	return static_cast<typename	sha512_half_hasher::result_type>(h);
}

Block::Block()
: id_()
, block_data_()
, signature_() {
}

Block::~Block() {
}

Block Block::empty() {
	return Block();
}

Block Block::nil_block(Round round, const QuorumCertificate& hqc) {
	Block block = Block::empty();

	BlockData block_data;
	block_data.round = round;
	block_data.quorum_cert = hqc;
	block_data.block_type = BlockData::NilBlock;

    block_data.getLedgerInfo() = hqc.certified_block().ledger_info;

	block.id_ = BlockData::hash(block_data);
	block.block_data_ = block_data;

	return block;
}

Block Block::new_from_block_data(const BlockData& block_data, ValidatorVerifier* verifier) {
	Block block;

	block.id_ = BlockData::hash(block_data);
	block.block_data_ = block_data;

	Signature signature;
	if (verifier->signature(block.id_, signature))
		block.signature_ = signature;
	return block;
}

Block Block::new_genesis_block(
	const ripple::LedgerInfo& ledger_info,
	const Epoch& epoch) {

	// genesis blockinfo
	BlockInfo genesis_block_info(ZeroHash());
	genesis_block_info.ledger_info = ledger_info;
	genesis_block_info.epoch = epoch;

	// genesis VoteData
	VoteData genesis_vote_data = VoteData::New(genesis_block_info, genesis_block_info, 0);
	// genesis LedgerInfo
	LedgerInfoWithSignatures::LedgerInfo genesis_ledger{genesis_block_info, genesis_vote_data.hash()};
	// genesis QuorumCertificate
	QuorumCertificate genesis_qc(genesis_vote_data, genesis_ledger);

	// genesis BlockDate
	BlockData genesis_block_data;
	genesis_block_data.epoch = genesis_block_info.epoch;
	genesis_block_data.round = genesis_block_info.round;
	genesis_block_data.timestamp_msecs = genesis_block_info.timestamp_msecs;
	genesis_block_data.quorum_cert = genesis_qc;
	genesis_block_data.block_type = BlockData::Genesis;


	Block genesis_block;
	genesis_block.id_ = BlockData::hash(genesis_block_data);
	genesis_block.block_data_ = genesis_block_data;

	return genesis_block;
}

BlockInfo Block::gen_block_info(
	const ripple::LedgerInfo& ledger_info,
		const boost::optional<EpochState>& next_epoch_state) {
	BlockInfo block_info(id_);
	block_info.epoch = block_data_.epoch;
	block_info.round = block_data_.round;
	block_info.ledger_info = ledger_info;
	block_info.timestamp_msecs = block_data_.timestamp_msecs;
	block_info.next_epoch_state = next_epoch_state;

	return block_info;
}

} // namespace hotstuff
} // namespace ripple