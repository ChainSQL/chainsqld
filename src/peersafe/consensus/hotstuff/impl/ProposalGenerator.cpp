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

#include <ripple/basics/Log.h>

namespace ripple {
namespace hotstuff {

ProposalGenerator::ProposalGenerator(
	const beast::Journal& journal,
	CommandManager* cm, 
	BlockStorage* block_store,
	const Author& author)
: journal_(journal)
, command_manager_(cm)
, block_store_(block_store)
, author_(author)
, last_round_generated_(0) {

}

ProposalGenerator::~ProposalGenerator() {

}

boost::optional<Block> ProposalGenerator::GenerateNilBlock(Round round) {
	boost::optional<QuorumCertificate> hqc = EnsureHighestQuorumCert(round);
	if (hqc) {
		Block block = Block::nil_block(round, hqc.get());
		return boost::optional<Block>(block);
	}
	return boost::optional<Block>();
}

boost::optional<BlockData> ProposalGenerator::Proposal(Round round) {
	if (last_round_generated_.exchange(round) >= round) {
		return boost::optional<BlockData>();
	}

	boost::optional<QuorumCertificate> hqc = EnsureHighestQuorumCert(round);
	if (hqc) {
		BlockData blockData;
		blockData.block_type = BlockData::Proposal;
		BlockData::Payload payload;
		payload.author = author_;
		blockData.round = round;
		blockData.epoch = hqc->certified_block().epoch;
		blockData.quorum_cert = hqc.get();
		blockData.timestamp_usecs = 0;
		if (hqc->certified_block().hasReconfiguration()) {
			payload.cmd = ZeroHash();
			blockData.timestamp_usecs = hqc->certified_block().timestamp_usecs;
		}
		else {
			// extrats txs
            auto cmd = command_manager_->extract(blockData);
            if (!cmd)
            {
                return boost::none;
            }
            payload.cmd = cmd.get();
		}
		blockData.payload = payload;

		return boost::optional<BlockData>(blockData);
	}
	
	return boost::optional<BlockData>();

}

boost::optional<QuorumCertificate> ProposalGenerator::EnsureHighestQuorumCert(Round round) {
	QuorumCertificate hqc = block_store_->HighestQuorumCert();
	if (hqc.certified_block().round >= round) {
		JLOG(journal_.error())
			<< "HQC's round in local is higher than new round.";
		return boost::optional<QuorumCertificate>();
	}
	if (hqc.endsEpoch()) {
		JLOG(journal_.warn())
			<< "HQC is endsepoch. The HQC's round is "
			<< hqc.certified_block().round;
		return boost::optional<QuorumCertificate>();
	}
	return boost::optional<QuorumCertificate>(hqc);
}

} // namespace hotstuff
} // namespace ripple