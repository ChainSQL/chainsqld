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


#include <peersafe/app/shard/LedgerBase.h>
#include <ripple/protocol/Protocol.h>


namespace ripple {

class MicroLedger : public LedgerBase {

private:
    LedgerIndex             mSeq;                       // Ledger sequence.
    uint32                  mShardID;                   // The ID of the shard generated this MicroLedger.
    std::vector<TxID>       mTxsHashes;                 // All transactions hash set in this MicroLedger.
    Blob                    mStateDelta;                // The state changes by the transactions in this MicroLedger.
    uint256                 mTxWMRootHash;              // The transactions with meta data hash(Shamap not yet in use now).

public:
    MicroLedger();

};

}

#endif