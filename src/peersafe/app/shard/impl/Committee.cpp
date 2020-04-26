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

#include <peersafe/app/shard/Committee.h>

namespace ripple {

Committee::Committee(ShardManager& m, Application& app, Config& cfg, beast::Journal journal)
    : mShardManager(m)
    , app_(app)
    , journal_(journal)
    , cfg_(cfg)
{
    // TODO
}

void Committee::onConsensusStart(LedgerIndex seq, uint64 view, PublicKey const pubkey)
{
    //mMicroLedger.reset();

    auto const& validators = mValidators->validators();
    assert(validators.size() > 0);
    int index = (view + seq) % validators.size();

    mIsLeader = (pubkey == validators[index]);

}

void Committee::sendMessage(std::shared_ptr<Message> const &m)
{
    std::lock_guard<std::mutex> lock(mPeersMutex);

    for (auto w : mPeers)
    {
        if (auto p = w.lock())
            p->send(m);
    }
}

void Committee::onMessage(protocol::TMMicroLedgerSubmit const& m)
{

}

}
