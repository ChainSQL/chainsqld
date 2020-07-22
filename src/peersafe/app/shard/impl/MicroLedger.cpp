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
#include <peersafe/app/util/Common.h>
#include <peersafe/app/shard/ShardManager.h>


namespace ripple {

MicroLedger::MicroLedger(uint64 viewChange, uint32 shardID_, LedgerIndex seq_, OpenView &view)
    : mSeq(seq_)
    , mViewChange(viewChange)
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
        for (auto const& txHash : mTxsHashes)
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
        for (auto const& stateDelta : mStateDeltas)
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
        mViewChange,
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
        for (auto const& txHash : mTxsHashes)
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

    m.set_viewchange(mViewChange);
    m.set_ledgerseq(mSeq);
    m.set_shardid(mShardID);
    m.set_dropsdestroyed(mDropsDestroyed);

    // Transaction hashes.
    for (auto const& it : mTxsHashes)
    {
        m.add_txhashes(it.data(), it.size());
    }

    // Transaction with meta data root hash.
    if (mHashSet.TxWMRootHash != zero)
    {
        m.set_txwmhashroot(mHashSet.TxWMRootHash.data(),
            mHashSet.TxWMRootHash.size());
    }

    // Statedetals.
    for (auto const& it : mStateDeltas)
    {
        protocol::StateDelta& s = *m.add_statedeltas();
        s.set_key(it.first.data(), it.first.size());                  // key
        s.set_action((uint32)it.second.first);                        // action
        s.set_sle(it.second.second.data(), it.second.second.size());  // sle
    }

    // Signatures
    for (auto const& it : signatures())
    {
        protocol::Signature& s = *ms.add_signatures();
        s.set_publickey(it.first.data(), it.first.size());
        s.set_signature(it.second.data(), it.second.size());
    }

    if (withTxMeta)
    {
        // add tx with meta and send to lookups.
        for (auto const& it : mTxWithMetas)
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
            auto const& oriSle = base.read(Keylet{ sle->getType(), key });
            assert(oriSle);

            auto& balance = sle->getFieldAmount(sfBalance);
            auto& priorBalance = oriSle->getFieldAmount(sfBalance);
            sle->setFieldAmount(sfBalance, balance - priorBalance);

            std::uint32_t ownerCount = sle->getFieldU32(sfOwnerCount);
            std::uint32_t priorOwnerCount = oriSle->getFieldU32(sfOwnerCount);
            sle->setFieldU32(sfOwnerCount, ownerCount - priorOwnerCount);
        }
        mStateDeltas.emplace(key, std::make_pair(action, std::move(sle->getSerializer())));
        break;
    case ltTABLELIST:
        mStateDeltas.emplace(key, std::make_pair(action, std::move(sle->getSerializer())));
        break;
    case ltDIR_NODE:
        mStateDeltas.emplace(key, std::make_pair(action, std::move(sle->getSerializer())));
        break;
    case ltCHAINID:
        mStateDeltas.emplace(key, std::make_pair(action, std::move(sle->getSerializer())));
        break;
    default:
        break;
    }
}

bool MicroLedger::sameShard(std::shared_ptr<SLE>& sle, Application& app) const
{
    std::uint32_t shardCount = app.getShardManager().shardCount();

    if (shardCount == 0) return false;

    std::uint32_t x = 0;

    std::string sAccountID = toBase58(sle->getAccountID(sfAccount));
    std::uint32_t len = sAccountID.size();
    assert(len >= 4);

    // Take the last four bytes of the address
    for (std::uint32_t i = 0; i < 4; i++)
    {
        x = (x << 8) | sAccountID[len - 4 + i];
    }

    return (x % shardCount + 1) == mShardID;
}

void MicroLedger::applyAccountRoot(
    OpenView& to,
    detail::RawStateTable::Action action,
    std::shared_ptr<SLE>& sle,
    beast::Journal& j,
    Application& app) const
{
    //auto st = utcTimeUs();

    switch (action)
    {
    case detail::RawStateTable::Action::insert:
    {
        auto const& iter = to.items().items().find(sle->key());
        if (iter != to.items().items().end())
        {
            auto& item = iter->second;
            if (item.first == detail::RawStateTable::Action::insert)
            {
                auto& preSle = item.second;

                auto& priorBlance = preSle->getFieldAmount(sfBalance);
                auto& deltaBalance = sle->getFieldAmount(sfBalance);
                preSle->setFieldAmount(sfBalance, priorBlance + deltaBalance);

                std::int32_t deltaOwnerCount = (std::int32_t)sle->getFieldU32(sfOwnerCount);
                if (deltaOwnerCount)
                {
                    std::uint32_t const adjusted = confineOwnerCount(
                        preSle->getFieldU32(sfOwnerCount),
                        deltaOwnerCount,
                        preSle->getAccountID(sfAccount),
                        j);
                    preSle->setFieldU32(sfOwnerCount, adjusted);
                }

                preSle->setFieldH256(sfPreviousTxnID, sle->getFieldH256(sfPreviousTxnID));
            }
            else
            {
                LogicError("RawStateTable::ltACCOUNT_ROOT insert action conflict with other action");
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
        // Account root erase only occur with Contract.
        if (sle->isFieldPresent(sfContractCode))
        {
            // TODO
            //if (to.items().items().count(sle->key()))
            //{
            //    //LogicError("RawStateTable::");
            //}
            //else
            //{
            //    to.rawErase(sle);
            //}
        }
        else
        {
            LogicError("RawStateTable::ltACCOUNT_ROOT erase action occurred with non-contract account");
        }
        break;
    }
    case detail::RawStateTable::Action::replace:
    {
        auto const& iter = to.items().items().find(sle->key());
        if (iter != to.items().items().end())
        {
            auto& item = iter->second;
            if (item.first == detail::RawStateTable::Action::replace)
            {
                auto& preSle = item.second;

                auto& priorBlance = preSle->getFieldAmount(sfBalance);
                auto& deltaBalance = sle->getFieldAmount(sfBalance);
                preSle->setFieldAmount(sfBalance, priorBlance + deltaBalance);

                std::int32_t deltaOwnerCount = (std::int32_t)sle->getFieldU32(sfOwnerCount);
                if (deltaOwnerCount)
                {
                    std::uint32_t const adjusted = confineOwnerCount(
                        preSle->getFieldU32(sfOwnerCount),
                        deltaOwnerCount,
                        preSle->getAccountID(sfAccount),
                        j);
                    preSle->setFieldU32(sfOwnerCount, adjusted);
                }

                preSle->setFieldH256(sfPreviousTxnID, sle->getFieldH256(sfPreviousTxnID));

                if (sameShard(sle, app))
                {
                    preSle->setFieldU32(sfSequence, sle->getFieldU32(sfSequence));
                    preSle->setFieldU32(sfFlags, sle->getFieldU32(sfFlags));
                    if (sle->isFieldPresent(sfAccountTxnID))
                        preSle->setFieldH256(sfAccountTxnID, sle->getFieldH256(sfAccountTxnID));
                    if (sle->isFieldPresent(sfRegularKey))
                        preSle->setAccountID(sfRegularKey, sle->getAccountID(sfRegularKey));
                    if (sle->isFieldPresent(sfEmailHash))
                        preSle->setFieldH128(sfEmailHash, sle->getFieldH128(sfEmailHash));
                    if (sle->isFieldPresent(sfWalletLocator))
                        preSle->setFieldH256(sfWalletLocator, sle->getFieldH256(sfWalletLocator));
                    //if (sle->isFieldPresent(sfWalletSize))    // Not used
                    //    preSle->setFieldU32(sfWalletSize, sle->getFieldU32(sfWalletSize));
                    if (sle->isFieldPresent(sfMessageKey))
                        preSle->setFieldVL(sfMessageKey, sle->getFieldVL(sfWalletLocator));
                    if (sle->isFieldPresent(sfTransferRate))
                        preSle->setFieldU32(sfTransferRate, sle->getFieldU32(sfTransferRate));
                    if (sle->isFieldPresent(sfTransferFeeMin))
                        preSle->setFieldVL(sfTransferFeeMin, sle->getFieldVL(sfTransferFeeMin));
                    if (sle->isFieldPresent(sfTransferFeeMax))
                        preSle->setFieldVL(sfTransferFeeMax, sle->getFieldVL(sfTransferFeeMax));
                    if (sle->isFieldPresent(sfDomain))
                        preSle->setFieldVL(sfDomain, sle->getFieldVL(sfDomain));
                    if (sle->isFieldPresent(sfMemos))
                        preSle->setFieldArray(sfMemos, sle->getFieldArray(sfMemos));
                    if (sle->isFieldPresent(sfTickSize))
                        preSle->setFieldU8(sfTickSize, sle->getFieldU8(sfTickSize));
                }
            }
            else if (item.first == detail::RawStateTable::Action::erase)
            {
                auto& preSle = item.second;
                JLOG(j.warn()) << "RawStateTable::ltACCOUNT_ROOT account "
                    << toBase58(preSle->getAccountID(sfAccount))
                    << " deleted on other shard";
            }
            else
            {
                LogicError("RawStateTable::ltACCOUNT_ROOT replace action conflact with insert action");
            }
        }
        else
        {
            auto const& base = to.read(Keylet{sle->getType(), sle->key()});

            auto& priorBlance = base->getFieldAmount(sfBalance);
            auto& deltaBalance = sle->getFieldAmount(sfBalance);
            sle->setFieldAmount(sfBalance, priorBlance + deltaBalance);

            std::int32_t deltaOwnerCount = (std::int32_t)sle->getFieldU32(sfOwnerCount);
            if (deltaOwnerCount)
            {
                std::uint32_t const adjusted = confineOwnerCount(
                    base->getFieldU32(sfOwnerCount),
                    deltaOwnerCount,
                    base->getAccountID(sfAccount),
                    j);
                sle->setFieldU32(sfOwnerCount, adjusted);
            }
            to.rawReplace(sle);
        }
        break;
    }
    default:
        break;
    }

    //JLOG(j.info()) << "apply account root time used : " << utcTimeUs() - st << "us";
}

void MicroLedger::applyTableList(
    OpenView& to,
    detail::RawStateTable::Action action,
    std::shared_ptr<SLE>& sle,
    beast::Journal& j) const
{
    //auto st = utcTimeUs();

    switch (action)
    {
    case detail::RawStateTable::Action::insert:
    case detail::RawStateTable::Action::erase:
    {
        if (to.items().items().count(sle->key()))
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
        auto const& iter = to.items().items().find(sle->key());
        if (iter != to.items().items().end())
        {
            auto& item = iter->second;
            if (item.first == detail::RawStateTable::Action::replace)
            {
                auto const& base = to.read(Keylet{sle->getType(), sle->key()});
                auto& tableEntries = sle->getFieldArray(sfTableEntries);
                auto& baseTableEntries = base->getFieldArray(sfTableEntries);
                // tableEntry.users modified or owner add/delete a table
                if ([&]() -> bool {
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
                }())
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

    //JLOG(j.info()) << "apply table list time used : " << utcTimeUs() - st << "us";
}

void MicroLedger::applyCommons(
    OpenView& to,
    detail::RawStateTable::Action action,
    std::shared_ptr<SLE>& sle,
    beast::Journal& j) const
{
    //auto st = utcTimeUs();

    switch (action)
    {
    case detail::RawStateTable::Action::insert:
    case detail::RawStateTable::Action::erase:
    case detail::RawStateTable::Action::replace:        // replace need any other handled?
        if (to.items().items().count(sle->key()))
        {
            //LogicError("RawStateTable::");
        }
        else
        {
            to.rawInsert(sle);
        }
        break;
    }

    //JLOG(j.info()) << "apply table list time used : " << utcTimeUs() - st << "us";
}

void MicroLedger::apply(OpenView& to, beast::Journal& j, Application& app) const
{
    to.rawDestroyZXC(mDropsDestroyed);

    auto st = utcTime();

    for (auto const& stateDelta : mStateDeltas)
    {
        std::shared_ptr<SLE> sle = std::make_shared<SLE>(stateDelta.second.second.slice(), stateDelta.first);
        switch (sle->getType())
        {
        case ltACCOUNT_ROOT:
            applyAccountRoot(to, stateDelta.second.first, sle, j, app);
            break;
        case ltTABLELIST:
            applyTableList(to, stateDelta.second.first, sle, j);
            break;
        case ltDIR_NODE:
        case ltCHAINID:
            applyCommons(to, stateDelta.second.first, sle, j);
            break;
        default:
            break;
        }
    }

    JLOG(j.info()) << "apply " << mStateDeltas.size() << " stateDeltas time used: " << utcTime() - st << "ms";

    st = utcTime();

    for (auto const& it : mTxsHashes)
    {
        to.rawTxInsert(it,
            std::make_shared<Serializer>(0),    // tx
            std::make_shared<Serializer>(0));   // meta
    }

    JLOG(j.info()) << "apply " << mTxsHashes.size() << " txs time used: " << utcTime() - st << "ms";
}

void MicroLedger::apply(Ledger& to) const
{
    for (auto const& it : mTxWithMetas)
    {
        to.rawTxInsert(it.first, it.second.first, it.second.second);
    }
}

void MicroLedger::readMicroLedger(protocol::MicroLedger const& m)
{
    mViewChange = m.viewchange();
	mSeq = m.ledgerseq();
	mShardID = m.shardid();
    mDropsDestroyed = m.dropsdestroyed();
    if (m.has_txwmhashroot())
    {
        memcpy(mHashSet.TxWMRootHash.begin(), m.txwmhashroot().data(), 32);
    }
    else
    {
        mHashSet.TxWMRootHash = zero;
    }
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

void MicroLedger::setMetaIndex(TxID const& hash, uint32 index, beast::Journal& j)
{
    assert(mTxWithMetas.find(hash) != mTxWithMetas.end());
	auto& txMetaPair = mTxWithMetas[hash];
	auto txMeta = std::make_shared<TxMeta>(hash, mSeq, txMetaPair.second->getData(), j);
	// re-serialize
	auto sMeta = std::make_shared<Serializer>();
	txMeta->addRaw(*sMeta, txMeta->getResultTER(), index);
    txMetaPair.second = sMeta;
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
        for (TxID const& hash : mTxsHashes)
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
