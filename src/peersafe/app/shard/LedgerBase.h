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
#include <ripple/app/misc/ValidatorList.h>
#include "ripple.pb.h"

namespace ripple {

class LedgerBase {

protected:

    ripple::LedgerHash          mLedgerHash;
    std::map<PublicKey, Blob>   mSignatures;

public:
    
    LedgerBase() {}

    inline const ripple::LedgerHash& ledgerHash()
    {
        return mLedgerHash;
    }

    inline const ripple::LedgerHash& ledgerHash() const
    {
        return mLedgerHash;
    }

    inline void setLedgerHash(const ripple::LedgerHash& ledgerHash)
    {
        mLedgerHash = ledgerHash;
    }

    inline const std::map<PublicKey, Blob> & signatures()
    {
        return mSignatures;
    }

    inline auto addSignature(const PublicKey& pubkey, const Blob& sign)
    {
        return mSignatures.emplace(pubkey, sign);
    }

	inline void readSignature(const ::google::protobuf::RepeatedPtrField< ::protocol::Signature >& signatures)
	{
		for (int i = 0; i < signatures.size(); i++)
		{
			protocol::Signature const& sig = signatures.Get(i);
			auto const publicKey = parseBase58<PublicKey>(
				TokenType::TOKEN_NODE_PUBLIC, sig.publickey());
			if (publicKey)
			{
				Blob signature;
				signature.assign(sig.signature().begin(), sig.signature().end());
				addSignature(*publicKey, signature);
			}
		}
	}

	inline virtual bool checkValidity(std::unique_ptr <ValidatorList> const& list)
	{
		//check signature
		for (auto iter = mSignatures.begin(); iter != mSignatures.end(); iter++)
		{
			boost::optional<PublicKey> pubKey = list->getTrustedKey(iter->first);
			if (!pubKey)
				return false;
			bool validSig = verifyDigest(
				iter->first,
				mLedgerHash,
				makeSlice(iter->second));
			if (!validSig)
				return false;
		}
		if (mSignatures.size() < list->quorum())
			return false;

		return true;
	}
};

}

#endif