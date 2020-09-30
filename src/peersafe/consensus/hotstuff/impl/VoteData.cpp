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

VoteData::VoteData()
: proposed_()
, parent_() {

}

VoteData::~VoteData() {
}

BlockHash VoteData::hash() {
	using beast::hash_append;
	ripple::sha512_half_hasher h;
	hash_append(h, BlockInfo::hash(proposed()));
	hash_append(h, BlockInfo::hash(parent()));

	return static_cast<typename	sha512_half_hasher::result_type>(h);
}

} // namespace hotstuff
} // namespace ripple