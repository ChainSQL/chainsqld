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
#include <ripple/protocol/Protocol.h>


namespace ripple {

class FinalLedger : public LedgerBase {

private:
    LedgerIndex                     mSeq;                       // Ledger sequence.

    std::vector<TxID>               mTxsHashes;                 // All transactions hash set in this FinalBlock.
    uint256                         mTxShaMapRootHash;          // The final transactions Shamap root hash.

    Blob                            mStateDelta;                // The state changes by the transactions in this FinalLedger.
    uint256                         mStateShaMapRootHash;       // The final state Shamap root hash.

    std::map<uint32, ripple::LedgerHash>    mMicroLedgers;      // The MicroLedger hash set in this FinalLedger.

public:
    FinalLedger();

};

}

#endif