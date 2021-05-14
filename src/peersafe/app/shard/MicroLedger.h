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

#ifndef PEERSAFE_APP_SHARD_MICROLEDGER_H_INCLUDED
#define PEERSAFE_APP_SHARD_MICROLEDGER_H_INCLUDED

#include "ripple.pb.h"
#include <peersafe/app/shard/LedgerBase.h>
#include <ripple/protocol/Protocol.h>
#include <ripple/ledger/OpenView.h>
#include <ripple/ledger/detail/RawStateTable.h>
#include <ripple/app/ledger/Ledger.h>
#include <ripple/beast/utility/Journal.h>

#include <memory>
#include <utility>


namespace ripple {

class Application;

class MicroLedger : public LedgerBase {
public:
    struct MicroLedgerHashSet {
        uint256     TxsRootHash;        // Transactions hashes root hash(Shamap not yet in use now).
        uint256     TxWMRootHash;       // Transactions with meta data root hash(Shamap not yet in use now).
        uint256     StateDeltaHash;     // StateDeltas root hash.
    };

    using Action = detail::RawStateTable::Action;
    using TxMetaPair = std::pair<std::shared_ptr<Serializer const>,
		std::shared_ptr<Serializer>>;

protected:
    LedgerIndex                                 mSeq;               // Ledger sequence.
    uint64                                      mViewChange;        // View change sequence.
    uint32                                      mShardID;           // The ID of the shard generated this MicroLedger.
    uint32                                      mShardCount;
    std::int64_t                                mDropsDestroyed;    //

    std::unordered_map<TxID, uint256>           mTxsHashes;         // All transactions hash and tree node hash in SHAMap.
    std::unordered_map<TxID, TxMetaPair>        mTxWithMetas;       // Serialized transactions with meta data maped by TxID;

    std::map<uint256,
        std::pair<
        Action, Serializer>,
        std::less<uint256>>                     mStateDeltas;       // The state changes by the transactions in this MicroLedger.

    MicroLedgerHashSet                          mHashSet;


    void computeHash(bool withTxMeta);
    uint256 computeTxWithMetaHash();

	void readMicroLedger(protocol::MicroLedger const& m);
	void readTxHashes(::google::protobuf::RepeatedPtrField<::protocol::TxHash> const& hashes);
	void readStateDelta(::google::protobuf::RepeatedPtrField<::protocol::StateDelta> const& stateDeltas);
    void readTxWithMeta(::google::protobuf::RepeatedPtrField <::protocol::TxWithMeta> const& txWithMetas);

public:
    MicroLedger() = delete;
	MicroLedger(protocol::TMMicroLedgerSubmit const& m, bool withTxMeta = true);
    MicroLedger(uint64 viewChange, uint32 shardID_, uint32 shardCount, LedgerIndex seq_, OpenView const& view);
    MicroLedger(uint64 viewChange, uint32 shardID_, uint32 shardCount, LedgerIndex seq_, OpenView const& view, std::shared_ptr<CanonicalTXSet const> txSet);

    inline LedgerIndex seq()
    {
        return mSeq;
    }

    inline LedgerIndex seq() const
    {
        return mSeq;
    }

    inline uint32 shardID()
    {
        return mShardID;
    }

    inline uint32 shardID() const
    {
        return mShardID;
    }

    inline uint32 shardCount() const
    {
        return mShardCount;
    }

    inline auto& txHashes()
    {
        return mTxsHashes;
    }

    inline auto const& txHashes() const
    {
        return mTxsHashes;
    }

    inline size_t txCounts() const
    {
        return mTxsHashes.size();
    }

    inline auto& stateDeltas()
    {
        return mStateDeltas;
    }

    inline MicroLedgerHashSet& hashSet()
    {
        return mHashSet;
    }

    inline uint256 txRootHash()
    {
        return mHashSet.TxsRootHash;
    }

    inline void addTxID(TxID txID, uint256 nh)
    {
        mTxsHashes.emplace(txID, nh);
    }

    inline void setDropsDestroyed(std::int64_t drops)
    {
        mDropsDestroyed = drops;
    }

    inline bool rawTxInsert(TxID const& key,
        std::shared_ptr<Serializer const> const& txn,
        std::shared_ptr<Serializer const> const& metaData)
    {
		std::shared_ptr<Serializer> meta = std::const_pointer_cast<Serializer>(metaData);
        return mTxWithMetas.emplace(key, std::make_pair(txn, meta)).second;
    }

	inline bool hasTxWithMeta(TxID const& hash)
	{
        return (mTxWithMetas.find(hash) != mTxWithMetas.end()) ? true : false;
	}

    inline TxMetaPair const& getTxWithMeta(TxID const& hash)
    {
        return mTxWithMetas[hash];
    }

    inline bool isEmptyLedger()
    {
        return mHashSet.TxsRootHash == zero &&
            mHashSet.TxWMRootHash == zero &&
            mHashSet.StateDeltaHash == zero;
    }

	void setMetaIndex(TxID const& hash, uint32 index, beast::Journal& j);

    void addStateDelta(ReadView const& base, uint256 key, Action action, std::shared_ptr<SLE> sle);

    void compose(protocol::TMMicroLedgerSubmit& ms, bool withTxMeta) const;

	bool checkValidity(std::unique_ptr <ValidatorList> const& list, bool withTxMeta);

    bool sameShard(std::shared_ptr<SLE>& sle, Application& app) const;

    void applyAccountRoot(
        OpenView& to,
        detail::RawStateTable::Action action,
        std::shared_ptr<SLE>& sle,
        beast::Journal& j,
        Application& app) const;
    void applyRippleState(
        OpenView& to,
        detail::RawStateTable::Action action,
        std::shared_ptr<SLE>& sle,
        beast::Journal& j,
        Application& app) const;
    void applyDirNode(
        OpenView& to,
        detail::RawStateTable::Action action,
        std::shared_ptr<SLE>& sle,
        beast::Journal& j) const;
    void applyEscrow(
        OpenView& to,
        detail::RawStateTable::Action action,
        std::shared_ptr<SLE>& sle,
        beast::Journal& j,
        Application& app) const;
    void applyTableList(
        OpenView& to,
        detail::RawStateTable::Action action,
        std::shared_ptr<SLE>& sle,
        beast::Journal& j,
        Application& app) const;
    void applyFeeSetting(
        OpenView& to,
        std::shared_ptr<SLE>& sle,
        beast::Journal& j) const;
    void applyAmendments(
        OpenView& to,
        std::shared_ptr<SLE>& sle,
        beast::Journal& j) const;
    void applyCommons(
        OpenView& to,
        detail::RawStateTable::Action action,
        std::shared_ptr<SLE>& sle,
        beast::Journal& j) const;
    void apply(OpenView& to, beast::Journal& j, Application& app) const;

    void apply(Ledger& to) const;
};


class CachedMLs
{
public:
    using digest_type = uint256;

    using value_type = std::shared_ptr<MicroLedger>;

    CachedMLs(CachedMLs const&) = delete;
    CachedMLs& operator= (CachedMLs const&) = delete;

    template <class Rep, class Period>
    CachedMLs(std::chrono::duration<
        Rep, Period> const& timeToLive,
        Stopwatch& clock)
        : timeToLive_(timeToLive)
        , map_(clock)
    {
    }

    /** Discard expired entries.

        Needs to be called periodically.
    */
    void expire();

    /** Fetch an item from the cache.

        If the digest was not found, Handler
        will be called with this signature:

            std::shared_ptr<MicroLedger const>(void)
    */
    value_type fetch(digest_type const& digest);

    value_type emplace(digest_type const& digest, value_type const& value);

    std::size_t size() const
    {
        return map_.size();
    }

    void clear()
    {
        return map_.clear();
    }

    template<class fn>
    void for_each(fn&& f)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        for (auto& it : map_)
        {
            f(it.second);
        }
    }

private:
    std::mutex mutable mutex_;
    Stopwatch::duration timeToLive_;
    beast::aged_unordered_map <digest_type,
        value_type, Stopwatch::clock_type,
        hardened_hash<strong_hash>> map_;
};

}

#endif