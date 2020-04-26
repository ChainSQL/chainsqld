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

#ifndef PEERSAFE_APP_SHARD_COMMITTEE_H_INCLUDED
#define PEERSAFE_APP_SHARD_COMMITTEE_H_INCLUDED

#include "ripple.pb.h"
#include <ripple/beast/utility/Journal.h>
#include <ripple/overlay/impl/PeerImp.h>
#include <ripple/app/misc/ValidatorList.h>

#include <vector>
#include <mutex>


namespace ripple {

class Application;
class Config;
class ShardManager;


class Committee {

private:

    // Used if I am a Committee node
    bool                                                mIsLeader;

    // Hold all commmittee peers
    std::vector<std::weak_ptr <PeerImp>>                mPeers;
    std::mutex                                          mPeersMutex;

    // Hold all committee validators
    std::unique_ptr <ValidatorList>                     mValidators;

    ShardManager&                                       mShardManager;

    Application&                                        app_;
    beast::Journal                                      journal_;
    Config&                                             cfg_;

public:

    Committee(ShardManager& m, Application& app, Config& cfg, beast::Journal journal);
    ~Committee() {}

    inline std::vector<std::weak_ptr <PeerImp>>& Peers()
    {
        return mPeers;
    }

    inline ValidatorList& validators()
    {
        return *mValidators;
    }

    inline std::unique_ptr<ValidatorList>& validatorsPtr()
    {
        return mValidators;
    }

    inline bool isLeader()
    {
        return mIsLeader;
    }

    void onConsensusStart(LedgerIndex seq, uint64 view, PublicKey const pubkey);

    void sendMessage(std::shared_ptr<Message> const &m);

    void onMessage(protocol::TMMicroLedgerSubmit const& m);

};

}

#endif
