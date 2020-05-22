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

#ifndef PEERSAFE_APP_SHARD_FINALLEDGER_H_INCLUDED
#define PEERSAFE_APP_SHARD_FINALLEDGER_H_INCLUDED


#include <peersafe/app/shard/LedgerBase.h>
#include <peersafe/app/shard/MicroLedger.h>
#include <ripple/ledger/OpenView.h>
#include <ripple/app/ledger/Ledger.h>
#include <ripple/protocol/Protocol.h>
#include <ripple/ledger/detail/RawStateTable.h>
#include "ripple.pb.h"

namespace ripple {


class FinalLedger : public LedgerBase {

private:
    LedgerIndex                     mSeq;                       // Ripple::Ledger sequence.
    LedgerHash                      mHash;                      // Ripple::Ledger hash
    LedgerHash                      mParentHash;
    uint64                          mDrops;
    uint32                          mCloseTime;
    uint32                          mCloseTimeResolution;
    uint32                          mCloseFlags;

    std::vector<TxID>               mTxsHashes;                 // All transactions hash set in this FinalBlock.
    uint256                         mTxShaMapRootHash;          // The final transactions Shamap root hash.

    detail::RawStateTable           mStateDelta;                // The state changes by the transactions in this FinalLedger.
    uint256                         mStateShaMapRootHash;       // The final state Shamap root hash.

    std::map<uint32, ripple::LedgerHash>    mMicroLedgers;      // The MicroLedger hash set in this FinalLedger.
    uint256                         mMicroLedgerSetHash;        // 

public:
    FinalLedger() = delete;
    FinalLedger(
        OpenView const& view,
        std::shared_ptr<Ledger const>ledger,
        std::vector<std::shared_ptr<MicroLedger const>> const& microLedgers);
	FinalLedger(protocol::TMFinalLedgerSubmit const& m);

    void computeHash();

    void compose(protocol::TMFinalLedgerSubmit& ms);

    inline LedgerIndex seq()
    {
        return mSeq;
    }

    inline LedgerHash hash()
    {
        return mHash;
    }

    inline LedgerHash parentHash()
    {
        return mParentHash;
    }

    inline detail::RawStateTable const& getRawStateTable()
    {
        return mStateDelta;
    }

    inline auto getTxHashes()
        -> std::vector<TxID> const&
    {
        return mTxsHashes;
    }

    inline LedgerHash getMicroLedgerHash(uint32 shardID)
    {
        if (mMicroLedgers.find(shardID) != mMicroLedgers.end())
        {
            return mMicroLedgers[shardID];
        }
        else
        {
            return zero;
        }
    }

	LedgerInfo getLedgerInfo();

    void apply(Ledger& to, bool withTxs = true);

};

}

#endif
