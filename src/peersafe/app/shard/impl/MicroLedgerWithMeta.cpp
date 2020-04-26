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


#include <peersafe/app/shard/MicroLedgerWithMeta.h>

namespace ripple {

MicroLedgerWithMeta::MicroLedgerWithMeta()
{
}

MicroLedgerWithMeta::MicroLedgerWithMeta(protocol::TMMicroLedgerWithTxsSubmit const& m)
	:MicroLedger(m)
{
	for (int i = 0; i < m.txwithmetas().size(); i++)
	{
		TxID txHash;
		protocol::TMTxWithMeta const& txWithMeta = m.txwithmetas(i);
		memcpy(txHash.begin(), txWithMeta.txhash().data(), 32);
		Blob rawTx, rawMeta;
		rawTx.assign(txWithMeta.rawtransaction().begin(), txWithMeta.rawtransaction().end());
		rawMeta.assign(txWithMeta.txmeta().begin(), txWithMeta.txmeta().end());
		auto meta = std::make_shared<TxMeta>(txHash,mSeq,rawMeta);
		mMapTxWithMeta.emplace(txHash, std::make_pair(rawTx, meta));
	}
}

bool MicroLedgerWithMeta::hasTx(TxID const& hash)
{
	if (mMapTxWithMeta.find(hash) != mMapTxWithMeta.end())
		return true;
	return false;
}

void MicroLedgerWithMeta::setMetaIndex(TxID const& hash, uint32 index)
{
	mMapTxWithMeta[hash].second->resetIndex(index);
}

bool MicroLedgerWithMeta::checkValidity(ValidatorList const& list, Blob signingData)
{
	if (!MicroLedger::checkValidity(list, signingData))
		return false;

	if (mTxsHashes.size() != mMapTxWithMeta.size())
		return false;

	//check all tx-meta corresponds to tx-hashes
	for (TxID hash : mTxsHashes)
	{
		if (mMapTxWithMeta.find(hash) == mMapTxWithMeta.end())
			return false;
	}
	return true;
}

TxWithMeta const& MicroLedgerWithMeta::getTxWithMeta(TxID const& hash)
{
	return mMapTxWithMeta[hash];
}

}
