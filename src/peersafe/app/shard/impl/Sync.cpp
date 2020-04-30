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

#include <peersafe/app/shard/Sync.h>
#include <ripple/overlay/Peer.h>
#include <ripple/overlay/impl/PeerImp.h>

namespace ripple {

Sync::Sync(ShardManager& m, Application& app, Config& cfg, beast::Journal journal)
    : mShardManager(m)
    , app_(app)
    , journal_(journal)
    , cfg_(cfg)
{
    // TODO
}

void Sync::addActive(std::shared_ptr<PeerImp> const& peer)
{
	//auto const result = mPeers.emplace(peer);
	//assert(result.second);
	//(void)result.second;
}

void Sync::eraseDeactivate(Peer::id_t id)
{
	//std::lock_guard <decltype(mPeersMutex)> lock(mPeersMutex);
	//mPeers.erase(id);
}

void Sync::onMessage(protocol::TMMicroLedgerSubmit const& m)
{

}

void Sync::onMessage(protocol::TMFinalLedgerSubmit const& m)
{

}

}
