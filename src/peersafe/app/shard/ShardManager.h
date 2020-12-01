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

#ifndef PEERSAFE_APP_SHARD_SHARDMANAGER_H_INCLUDED
#define PEERSAFE_APP_SHARD_SHARDMANAGER_H_INCLUDED

#include <ripple/basics/Log.h>
#include <ripple/core/Config.h>
#include <peersafe/app/shard/NodeBase.h>
#include <peersafe/app/shard/Lookup.h>
#include <peersafe/app/shard/Node.h>
#include <peersafe/app/shard/Committee.h>


namespace ripple {

class Application;


class ShardManager {

public:

    enum ShardRole {
        UNKNOWN     = Config::SHARD_ROLE_UNDEFINED,

        LOOKUP      = Config::SHARD_ROLE_LOOKUP,
        SHARD       = Config::SHARD_ROLE_SHARD,
        COMMITTEE   = Config::SHARD_ROLE_COMMITTEE,
        SYNC        = Config::SHARD_ROLE_SYNC,
    };

    static std::string to_string(ShardRole role)
    {
        switch (role)
        {
        case LOOKUP:
            return "LOOKUP";
        case SHARD:
            return "SHARD";
        case COMMITTEE:
            return "COMMITTEE";
        case SYNC:
            return "SYNC";
        default:
            return "UNKNOWN";
        }
    }

private:

    ShardRole                           mShardRole;

    std::shared_ptr<ripple::Lookup>     mLookup;

    std::shared_ptr<ripple::Node>       mNode;

    std::shared_ptr<ripple::Committee>  mCommittee;

    std::shared_ptr<ripple::NodeBase>   mNodeBase;

    Application&                        app_;
    Config&                             cfg_;
    beast::Journal                      j_;

    boost::shared_mutex mutable         mutex_;

public:

    ShardManager(Application& app, Config& cfg, Logs& log);

    ~ShardManager() {}

    inline ShardRole myShardRole()
    {
        return mShardRole;
    }

    inline ripple::Node& node()
    {
        return *mNode;
    }

    inline ripple::Lookup& lookup()
    {
        return *mLookup;
    }

    inline ripple::Committee& committee()
    {
        return *mCommittee;
    }

    inline ripple::NodeBase& nodeBase()
    {
        return *mNodeBase;
    }

	inline uint32 shardCount()
	{
		return cfg_.SHARD_COUNT;
	}

    void applyList(Json::Value const& list, PublicKey const& pubKey);
    void checkValidatorLists();
    bool checkNetQuorum();
};

}

#endif
