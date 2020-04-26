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

MicroLedger::MicroLedger(protocol::TMMicroLedgerSubmit const& m)
{
    readMicroLedger(m.microledger());
    readSignature(m.signatures());

    // maybe need or not
    //computeHash();
}

void MicroLedger::computeHash()
{
    using beast::hash_append;
    sha512_half_hasher txRootHash;
    sha512_half_hasher txWMRootHash;

    for (auto iter = mTxsHashes.begin(); iter != mTxsHashes.end(); iter++)
    {
        hash_append(txRootHash, *iter);

        if (mHashSet.TxWMRootHash != zero)
        {
            hash_append(txWMRootHash, mTxWithMetas.at(*iter).first->slice());
            hash_append(txWMRootHash, mTxWithMetas.at(*iter).second->slice());
        }
    }

    mHashSet.TxsRootHash = static_cast<typename sha512_half_hasher::result_type>(txRootHash);

    if (mHashSet.TxWMRootHash != zero)
    {
        mHashSet.TxWMRootHash = static_cast<typename sha512_half_hasher::result_type>(txWMRootHash);
    }

    sha512_half_hasher stateDeltaHash;

    for (auto iter = mStateDeltas.begin(); iter != mStateDeltas.end(); iter++)
    {
        hash_append(stateDeltaHash, (uint8)iter->second.first);
        hash_append(stateDeltaHash, iter->second.second.slice());
    }

    mHashSet.StateDeltaHash = static_cast<typename sha512_half_hasher::result_type>(stateDeltaHash);

    sha512_half_hasher ledgerHash;

    setLedgerHash( sha512Half(makeSlice(getSigningData())) );
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
    for (auto it : signatures())
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

void MicroLedger::readMicroLedger(protocol::MicroLedger const& m)
{
	mSeq = m.ledgerseq();
	mShardID = m.shardid();
	memcpy(mHashSet.TxWMRootHash.begin(), m.txwmhashroot().data(), 32);
	readTxHashes(m.txhashes());
	readStateDelta(m.statedeltas());
	readTxWithMeta(m.txwithmetas());
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

void MicroLedger::readStateDelta(::google::protobuf::RepeatedPtrField<::protocol::StateDelta> const& stateDeltas)
{
	for (int i = 0; i < stateDeltas.size(); i++)
	{
        const protocol::StateDelta& delta = stateDeltas.Get(i);

        uint256 key;

        memcpy(key.begin(), delta.key().data(), 32);
        Action action = (Action)delta.action();
        Serializer s(delta.sle().data(), delta.sle().size());

		mStateDeltas.emplace(key, std::make_pair(action, std::move(s)));
	}
}

void MicroLedger::readTxWithMeta(::google::protobuf::RepeatedPtrField <::protocol::TxWithMeta> const& txWithMetas)
{
	for (int i = 0; i < txWithMetas.size(); i++)
	{
        const protocol::TxWithMeta& tm = txWithMetas.Get(i);

		TxID txHash;

		memcpy(txHash.begin(), tm.hash().data(), 32);

        std::shared_ptr<Serializer> body = std::make_shared<Serializer>(tm.body().data(), tm.body().size());
        std::shared_ptr<Serializer> meta = std::make_shared<Serializer>(tm.meta().data(), tm.meta().size());

		mTxWithMetas.emplace(txHash, std::make_pair(body, meta));
	}
}

bool MicroLedger::checkValidity(std::unique_ptr <ValidatorList> const& list, Blob signingData)
{
	bool ret = LedgerBase::checkValidity(list, signingData);
	if (!ret)
		return ret;

	//check tx-roothash

	//
	if (mTxWithMetas.size() > 0 && mTxsHashes.size() != mTxWithMetas.size())
		return false;

	//check all tx-meta corresponds to tx-hashes
	for (TxID hash : mTxsHashes)
	{
		if (mTxWithMetas.find(hash) == mTxWithMetas.end())
			return false;
	}

    return true;
}

const Blob& MicroLedger::getSigningData()
{
    Serializer s;
    s.add32(mSeq);
    s.add32(mShardID);
    s.add256(mHashSet.TxsRootHash);
    s.add256(mHashSet.TxWMRootHash);
    s.add256(mHashSet.StateDeltaHash);

    return s.peekData();
}

}
