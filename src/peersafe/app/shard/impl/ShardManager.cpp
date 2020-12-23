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
#include <ripple/core/ConfigSections.h>
#include <ripple/overlay/Peer.h>
#include <ripple/overlay/impl/PeerImp.h>
#include <ripple/app/misc/Transaction.h>
#include <ripple/app/misc/ValidatorSite.h>
#include <ripple/app/consensus/RCLValidations.h>
#include <boost/algorithm/string.hpp>
#include <memory>

namespace ripple
{

ShardManager::ShardManager(Application& app, Config& cfg, Logs& log)
    : app_(app)
    , cfg_(cfg)
    , j_(log.journal("shardManager"))
{

	mShardRole = (ShardRole)cfg.getShardRole();

    mLookup = std::make_unique<ripple::Lookup>(*this, app, cfg, log.journal("Lookup"));
    mNode = std::make_unique<ripple::Node>(*this, app, cfg, log.journal("Node"));
    mCommittee = std::make_unique<ripple::Committee>(*this, app, cfg, log.journal("Committee"));

	if (mShardRole == ShardRole::SHARD)
	{
		mNodeBase = mNode;
	}
	else if (mShardRole == ShardRole::COMMITTEE)
	{
		mNodeBase = mCommittee;
	}

    checkValidatorLists();
}

void ShardManager::applyList(Json::Value const& list, PublicKey const& pubKey)
{
    Json::Value const& newList = list["validators"];
    std::vector<PublicKey> lookupList, committeeList;
    std::map<uint32, std::vector<PublicKey>> shardLists;

    for (auto const& val : newList)
    {
        if (val.isObject() &&
            val.isMember("validation_public_key") && val["validation_public_key"].isString() &&
            val.isMember("shard_role") && val["shard_role"].isString())
        {
            std::pair<Blob, bool> ret(strUnHex(val["validation_public_key"].asString()));

            if (!ret.second || !ret.first.size())
            {
                JLOG(j_.error()) << "Invalid node identity: " << val["validation_public_key"].asString();
                continue;
            }

            std::uint32_t shardRole = UNKNOWN;
            std::vector<std::string> vecRoles;
            auto strRoles = val["shard_role"].asString();
            boost::split(vecRoles, strRoles, boost::is_any_of(","), boost::token_compress_on);

            for (auto item : vecRoles)
            {
                if (item == std::string("lookup"))         shardRole |= LOOKUP;
                else if (item == std::string("shard"))     shardRole |= SHARD;
                else if (item == std::string("committee")) shardRole |= COMMITTEE;
                else if (item == std::string("sync"))      shardRole |= SYNC;
            }

            if (shardRole & SHARD && shardRole ^ SHARD)
            {
                JLOG(j_.error()) << "Invalid role: shard role cannot combine other roles!";
                continue;
            }
            if (shardRole & COMMITTEE && shardRole ^ COMMITTEE)
            {
                JLOG(j_.error()) << "Invalid role : committee role cannot combine other roles!";
                continue;
            }

            std::uint32_t shardIndex = std::numeric_limits<std::uint32_t>::max();
            if (shardRole == SHARD)
            {
                if (val.isMember("shard_index") && val["shard_index"].isInt())
                {
                    shardIndex = val["shard_index"].asInt();
                }
                else
                {
                    JLOG(j_.error()) << "Invalid validator node : missing shard_index field";
                    continue;
                }
            }

            switch (shardRole)
            {
            case LOOKUP:
            case SYNC:
            case LOOKUP | SYNC:
                lookupList.push_back(PublicKey(Slice{ ret.first.data(), ret.first.size() }));
                break;
            case SHARD:
                shardLists[shardIndex].push_back(PublicKey(Slice{ ret.first.data(), ret.first.size() }));
                break;
            case COMMITTEE:
                committeeList.push_back(PublicKey(Slice{ ret.first.data(), ret.first.size() }));
                break;
            default:
                JLOG(j_.error()) << "Invalid validator node : Unknown role";
                break;
            }
        }
        else
        {
            JLOG(j_.error()) << "Invalid validator node: missing validation_public_key or shard_role field";
        }
    }

    mLookup->validators().applyNewList(list, pubKey, lookupList);

    auto iNew = shardLists.begin();
    auto iOld = mNode->shardValidators().begin();
    while (iNew != shardLists.end() || iOld != mNode->shardValidators().end())
    {
        if (iOld == mNode->shardValidators().end())
        {
            // Add a new shard
            mNode->shardValidators()[iNew->first] = std::make_unique<ValidatorList>(
                app_.validatorManifests(), app_.publisherManifests(), app_.timeKeeper(),
                j_, cfg_.VALIDATION_QUORUM);
            mNode->shardValidators()[iNew->first]->load(
                app_.getValidationPublicKey(),
                std::vector<std::string>{},
                cfg_.section(SECTION_VALIDATOR_LIST_KEYS).values(),
                (mShardRole == SHARD && mNode->shardID() == iNew->first));
            mNode->shardValidators()[iNew->first]->applyNewList(list, pubKey, iNew->second);
            ++iNew;
        }
        else if (iNew == shardLists.end() || iNew->first > iOld->first)
        {
            // Remove a shard
            mNode->shardValidators()[iOld->first]->applyNewList(list, pubKey, std::vector<PublicKey>{});
            ++iOld;
        }
        else
        {
            mNode->shardValidators()[iOld->first]->applyNewList(list, pubKey, iNew->second);
            ++iNew;
            ++iOld;
        }
    }

    mCommittee->validators().applyNewList(list, pubKey, committeeList);
}

void ShardManager::checkValidatorLists()
{
    // Setup trustKey and quorum
    if (mLookup->validators().shouldUpdate())
    {
        JLOG(j_.info()) << "Setup lookup validators trustKey and quorum";
        mLookup->validators().resetValidators();
        mLookup->validators().onConsensusStart();
        if (app_.config().section(SECTION_VALIDATOR_LIST_SITES).values().size() == 0)
        {
            mLookup->validators().clearShouldUpdate();
        }
    }

    bool checkShardCount = false;
    for (auto const& validators : mNode->shardValidators())
    {
        if (validators.second->shouldUpdate())
        {
            validators.second->resetValidators();
            if (mShardRole != SHARD || validators.first == mNode->shardID())
            {
                JLOG(j_.info()) << "Setup shard " << validators.first << " validators trustKey and quorum";
                validators.second->onConsensusStart();
            }
            if (app_.config().section(SECTION_VALIDATOR_LIST_SITES).values().size() == 0)
            {
                validators.second->clearShouldUpdate();
            }
            checkShardCount = true;
        }
    }

    // adjust shard count
    if (checkShardCount)
    {
        for (auto rit = mNode->shardValidators().rbegin(); rit != mNode->shardValidators().rend(); rit++)
        {
            if (rit->second->validators().size())
            {
                cfg_.SHARD_COUNT = rit->first;
                JLOG(j_.info()) << "SHARD_COUNT adjust to " << cfg_.SHARD_COUNT;
                break;
            }
        }
    }

    if (mCommittee->validators().shouldUpdate())
    {
        JLOG(j_.info()) << "Setup committee validators trustKey and quorum";
        mCommittee->validators().resetValidators();
        mCommittee->validators().onConsensusStart();
        if (app_.config().section(SECTION_VALIDATOR_LIST_SITES).values().size() == 0)
        {
            mCommittee->validators().clearShouldUpdate();
        }
    }
}

bool ShardManager::checkNetQuorum()
{
    switch ((int)mShardRole)
    {
    case LOOKUP:
    case LOOKUP | SYNC:
        return mNode->checkNetQuorum(true) && mCommittee->checkNetQuorum();
    case SHARD:
        return mNode->checkNetQuorum(false);
    case COMMITTEE:
        return mCommittee->checkNetQuorum();
    default:
        return true;
    }
}

}