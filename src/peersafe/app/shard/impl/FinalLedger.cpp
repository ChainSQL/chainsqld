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
#include <ripple/protocol/digest.h>

namespace ripple {

FinalLedger::FinalLedger(
    OpenView const& view,
    std::shared_ptr<Ledger const>ledger,
    std::vector<std::shared_ptr<MicroLedger const>> const& microLedgers)
    : mStateDelta(std::move(view.items()))
{
    mSeq = ledger->seq();
    mHash = ledger->info().hash;
    mDrops = ledger->info().drops.drops();
    mCloseTime = ledger->info().closeTime.time_since_epoch().count();
    mCloseTimeResolution = ledger->info().closeTimeResolution.count();
    mCloseFlags = ledger->info().closeFlags;

    mTxShaMapRootHash = ledger->info().txHash;
    mStateShaMapRootHash = ledger->info().accountHash;

    for (auto const& microLedger : microLedgers)
    {
        mTxsHashes.insert(mTxsHashes.end(), microLedger->txHashes().begin(), microLedger->txHashes().end());

        mMicroLedgers.emplace(microLedger->shardID(), microLedger->ledgerHash());
    }

    computeHash();
}

void FinalLedger::computeHash()
{
    using beast::hash_append;

    if (mMicroLedgers.size() > 0)
    {
        sha512_half_hasher h;
        for (auto const& it : mMicroLedgers)
        {
            hash_append(h, it.second);
        }
        mMicroLedgerSetHash = static_cast<typename sha512_half_hasher::result_type>(h);
    }
    else
    {
        mMicroLedgerSetHash = zero;
    }

    setLedgerHash(sha512Half(
        mSeq,
        mHash,
        mDrops,
        mCloseTime,
        mCloseTimeResolution,
        mCloseFlags,
        mTxShaMapRootHash,
        mStateShaMapRootHash,
        mMicroLedgerSetHash));
}

FinalLedger::FinalLedger(protocol::TMFinalLedgerSubmit const& m)
{
	protocol::FinalLedger const& finalLedger = m.finalledger();
	mSeq = finalLedger.ledgerseq();
    memcpy(mHash.begin(), finalLedger.ledgerhash().data(), 32);
	mDrops = finalLedger.drops();
	mCloseTime = finalLedger.closetime();
	mCloseTimeResolution = finalLedger.closetimeresolution();
	mCloseFlags = finalLedger.closeflags();

	for (int i = 0; i < finalLedger.txhashes().size(); i++)
	{
		TxID txHash;
		memcpy(txHash.begin(), finalLedger.txhashes(i).data(), 32);
		mTxsHashes.push_back(txHash);
	}

    if (finalLedger.has_txshamaproothash())
    {
        memcpy(mTxShaMapRootHash.begin(), finalLedger.txshamaproothash().data(), 32);
    }
    else
    {
        mTxShaMapRootHash = zero;
    }

    if (finalLedger.has_stateshamaproothash())
    {
        memcpy(mStateShaMapRootHash.begin(), finalLedger.stateshamaproothash().data(), 32);
    }
    else
    {
        mStateShaMapRootHash = zero;
    }

    for (auto const& stateDelta : finalLedger.statedeltas())
    {
        uint256 key;
        memcpy(key.begin(), stateDelta.key().data(), 32);

        switch ((detail::RawStateTable::Action)stateDelta.action())
        {
        case detail::RawStateTable::Action::insert:
            mStateDelta.insert(std::make_shared<SLE>(makeSlice(stateDelta.sle()), key));
            break;
        case detail::RawStateTable::Action::erase:
            mStateDelta.erase(std::make_shared<SLE>(makeSlice(stateDelta.sle()), key));
            break;
        case detail::RawStateTable::Action::replace:
            mStateDelta.replace(std::make_shared<SLE>(makeSlice(stateDelta.sle()), key));
            break;
        default:
            break;
        }
    }

	for (int i = 0; i < finalLedger.microledgers().size(); i++)
	{
		protocol::FinalLedger_MLInfo const& mbInfo = finalLedger.microledgers(i);
		ripple::LedgerHash mbHash;
		memcpy(mbHash.begin(), mbInfo.mlhash().data(), 32);
		mMicroLedgers[mbInfo.shardid()] = mbHash;
	}

	readSignature(m.signatures());

    computeHash();
}

void FinalLedger::compose(protocol::TMFinalLedgerSubmit& ms)
{
    protocol::FinalLedger& m = *(ms.mutable_finalledger());

    m.set_ledgerseq(mSeq);
    m.set_ledgerhash(mHash.data(), mHash.size());
    m.set_drops(mDrops);
    m.set_closetime(mCloseTime);
    m.set_closetimeresolution(mCloseTimeResolution);
    m.set_closeflags(mCloseFlags);

    // Transaction hashes
    for (auto const& it : mTxsHashes)
    {
        m.add_txhashes(it.data(), it.size());
    }

    // Tx shamap root hash
    if (mTxShaMapRootHash != zero)
    {
        m.set_txshamaproothash(mTxShaMapRootHash.data(),
            mTxShaMapRootHash.size());
    }

    if (mStateShaMapRootHash != zero)
    {
        m.set_stateshamaproothash(mStateShaMapRootHash.data(),
            mStateShaMapRootHash.size());
    }

    // Statedetals.
    for (auto const& it : mStateDelta.items())
    {
        protocol::StateDelta& s = *m.add_statedeltas();
        s.set_key(it.first.data(), it.first.size());                // key
        s.set_action((uint32)it.second.first);                      // action
        s.set_sle(it.second.second->getSerializer().data(),
            it.second.second->getSerializer().size());              // sle
    }

    // Micro ledger infos.
    for (auto const& microLeger : mMicroLedgers)
    {
        protocol::FinalLedger_MLInfo& mInfo = *m.add_microledgers();
        mInfo.set_shardid(microLeger.first);
        mInfo.set_mlhash(microLeger.second.data(), microLeger.second.size());
    }

    // Signatures
    for (auto it : signatures())
    {
        protocol::Signature& s = *ms.add_signatures();
        s.set_publickey((const char *)it.first.data(), it.first.size());
        s.set_signature(it.second.data(), it.second.size());
    }
}

LedgerInfo FinalLedger::getLedgerInfo()
{
	LedgerInfo info;
	info.seq = mSeq;
    info.hash = mHash;
	info.txHash = mTxShaMapRootHash;
	info.drops = mDrops;
	info.closeFlags = mCloseFlags;
	info.closeTimeResolution = NetClock::duration{ mCloseTimeResolution };
	info.closeTime = NetClock::time_point{ NetClock::duration{mCloseTime} };
	info.accountHash = mStateShaMapRootHash;
	
	return std::move(info);
}

void FinalLedger::apply(Ledger& to)
{
    mStateDelta.destroyZXC(to.info().drops - mDrops);

    mStateDelta.apply(to);

    for (auto const& tx : mTxsHashes)
    {
        to.rawTxInsert(tx,
            std::make_shared<Serializer>(0),
            std::make_shared<Serializer>(0));
    }
}


}
