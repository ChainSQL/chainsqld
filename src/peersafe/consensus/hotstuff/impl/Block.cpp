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

#include <peersafe/consensus/hotstuff/impl/Block.h>

#include <vector>

namespace ripple { namespace hotstuff {

Block::Block()
: hash()
, height(0)
, parent()
, justify()
, cmd()
, committed(false) {
}

Block::~Block() {
}

BlockHash Block::blockHash(const Block& block) {
    using beast::hash_append;

    if(block.hash.isNonZero())
        return block.hash;

    ripple::sha512_half_hasher h;
    hash_append(h, block.height);
    hash_append(h, block.parent);
    hash_append(h, block.justify.toBytes());
    
    for(std::size_t i = 0; i < block.cmd.size(); i++) {
        hash_append(h, block.cmd[i]);
    }

    return static_cast<typename
        sha512_half_hasher::result_type>(h);
}

} // namespace hotstuff
} // namespace ripple