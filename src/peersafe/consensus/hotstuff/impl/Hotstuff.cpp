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
    ripple::Adaptor& adaptor,
    clock_type const& clock,
    beast::Journal j)
    : ConsensusBase(clock, j)
    , adaptor_(*(HotstuffAdaptor*)(&adaptor))
    , init_vote_data_(VoteData::New(BlockInfo(ZeroHash()), BlockInfo(ZeroHash())))
    , init_ledgerinfo_(LedgerInfoWithSignatures::LedgerInfo{ init_vote_data_.proposed(), init_vote_data_.hash() })
    , storage_(
	    this, 
	    QuorumCertificate(init_vote_data_, init_ledgerinfo_),
	    QuorumCertificate(init_vote_data_, init_ledgerinfo_))
    , epoch_state_()
    , round_state_(adaptor_.getIOService())
    , proposal_generator_(this, &storage_, adaptor_.valPublic())
    , hotstuff_core_(j, this, &epoch_state_)
    , round_manager_(nullptr)
{
	epoch_state_.epoch = 0;
	epoch_state_.verifier = this;

	round_manager_ = new RoundManager(
		&storage_, 
		&round_state_, 
		&hotstuff_core_,
		&proposal_generator_,
		&adaptor_,
        &adaptor_);
}

Hotstuff::~Hotstuff() {
    delete round_manager_;
}

void Hotstuff::startRound(
    NetClock::time_point const& now,
    typename Ledger_t::ID const& prevLedgerID,
    Ledger_t prevLedger,
    hash_set<NodeID> const& nowUntrusted,
    bool proposing)
{
	round_manager_->start();
}

void Hotstuff::timerEntry(NetClock::time_point const& now)
{
    (void)now;
}

bool Hotstuff::peerConsensusMessage(
    std::shared_ptr<PeerImp>& peer,
    bool isTrusted,
    std::shared_ptr<protocol::TMConsensus> const& m)
{
    return true;
}

void Hotstuff::gotTxSet(NetClock::time_point const& now, TxSet_t const& txSet)
{

}

Json::Value Hotstuff::getJson(bool full) const
{
    Json::Value ret(Json::objectValue);

    return ret;
}

void Hotstuff::extract(ripple::hotstuff::Command& cmd)
{

}

bool Hotstuff::compute(const Block& block, ripple::LedgerInfo& ledger_info)
{
    return false;
}

bool Hotstuff::verify(const ripple::LedgerInfo& ledger_info, const ripple::LedgerInfo& parent_ledger_info)
{
    return false;
}

int Hotstuff::commit(const Block& block)
{
    return false;
}

const Author& Hotstuff::Self() const
{
    return adaptor_.valPublic();
}

bool Hotstuff::signature(const ripple::Slice& message, Signature& signature)
{
    return false;
}

bool Hotstuff::verifySignature(const Author& author, const Signature& signature, const ripple::Slice& message)
{
    return false;
}

const bool Hotstuff::verifyLedgerInfo(
    const BlockInfo& commit_info,
    const HashValue& consensus_data_hash,
    const std::map<Author, Signature>& signatures) const
{
    return false;
}

const bool Hotstuff::checkVotingPower(const std::map<Author, Signature>& signatures) const
{
    return false;
}

// -------------------------------------------------------------------
// Private member functions

int Hotstuff::handleProposal(
    const ripple::hotstuff::Block& proposal,
    const ripple::hotstuff::SyncInfo& sync_info)
{
    return round_manager_->ProcessProposal(proposal, sync_info);
}

int Hotstuff::handleVote(
    const ripple::hotstuff::Vote& vote,
    const ripple::hotstuff::SyncInfo& sync_info)
{
    return round_manager_->ProcessVote(vote, sync_info);
}

} // namespace hotstuff
} // namespace ripple