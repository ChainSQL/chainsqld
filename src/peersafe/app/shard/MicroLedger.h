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

#include <memory>
#include <utility>


namespace ripple {

class MicroLedger : public LedgerBase {
public:
    struct MicroLedgerHashSet {
        uint256     TxsRootHash;        // Transactions hashes root hash(Shamap not yet in use now).
        uint256     TxWMRootHash;       // Transactions with meta data root hash(Shamap not yet in use now).
        uint256     StateDeltaHash;     // StateDeltas root hash.
    };

    using Action = detail::RawStateTable::Action;

protected:
    LedgerIndex                                 mSeq;               // Ledger sequence.
    uint32                                      mShardID;           // The ID of the shard generated this MicroLedger.

    std::vector<TxID>                           mTxsHashes;         // All transactions hash set in this MicroLedger.
    std::unordered_map<TxID,
         std::pair<
         std::shared_ptr<Serializer const>,
		 std::shared_ptr<Serializer const>
         >>                                      mTxWithMetas;       // Serialized transactions with meta data maped by TxID;
    std::map<uint256,
        std::pair<
        Action, Serializer>,
        std::less<uint256>>                     mStateDeltas;       // The state changes by the transactions in this MicroLedger.

    MicroLedgerHashSet                          mHashSet;


    void computeHash();
    // std::vector<Blob>       mStateDeltas;                // The state changes by the transactions in this MicroLedger.
    // uint256                 mTxWMRootHash;              // The transactions with meta data hash(Shamap not yet in use now).

	void readMicroLedger(protocol::MicroLedger const& m);
	void readTxHashes(::google::protobuf::RepeatedPtrField<std::string> const& hashes);
	void readStateDelta(::google::protobuf::RepeatedPtrField<std::string> const& stateDeltas);
	void readTxWithMeta(protocol::MicroLedger_TxWithMeta const& txWithMetas);
public:
    MicroLedger() = delete;
	MicroLedger(protocol::TMMicroLedgerSubmit const& m);

    explicit MicroLedger(uint32 shardID_, LedgerIndex seq_, OpenView &view);

    inline LedgerIndex seq()
    {
        return mSeq;
    }

    inline uint32 shardID()
    {
        return mShardID;
    }

    inline auto& txHashes()
    {
        return mTxsHashes;
    }

    inline auto& stateDeltas()
    {
        return mStateDeltas;
    }

    inline MicroLedgerHashSet& hashSet()
    {
        return mHashSet;
    }

    inline void addTxID(TxID txID)
    {
        mTxsHashes.push_back(txID);
    }

    inline bool rawTxInsert(TxID const& key,
        std::shared_ptr<Serializer const> const& txn,
        std::shared_ptr<Serializer const> const& metaData)
    {
         return mTxWithMetas.emplace(key, std::make_pair(txn, metaData)).second;
    }

	inline bool hasTxWithMeta(TxID const& hash)
	{
		if (mTxWithMetas.find(hash) != mTxWithMetas.end())
			return true;
		return false;
	}

	//inline void setMetaIndex(TxID const& hash, uint32 index)
	//{
	//	mMapTxWithMeta[hash].second->resetIndex(index);
	//}
	std::pair<
		std::shared_ptr<Serializer const>,
		std::shared_ptr<Serializer const>
		> const& getTxWithMeta(TxID const& hash)
	{
		return mTxWithMetas[hash];
	}

    void addStateDelta(ReadView const& base, uint256 key, Action action, std::shared_ptr<SLE> sle);

    void compose(protocol::TMMicroLedgerSubmit& ms, bool withTxMeta);

	bool checkValidity(std::unique_ptr <ValidatorList> const& list, Blob signingData);
	Blob signingData();
};


}

#endif