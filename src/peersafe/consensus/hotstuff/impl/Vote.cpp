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

#include <peersafe/consensus/hotstuff/impl/Vote.h>

namespace ripple {
namespace hotstuff {

Vote::Vote()
: vote_data_(VoteData::New(BlockInfo(ZeroHash()), BlockInfo(ZeroHash())))
, author_()
, ledger_info_(LedgerInfoWithSignatures::LedgerInfo{BlockInfo(ZeroHash()), ZeroHash()})
, signature_()
, timeout_signature_() {

}

Vote::~Vote() {

}

Vote Vote::New(
	const Author author,
	const VoteData& vote_data,
	const LedgerInfoWithSignatures::LedgerInfo& ledger_info,
	const Signature& signature) {
	Vote vote;
	vote.vote_data_ = vote_data;
	vote.author_ = author;
	vote.ledger_info_ = ledger_info;
	vote.signature_ = signature;
	return vote;
}

} // namespace hotstuff
} // namespace ripple