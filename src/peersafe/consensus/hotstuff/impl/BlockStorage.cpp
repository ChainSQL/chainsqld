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
//=============================================================================


#include <peersafe/consensus/hotstuff/impl/BlockStorage.h>


namespace ripple { namespace hotstuff {

BlockStorage::BlockStorage()
: cache_blocks_() {

}

BlockStorage::~BlockStorage() {

}

bool BlockStorage::addBlock(const Block& block) {
    if(cache_blocks_.find(block.hash) != cache_blocks_.end())
        return false;
    
    cache_blocks_[block.hash] = block;
    return true;
}

void BlockStorage::gcBlocks(const Block& block) {
    Block parent = block;
    for(int i = 0; i < 50; i++) {
        if(blockOf(parent.parent, parent) == false)
            return;
    }

    recurseGCBlocks(parent);
}

// 通过 block hash 获取 block，如果本地没有函数返回 false
bool BlockStorage::blockOf(const BlockHash& hash, Block& block) const {
    auto it = cache_blocks_.find(hash);
    if(it == cache_blocks_.end()) {
        return false;
    }
    
    block = it->second;
    return true;
}

// 通过 block hash 获取 block, 如果本地没有则需要从网络同步
bool BlockStorage::expectBlock(const BlockHash& hash, Block& block) {
    if(blockOf(hash, block))
        return true;
    return false;
}

void BlockStorage::recurseGCBlocks(const Block& block) {
    Block parent;
    if(blockOf(block.parent, parent)) {
        recurseGCBlocks(parent);
    }

    auto it = cache_blocks_.find(block.hash);
    if(it != cache_blocks_.end())
        cache_blocks_.erase(it);
}

} // hotstuff
} // ripple