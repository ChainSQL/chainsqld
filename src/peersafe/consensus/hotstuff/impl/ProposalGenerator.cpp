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

#include <peersafe/consensus/hotstuff/impl/ProposalGenerator.h>


namespace ripple {
namespace hotstuff {

ProposalGenerator::ProposalGenerator(
	CommandManager* cm, 
	BlockStorage* block_store,
	const Author& author)
: command_manager_(cm)
, block_store_(block_store)
, author_(author)
, last_round_generated_(0) {

}

ProposalGenerator::~ProposalGenerator() {

}

boost::optional<Block> ProposalGenerator::GenerateNilBlock(Round round) {
	QuorumCertificate hqc = HighestQuorumCert(round);
	Block block = Block::nil_block(round, hqc);
	return boost::optional<Block>(block);
}

boost::optional<BlockData> ProposalGenerator::Proposal(Round round) {
	if (last_round_generated_.load() >= round) {
		return boost::optional<BlockData>();
	}
	last_round_generated_.store(round);

	QuorumCertificate hqc = HighestQuorumCert(round);
	BlockData blockData;
	blockData.epoch = hqc.certified_block().epoch;
	blockData.round = round;
	blockData.timestamp_usecs = 0;
	blockData.quorum_cert = hqc;
	blockData.block_type = BlockData::Proposal;
	
	BlockData::Payload payload;
	payload.author = author_;
	// extrats txs
	auto cmd = command_manager_->extract(blockData);
    if (!cmd)
    {
        return boost::none;
    }
    payload.cmd = cmd.get();

	blockData.payload = payload;

	return boost::optional<BlockData>(blockData);
}

QuorumCertificate ProposalGenerator::HighestQuorumCert(Round round) {
	QuorumCertificate hqc = block_store_->HighestQuorumCert();
	assert(hqc.certified_block().round < round);
	return hqc;
}

} // namespace hotstuff
} // namespace ripple