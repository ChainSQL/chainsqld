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

#ifndef PEERSAFE_APP_SHARD_LOOKUP_H_INCLUDED
#define PEERSAFE_APP_SHARD_LOOKUP_H_INCLUDED

#include "ripple.pb.h"
#include <ripple/basics/UnorderedContainers.h>
#include <ripple/beast/utility/Journal.h>
#include <ripple/overlay/impl/PeerImp.h>
#include <ripple/app/misc/ValidatorList.h>
#include <peersafe/app/shard/MicroLedger.h>
#include <peersafe/app/shard/FinalLedger.h>

#include <mutex>

namespace ripple {

class Application;
class Config;
class ShardManager;

class PeerImp;
class Peer;
class Transaction;

class Lookup {

private:

    // Hold all lookup peers
	std::vector<std::weak_ptr <PeerImp>>				mPeers;
	std::recursive_mutex								mPeersMutex;

    // Hold all Lookup validators
    std::unique_ptr <ValidatorList>                     mValidators;

    ShardManager&                                       mShardManager;

    Application&                                        app_;
    beast::Journal                                      journal_;
    Config&                                             cfg_;

	std::recursive_mutex								mutex_;
	using MapFinalLedger = std::map<LedgerIndex, std::shared_ptr<FinalLedger>>;
	MapFinalLedger										mMapFinalLedger;
	using MapMicroLedger = std::map<uint32, std::shared_ptr<MicroLedger>>;
	std::map<LedgerIndex, MapMicroLedger>				mMapMicroLedgers;

    std::atomic_flag                                    mSaveLedgerThread = ATOMIC_FLAG_INIT;
    LedgerIndex                                         mNetLedger = 0;

	std::mutex											mTransactionsMutex;
    std::uint32_t                                       mRelayInterval = 500;   // ms
	boost::asio::basic_waitable_timer<
		std::chrono::system_clock>                      mTimer;

    bool                                                mInitialized = false;

public:

    Lookup(ShardManager& m, Application& app, Config& cfg, beast::Journal journal);
    ~Lookup() {}

    inline ValidatorList& validators()
    {
        return *mValidators;
    }

    inline void saveMicroLedger(std::shared_ptr<MicroLedger> microLedger)
    {
        std::lock_guard <std::recursive_mutex> lock(mutex_);
        // Check microLedger
        if (mMapFinalLedger.count(microLedger->seq()))
        {
            if (microLedger->ledgerHash() != mMapFinalLedger[microLedger->seq()]->getMicroLedgerHash(microLedger->shardID()))
            {
                return;
            }
        }
        // Save microLedger into map
        mMapMicroLedgers[microLedger->seq()][microLedger->shardID()] = microLedger;
    }
    inline void saveFinalLedger(std::shared_ptr<FinalLedger> finalLedger)
    {
        std::lock_guard <std::recursive_mutex> lock(mutex_);
        // Save finalLedger into map
        mMapFinalLedger[finalLedger->seq()] = finalLedger;
        // Check received microLedgers
        for (auto const& m : mMapMicroLedgers[finalLedger->seq()])
        {
            if (m.second->ledgerHash() != finalLedger->getMicroLedgerHash(m.first))
            {
                mMapMicroLedgers[finalLedger->seq()].erase(m.first);
            }
        }
    }

    inline std::shared_ptr<MicroLedger> getMicroLedger(LedgerIndex seq, uint32 shardID)
    {
        std::lock_guard <std::recursive_mutex> lock(mutex_);
        assert(mMapMicroLedgers.count(seq));
        assert(mMapMicroLedgers[seq].count(shardID));
        return mMapMicroLedgers[seq][shardID];
    }

    inline std::shared_ptr<FinalLedger> getFinalLedger(LedgerIndex seq)
    {
        std::lock_guard <std::recursive_mutex> lock(mutex_);
        assert(mMapFinalLedger.count(seq));
        return mMapFinalLedger[seq];
    }

	void addActive(std::shared_ptr<PeerImp> const& peer);
	void eraseDeactivate();

    void onMessage(std::shared_ptr<protocol::TMMicroLedgerSubmit> const& m);
    void onMessage(std::shared_ptr<protocol::TMFinalLedgerSubmit> const& m);
    void onMessage(std::shared_ptr<protocol::TMCommitteeViewChange> const& m);

    void sendMessage(std::shared_ptr<Message> const &m);
    void distributeMessage(std::shared_ptr<Message> const &m, bool forceBroadcast = false);

	void checkSaveLedger(LedgerIndex seq);
    void saveLedgerThread();
    bool findNewLedgerToSave(LedgerIndex &toSaveOrAcquire);
    bool checkLedger(LedgerIndex seq);
    uint32 resetMetaIndex(LedgerIndex seq);
	void saveLedger(LedgerIndex seq);

	void setTimer();
	void onTimer(boost::system::error_code const& ec);

    void relayTxs();

	// shard related
    static unsigned int getTxShardIndex(const std::string& strAddress, unsigned int numShards);

    template <class UnaryFunc>
    void
    for_each(UnaryFunc&& f)
    {
        std::lock_guard<std::recursive_mutex> lock(mPeersMutex);

        // Iterate over a copy of the peer list because peer
        // destruction can invalidate iterators.
        std::vector<std::weak_ptr<PeerImp>> wp;
        wp.reserve(mPeers.size());

        for (auto& x : mPeers)
            wp.push_back(x);

        for (auto& w : wp)
        {
            if (auto p = w.lock())
                f(std::move(p));
        }
    }
};

}

#endif
