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


#include <peersafe/app/shard/FinalLedger.h>
#include <ripple/basics/Slice.h>

namespace ripple {

FinalLedger::FinalLedger()
{

}

FinalLedger::FinalLedger(protocol::TMFinalLedgerSubmit const& m)
{
	protocol::FinalLedger const& finalLedger = m.finalledger();
	mSeq = finalLedger.ledgerseq();
	mDrops = finalLedger.drops();
	mCloseTime = finalLedger.closetime();
	mCloseTimeResolution = finalLedger.closetimeresolution();
	mCloseFlags = finalLedger.closeflags();

	assert(finalLedger.txhashes().size() % 256 == 0);
	for (int i = 0; i < finalLedger.txhashes().size(); i++)
	{
		TxID txHash;
		memcpy(txHash.begin(), finalLedger.txhashes(i).data(), 32);
		mTxsHashes.push_back(txHash);
	}

	memcpy(mTxShaMapRootHash.begin(), finalLedger.txshamaproothash().data(), 32);
	memcpy(mStateShaMapRootHash.begin(), finalLedger.stateshamaproothash().data(), 32);

	std::vector<Blob> vecDeltas;
	for (int i = 0; i < finalLedger.statedeltas().size(); i++)
	{
		Blob delta;
		delta.assign(finalLedger.statedeltas(i).begin(), finalLedger.statedeltas(i).end());
		vecDeltas.push_back(delta);
	}
	mStateDelta.deSerialize(vecDeltas);

	for (int i = 0; i < finalLedger.microledgers().size(); i++)
	{
		protocol::FinalLedger_MLInfo const& mbInfo = finalLedger.microledgers(i);
		ripple::LedgerHash mbHash;
		memcpy(mbHash.begin(), mbInfo.mbhash().data(), 32);
		mMicroLedgers[mbInfo.shardid()] = mbHash;
	}

	readSignature(m.signatures());
}

protocol::TMFinalLedgerSubmit FinalLedger::ToTMMessage()
{
	protocol::TMFinalLedgerSubmit msg;

	return msg;
}

Blob FinalLedger::getSigningData()
{
	Serializer s;
	s.add32(mSeq);
	s.add64(mDrops);
	s.add32(mCloseTime);
	s.add32(mCloseTimeResolution);
	s.add32(mCloseFlags);
	s.add256(mTxShaMapRootHash);
	s.add256(mStateShaMapRootHash);

	return s.getData();
}

LedgerInfo FinalLedger::getLedgerInfo()
{
	LedgerInfo info;
	info.seq = mSeq;
	info.txHash = mTxShaMapRootHash;
	info.drops = mDrops;
	info.closeFlags = mCloseFlags;
	info.closeTimeResolution = NetClock::duration{ mCloseTimeResolution };
	info.closeTime = NetClock::time_point{ NetClock::duration{mCloseTime} };
	info.accountHash = mStateShaMapRootHash;
	
	return std::move(info);
}

detail::RawStateTable const& FinalLedger::getRawStateTable()
{
	return mStateDelta;
}

std::vector<TxID> const& FinalLedger::getTxHashes()
{
	return mTxsHashes;
}


}
