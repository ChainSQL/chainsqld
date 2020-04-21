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

#ifndef PEERSAFE_APP_SHARD_SYNC_H_INCLUDED
#define PEERSAFE_APP_SHARD_SYNC_H_INCLUDED

#include "ripple.pb.h"
#include <ripple/beast/utility/Journal.h>
#include <ripple/overlay/impl/PeerImp.h>
#include <ripple/app/misc/ValidatorList.h>

#include <vector>

namespace ripple {

class Application;
class Config;
class ShardManager;


class Sync {

private:

    // Hold all Sync peers
    std::vector<std::weak_ptr <PeerImp>>                mPeers;

    // Hold all Sync validators
    std::unique_ptr <ValidatorList>                     mValidators;

    ShardManager&                                       mShardManager;

    Application&                                        app_;
    beast::Journal                                      journal_;
    Config&                                             cfg_;


public:

    Sync(ShardManager& m, Application& app, Config& cfg, beast::Journal journal);
    ~Sync() {}

    inline std::vector<std::weak_ptr <PeerImp>>& Peers()
    {
        return mPeers;
    }

    inline ValidatorList& Validators()
    {
        return *mValidators;
    }

    void onMessage(protocol::TMMicroLedgerWithTxsSubmit const& m);
    void onMessage(protocol::TMFinalLedgerSubmit const& m);
};

}

#endif
