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

#ifndef PEERSAFE_APP_SHARD_LEDGERBASE_H_INCLUDED
#define PEERSAFE_APP_SHARD_LEDGERBASE_H_INCLUDED

#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/RippleLedgerHash.h>
#include <ripple/protocol/PublicKey.h>

namespace ripple {

class LedgerBase {

protected:

    ripple::LedgerHash          mLedgerHash;
    std::map<PublicKey, Blob>   mSignatures;

public:
    
    LedgerBase() {}

    inline const ripple::LedgerHash& LedgerHash()
    {
        return mLedgerHash;
    }

    inline void setLedgerHash(const ripple::LedgerHash& ledgerHash)
    {
        mLedgerHash = ledgerHash;
    }

    inline const std::map<PublicKey, Blob> & Signatures()
    {
        return mSignatures;
    }

    inline auto addSignature(const PublicKey& pubkey, const Blob& sign)
    {
        return mSignatures.emplace(pubkey, sign);
    }
};

}

#endif