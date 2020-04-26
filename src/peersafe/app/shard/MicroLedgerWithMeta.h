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

#ifndef PEERSAFE_APP_SHARD_MICROLEDGER_WITHMETA_H_INCLUDED
#define PEERSAFE_APP_SHARD_MICROLEDGER_WITHMETA_H_INCLUDED


#include <peersafe/app/shard/LedgerBase.h>
#include <peersafe/app/shard/MicroLedger.h>
#include <ripple/protocol/Protocol.h>
#include <ripple/protocol/STTx.h>
#include <ripple/ledger/TxMeta.h>
#include "ripple.pb.h"


namespace ripple {
class ValidatorList;
using TxWithMeta = std::pair<Blob, std::shared_ptr<TxMeta>>;
class MicroLedgerWithMeta : public MicroLedger {
private:
	std::map<TxID, TxWithMeta>	mMapTxWithMeta;

public:
	MicroLedgerWithMeta();
	MicroLedgerWithMeta(protocol::TMMicroLedgerWithTxsSubmit const& m);

	bool hasTx(TxID const& hash);
	void setMetaIndex(TxID const& hash,uint32 index);
	bool checkValidity(ValidatorList const& list, Blob signingData);
	TxWithMeta const& getTxWithMeta(TxID const& hash);
};

}

#endif