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
#include <ripple/ledger/TxMeta.h>
#include <peersafe/rpc/TableUtils.h>


namespace ripple {

MicroLedger::MicroLedger(uint32 shardID_, LedgerIndex seq_, OpenView &view)
    : mSeq(seq_)
    , mShardID(shardID_)
{
    assert(!view.open());

    view.apply(*this);

    computeHash(true);
}

MicroLedger::MicroLedger(protocol::TMMicroLedgerSubmit const& m, bool withTxMeta)
{
    readMicroLedger(m.microledger());
    readSignature(m.signatures());

    // The Message probably not have txWithMeta source data.
    // But it contains txWithMeta root hash. So, don't compute
    // txWithMeta root hash here, and check it late if it takes
    // the source data.
    computeHash(withTxMeta);
}

void MicroLedger::computeHash(bool withTxMeta)
{
    using beast::hash_append;

    if (mTxsHashes.size() > 0)
    {
        sha512_half_hasher txRootHash;
        for (auto txHash : mTxsHashes)
        {
            hash_append(txRootHash, txHash);
        }
        mHashSet.TxsRootHash = static_cast<typename sha512_half_hasher::result_type>(txRootHash);
    }
    else
    {
        mHashSet.TxsRootHash = zero;
    }

    if (mStateDeltas.size() > 0)
    {
        sha512_half_hasher stateDeltaHash;
        for (auto stateDelta : mStateDeltas)
        {
            hash_append(stateDeltaHash, stateDelta.first);
            hash_append(stateDeltaHash, (uint8)stateDelta.second.first);
            hash_append(stateDeltaHash, stateDelta.second.second.slice());
        }
        mHashSet.StateDeltaHash = static_cast<typename sha512_half_hasher::result_type>(stateDeltaHash);
    }
    else
    {
        mHashSet.StateDeltaHash = zero;
    }

    if (withTxMeta)
    {
        mHashSet.TxWMRootHash = computeTxWithMetaHash();
    }

    setLedgerHash( sha512Half(
        mSeq,
        mShardID,
        mDropsDestroyed,
        mHashSet.TxsRootHash,
        mHashSet.TxWMRootHash,
        mHashSet.StateDeltaHash) );
}

uint256 MicroLedger::computeTxWithMetaHash()
{
    using beast::hash_append;

    if (mTxsHashes.size() <= 0)
    {
        return zero;
    }

    sha512_half_hasher txWMRootHash;

    try
    {
        for (auto txHash : mTxsHashes)
        {
            hash_append(txWMRootHash, mTxWithMetas.at(txHash).first->slice());
            hash_append(txWMRootHash, mTxWithMetas.at(txHash).second->slice());
        }
    }
    catch (std::exception const&)
    {
        return zero;
    }

    return static_cast<typename sha512_half_hasher::result_type>(txWMRootHash);
}

void MicroLedger::compose(protocol::TMMicroLedgerSubmit& ms, bool withTxMeta)
{
    protocol::MicroLedger& m = *(ms.mutable_microledger());

    m.set_ledgerseq(mSeq);
    m.set_shardid(mShardID);
    m.set_dropsdestroyed(mDropsDestroyed);

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
        s.set_key(it.first.data(), it.first.size());                  // key
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

void MicroLedger::apply(OpenView& to) const
{
    to.rawDestroyZXC(mDropsDestroyed);

    for (auto& it : mStateDeltas)
    {
        std::shared_ptr<SLE> sle = std::make_shared<SLE>(std::move(it.second.second.slice()), it.first);
        Keylet k(sle->getType(), sle->key());
        std::shared_ptr<SLE const> base = to.read(k);
        switch (sle->getType())
        {
        case ltACCOUNT_ROOT:
            switch (it.second.first)
            {
            case detail::RawStateTable::Action::insert:
            {
                auto it = to.items().items().find(k.key);
                if (it != to.items().items().end())
                {
                    if (it->second.first == detail::RawStateTable::Action::insert)
                    {
                        auto preSle = it->second.second;
                        auto priorBlance = preSle->getFieldAmount(sfBalance);
                        auto deltaBalance = sle->getFieldAmount(sfBalance);
                        preSle->setFieldAmount(sfBalance, priorBlance + deltaBalance);
                    }
                    else
                    {
                        //LogicError("RawStateTable::");
                    }
                }
                else
                {
                    to.rawInsert(sle);
                }
                break;
            }
            case detail::RawStateTable::Action::erase:
            {
                // Account root erase only occur with Contract, don't support it.
                auto it = to.items().items().find(k.key);
                if (it != to.items().items().end())
                {
                    //LogicError("RawStateTable::");
                }
                else
                {
                    to.rawInsert(sle);
                }
                break;
            }
            case detail::RawStateTable::Action::replace:
            {
                auto it = to.items().items().find(k.key);
                if (it != to.items().items().end())
                {
                    if (it->second.first == detail::RawStateTable::Action::replace)
                    {
                        auto preSle = it->second.second;
                        auto priorBlance = preSle->getFieldAmount(sfBalance);
                        auto deltaBalance = sle->getFieldAmount(sfBalance);
                        preSle->setFieldAmount(sfBalance, priorBlance + deltaBalance);
                    }
                    else
                    {
                        //LogicError("RawStateTable::");
                    }
                }
                else
                {
                    auto priorBlance = base->getFieldAmount(sfBalance);
                    auto deltaBalance = sle->getFieldAmount(sfBalance);
                    sle->setFieldAmount(sfBalance, priorBlance + deltaBalance);
                    to.rawReplace(sle);
                }
                break;
            }
            default:
                break;
            }
            break;
        case ltTABLELIST:
            switch (it.second.first)
            {
            case detail::RawStateTable::Action::insert:
            case detail::RawStateTable::Action::erase:
            {
                auto it = to.items().items().find(k.key);
                if (it != to.items().items().end())
                {
                    //LogicError("RawStateTable::");
                }
                else
                {
                    to.rawInsert(sle);
                }
                break;
            }
            case detail::RawStateTable::Action::replace:
            {
                auto it = to.items().items().find(k.key);
                if (it != to.items().items().end())
                {
                    if (it->second.first == detail::RawStateTable::Action::replace)
                    {
                        auto tableEntries = sle->getFieldArray(sfTableEntries);
                        auto baseTableEntries = base->getFieldArray(sfTableEntries);
                        // tableEntry.users modified or owner add/delete a table
                        if ( [&]() -> bool {
                                 if (tableEntries.size() != baseTableEntries.size())
                                 {
                                     return true;
                                 }
                                 for (auto const& table : tableEntries)
                                 {
                                     auto baseTable = getTableEntry(baseTableEntries, table.getFieldVL(sfTableName));
                                     if (!baseTable || baseTable->getFieldArray(sfUsers) != table.getFieldArray(sfUsers))
                                     {
                                         return true;
                                     }
                                 }
                                 return false;
                             }() )
                        {
                            to.rawReplace(sle);
                        }
                    }
                    else
                    {
                        //? LogicError("RawStateTable::");
                    }
                }
                else
                {
                    to.rawReplace(sle);
                }
                break;
            }
            default:
                break;
            }
            break;
        case ltDIR_NODE:
            switch (it.second.first)
            {
            case detail::RawStateTable::Action::insert:
            case detail::RawStateTable::Action::erase:
            case detail::RawStateTable::Action::replace:        // replace need any other handled?
            {
                auto it = to.items().items().find(k.key);
                if (it != to.items().items().end())
                {
                    //LogicError("RawStateTable::");
                }
                else
                {
                    to.rawInsert(sle);
                }
                break;
            }
            }
            break;
        default:
            break;
        }
    }

    for (auto const& it : mTxsHashes)
    {
        to.rawTxInsert(it,
            std::make_shared<Serializer>(0),    // tx
            std::make_shared<Serializer>(0));   // meta
    }
}

void MicroLedger::readMicroLedger(protocol::MicroLedger const& m)
{
	mSeq = m.ledgerseq();
	mShardID = m.shardid();
    mDropsDestroyed = m.dropsdestroyed();
	memcpy(mHashSet.TxWMRootHash.begin(), m.txwmhashroot().data(), 32);
	readTxHashes(m.txhashes());
	readStateDelta(m.statedeltas());
	readTxWithMeta(m.txwithmetas());
}
void MicroLedger::readTxHashes(::google::protobuf::RepeatedPtrField< ::std::string> const& hashes)
{
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

void MicroLedger::setMetaIndex(TxID const& hash, uint32 index, beast::Journal j)
{
	if (mTxWithMetas.find(hash) == mTxWithMetas.end())
		return;
	auto txMetaPair = mTxWithMetas[hash];
	auto txMeta = std::make_shared<TxMeta>(hash, mSeq, txMetaPair.second->getData(), j);
	// re-serialize
	auto sMeta = std::make_shared<Serializer>();
	txMeta->addRaw(*sMeta, txMeta->getResultTER(), index);
	mTxWithMetas[hash].second = sMeta;
}

bool MicroLedger::checkValidity(std::unique_ptr <ValidatorList> const& list, bool withTxMeta)
{
    // We have compute the ledger digest(hash) by ownself,
    // only exclude the txWithMeta hash. Check it, if has TxWithMeta data.
    if (withTxMeta)
    {
        if (mTxsHashes.size() != mTxWithMetas.size())
        {
            return false;
        }

        // Check all tx-meta corresponds to tx-hashes
        for (TxID hash : mTxsHashes)
        {
            if (!hasTxWithMeta(hash))
                return false;
        }

        if (computeTxWithMetaHash() != mHashSet.TxWMRootHash)
        {
            return false;
        }
    }

    return LedgerBase::checkValidity(list);
}


}
