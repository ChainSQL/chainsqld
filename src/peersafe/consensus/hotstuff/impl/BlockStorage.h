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

#ifndef RIPPLE_CONSENSUS_HOTSTUFF_BLOCKSTORAGE_H
#define RIPPLE_CONSENSUS_HOTSTUFF_BLOCKSTORAGE_H

#include <map>

#include <peersafe/consensus/hotstuff/impl/HotstuffCore.h>
#include <peersafe/consensus/hotstuff/impl/Block.h>

namespace ripple { namespace hotstuff {

class BlockStorage : public Storage {
public:
    BlockStorage();
    virtual ~BlockStorage();

    // for blocks
    bool addBlock(const Block& block) override;
    // 基于区块高度释放不再需要用到的区块
    void gcBlocks(const Block& block) override;
    // 通过 block hash 获取 block，如果本地没有函数返回 false
    bool blockOf(const BlockHash& hash, Block& block) const override;
    // 通过 block hash 获取 block, 如果本地没有则需要从网络同步
    bool expectBlock(const BlockHash& hash, Block& block) override;

private:
    void recurseGCBlocks(const Block& block);
    std::map<BlockHash, Block> cache_blocks_;
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_BLOCKSTORAGE_H