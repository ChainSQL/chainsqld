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

#include <peersafe/consensus/hotstuff/impl/VoteData.h>

namespace ripple {
namespace hotstuff {

VoteData::VoteData(
    const BlockInfo& proposed,
    const BlockInfo& parent,
    std::size_t tc)
    : proposed_(proposed)
    , parent_(parent)
    , tc_(tc)
{
}

VoteData::~VoteData() {
}

VoteData VoteData::New(
	const BlockInfo& proposed,
	const BlockInfo& parent,
    std::size_t tc) {
	VoteData vote_data(proposed, parent, tc);
	//vote_data.proposed_ = proposed;
	//vote_data.parent_ = parent;
	return vote_data;
}

HashValue VoteData::hash() const {
	using beast::hash_append;
	ripple::sha512_half_hasher h;
	hash_append(h, proposed_.id);
	hash_append(h, parent_.id);
    hash_append(h, proposed_.ledger_info.hash);
    hash_append(h, parent_.ledger_info.hash);
    hash_append(h, tc_);
    if (proposed_.next_epoch_state)
    {
        hash_append(h, proposed_.next_epoch_state->epoch);
    }
    if (parent_.next_epoch_state)
    {
        hash_append(h, parent_.next_epoch_state->epoch);
    }

	return static_cast<typename	sha512_half_hasher::result_type>(h);
}

bool VoteData::Verify() const {
	if (parent().epoch != proposed().epoch)
		return false;
	if (parent().round >= proposed().round)
		return false;
	if (parent().timestamp_msecs > proposed().timestamp_msecs)
		return false;
	if (parent().version > proposed().version)
		return false;
	return true;
}

} // namespace hotstuff
} // namespace ripple