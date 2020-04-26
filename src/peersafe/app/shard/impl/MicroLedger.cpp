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


#include <peersafe/app/shard/MicroLedger.h>
#include <ripple/protocol/digest.h>
#include <ripple/protocol/Keylet.h>


namespace ripple {

MicroLedger::MicroLedger(uint32 shardID_, LedgerIndex seq_, OpenView &view)
    : mSeq(seq_)
    , mShardID(shardID_)
{
    assert(!view.open());

    view.apply(*this);

    computeHash();
}

void MicroLedger::computeHash()
{
    using beast::hash_append;
    sha512_half_hasher txRootHash;
    sha512_half_hasher txWMRootHash;

    for (auto iter = mTxsHashes.begin(); iter != mTxsHashes.end(); iter++)
    {
        hash_append(txRootHash, *iter);

        hash_append(txWMRootHash, mTxWithMetas.at(*iter).first->slice());
        hash_append(txWMRootHash, mTxWithMetas.at(*iter).second->slice());
    }

    mHashSet.TxsRootHash = static_cast<typename sha512_half_hasher::result_type>(txRootHash);
    mHashSet.TxWMRootHash = static_cast<typename sha512_half_hasher::result_type>(txWMRootHash);

    sha512_half_hasher stateDeltaHash;

    for (auto iter = mStateDeltas.begin(); iter != mStateDeltas.end(); iter++)
    {
        hash_append(stateDeltaHash, (uint8)iter->second.first);
        hash_append(stateDeltaHash, iter->second.second.slice());
    }

    mHashSet.StateDeltaHash = static_cast<typename sha512_half_hasher::result_type>(stateDeltaHash);

    sha512_half_hasher ledgerHash;

    setLedgerHash(sha512Half(
        mSeq,
        mShardID,
        mHashSet.TxsRootHash,
        mHashSet.TxWMRootHash,
        mHashSet.StateDeltaHash));
}


void MicroLedger::addStateDelta(ReadView const& base, uint256 key, Action action, std::shared_ptr<SLE> sle)
{
    switch (sle->getType())
    {
    case ltACCOUNT_ROOT:
        if (action == detail::RawStateTable::Action::replace)
        {
            // For Account, only Balance need merge.
            auto balance = sle->getFieldAmount(sfBalance);
            auto oriSle = base.read(Keylet(sle->getType(), key));
            assert(oriSle);
            auto priorBlance = oriSle->getFieldAmount(sfBalance);
            auto deltaBalance = balance - priorBlance;
            sle->setFieldAmount(sfBalance, deltaBalance);
        }
        mStateDeltas.emplace(key, std::make_pair(action, std::move(sle->getSerializer())));
        break;
    case ltTABLELIST:
        mStateDeltas.emplace(key, std::make_pair(action, std::move(sle->getSerializer())));
        break;
    case ltDIR_NODE:
        mStateDeltas.emplace(key, std::make_pair(action, std::move(sle->getSerializer())));
        break;
    default:
        break;
    }
}

void MicroLedger::compose(protocol::TMMicroLedgerSubmit& ms, bool withTxMeta)
{
    protocol::MicroLedger& m = *(ms.mutable_microledger());

    m.set_ledgerseq(mSeq);
    m.set_shardid(mShardID);

    // Transaction hashes.
    for (auto it : mTxsHashes)
    {
        m.add_txhashes(it.data(), it.size());
    }

    // Transaction with meta data root hash.
    m.set_txwmhashroot(mHashSet.TxWMRootHash.data(),
        mHashSet.TxWMRootHash.size());

    // Statedetals.
    for (auto it : mStateDeltas)
    {
        protocol::StateDelta& s = *m.add_statedeltas();
        s.set_action((uint32)it.second.first);                        // action
        s.set_sle(it.second.second.data(), it.second.second.size());  // sle
    }

    // Signatures
    for (auto it : Signatures())
    {
        protocol::Signature& s = *ms.add_signatures();
        s.set_publickey((const char *)it.first.data(), it.first.size());
        s.set_signature(it.second.data(), it.second.size());
    }

    if (withTxMeta)
    {
        // add tx with meta and send to lookups.
        for (auto it : mTxWithMetas)
        {
            protocol::TxWithMeta& tx = *m.add_txwithmetas();
            tx.set_hash(it.first.data(), it.first.size());
            tx.set_body(it.second.first->data(), it.second.first->size());
            tx.set_meta(it.second.second->data(), it.second.second->size());
        }
    }
}

MicroLedger::MicroLedger(protocol::TMMicroLedgerSubmit const& m)
{
	readMicroLedger(m.microledger());
	readSignature(m.signatures());
}

MicroLedger::MicroLedger(protocol::TMMicroLedgerWithTxsSubmit const& m)
{
	readMicroLedger(m.microledger());
	readSignature(m.signatures());
}

void MicroLedger::readMicroLedger(protocol::MicroLedger const& m)
{
	mSeq = m.ledgerseq();
	mShardID = m.shardid();
	readTxHashes(m.txhashes());
	readStateDelta(m.statedeltas());
	memcpy(mTxWMRootHash.begin(), m.txwmhashroot().data(), 32);
}
void MicroLedger::readTxHashes(::google::protobuf::RepeatedPtrField< ::std::string> const& hashes)
{
	assert(hashes.size() % 256 == 0);
	for (int i = 0; i < hashes.size(); i++)
	{
		TxID txHash;
		memcpy(txHash.begin(), hashes.Get(i).data(), 32);
		mTxsHashes.push_back(txHash);
	}
}

void MicroLedger::readStateDelta(::google::protobuf::RepeatedPtrField< ::std::string> const& stateDeltas)
{
	for (int i = 0; i < stateDeltas.size(); i++)
	{
		Blob delta;
		delta.assign(stateDeltas.Get(i).begin(), stateDeltas.Get(i).end());
		mStateDeltas.push_back(delta);
	}
}
LedgerIndex	MicroLedger::seq()
{
	return mSeq;
}
uint32	MicroLedger::shardID()
{
	return mShardID;
}
std::vector<TxID> const& MicroLedger::txHashes()
{
	return mTxsHashes;
}
std::vector<Blob> const& MicroLedger::stateDeltas()
{
	return mStateDeltas;
}
uint256	MicroLedger::txRootHash()
{
	return mTxWMRootHash;
}

bool MicroLedger::checkValidity(ValidatorList const& list, Blob signingData)
{
	bool ret = LedgerBase::checkValidity(list, signingData);
	if (!ret)
		return ret;

	//check tx-roothash

}

}
