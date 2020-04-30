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


#include <peersafe/app/shard/ShardManager.h>
#include <ripple/core/Config.h>
#include <ripple/overlay/Peer.h>
#include <ripple/overlay/impl/PeerImp.h>
#include <ripple/app/misc/Transaction.h>
#include <memory>

namespace ripple
{

ShardManager::ShardManager(Application& app, Config& cfg, Logs& log)
    : app_(app)
    , cfg_(cfg)
{

	mShardRole = (ShardRole)cfg.getShardRole();

    mLookup = std::make_unique<ripple::Lookup>(*this, app, cfg, log.journal("Lookup"));
    mNode = std::make_unique<ripple::Node>(*this, app, cfg, log.journal("Node"));
    mCommittee = std::make_unique<ripple::Committee>(*this, app, cfg, log.journal("Committee"));
    mSync = std::make_unique<ripple::Sync>(*this, app, cfg, log.journal("Sync"));

}


}