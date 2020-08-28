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
#include <ripple/app/misc/AmendmentTable.h>
#include <peersafe/rpc/TableUtils.h>
#include <peersafe/app/util/Common.h>
#include <peersafe/app/shard/ShardManager.h>


namespace ripple {

MicroLedger::MicroLedger(uint64 viewChange, uint32 shardID_, uint32 shardCount, LedgerIndex seq_, OpenView const& view, std::shared_ptr<CanonicalTXSet const> txSet)
    : mSeq(seq_)
    , mViewChange(viewChange)
    , mShardID(shardID_)
    , mShardCount(shardCount)
{
    assert(!view.open());

    view.apply(*this, txSet);

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
        mShardCount,
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
    m.set_shardcount(mShardCount);
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
        break;
    case ltRIPPLE_STATE:
        if (action == detail::RawStateTable::Action::replace)
        {
            auto const& oriSle = base.read(Keylet{ sle->getType(), key });
            assert(oriSle);

            auto& balance = sle->getFieldAmount(sfBalance);
            auto& priorBalance = oriSle->getFieldAmount(sfBalance);
            sle->setFieldAmount(sfBalance, balance - priorBalance);
        }
        break;
    case ltDIR_NODE:
        if (action == detail::RawStateTable::Action::insert)
        {
            STVector256 const& svIndexes = sle->getFieldV256(sfIndexes);
            if (svIndexes.size())
            {
                sle->setFlag(lsfAddIndex);
            }
            else
            {
                return;
            }
        }
        else if (action == detail::RawStateTable::Action::erase)
        {
            STVector256 svIndexes = sle->getFieldV256(sfIndexes);
            assert(svIndexes.size() == 0);
            if (svIndexes.size() == 0)
            {
                auto const& oriSle = base.read(Keylet{ sle->getType(), key });
                auto& priorIndexes = oriSle->getFieldV256(sfIndexes);
                for (auto const& key : priorIndexes)
                {
                    svIndexes.push_back(key);
                }
                if (svIndexes.size())
                {
                    sle->setFieldV256(sfIndexes, svIndexes);
                    sle->setFlag(lsfDeleteIndex);
                }
                else
                {
                    return;
                }
            }
            else
            {
                LogicError("RawStateTable::ltDIR_NODE earsed, but it contains any indexes");
                return;
            }
        }
        else
        {
            auto const& oriSle = base.read(Keylet{ sle->getType(), key });

            STVector256 svIndexes = sle->getFieldV256(sfIndexes);
            STVector256 priorIndexes = oriSle->getFieldV256(sfIndexes);

            // Only IndexNext or IndexPrevious changed.
            if (svIndexes.isEquivalent(priorIndexes)) return;

            std::vector<uint256> symDifference;
            std::sort(svIndexes.begin(), svIndexes.end());
            std::sort(priorIndexes.begin(), priorIndexes.end());
            std::set_symmetric_difference(
                svIndexes.begin(), svIndexes.end(),
                priorIndexes.begin(), priorIndexes.end(),
                std::back_inserter(symDifference));
            assert(symDifference.size());
            svIndexes = std::move(symDifference);
            sle->setFieldV256(sfIndexes, svIndexes);
            sle->setFlag(lsfAddIndex | lsfDeleteIndex);
        }
        break;
    case ltESCROW:
    case ltTABLELIST:
    case ltCHAINID:
    case ltFEE_SETTINGS:
    case ltAMENDMENTS:
        break;
    default:
        // Don't case other type of sle.
        return;
    }

    mStateDeltas.emplace(key, std::make_pair(action, std::move(sle->getSerializer())));
}

bool MicroLedger::sameShard(std::shared_ptr<SLE>& sle, Application& app) const
{
    AccountID sAccountID;
    switch (sle->getType())
    {
    case ltACCOUNT_ROOT:
    case ltESCROW:
    {
        sAccountID = sle->getAccountID(sfAccount);
        break;
    }
    case ltRIPPLE_STATE:
    {
        STAmount limit = sle->getFieldAmount(sfLowLimit);
        if (limit > zero)
        {
            sAccountID = limit.getIssuer();
        }
        else
        {
            sAccountID = sle->getFieldAmount(sfHighLimit).getIssuer();
        }
        break;
    }
    case ltTABLELIST:
    {
        sAccountID = sle->getAccountID(sfOwner);
        auto &aTableEntries = sle->peekFieldArray(sfTableEntries);
        if (aTableEntries.size() == 0)
        {
            // Owner drops all tables.
            return true;
        }
        auto& users = aTableEntries[0].peekFieldArray(sfUsers);
        sAccountID = users[0].getAccountID(sfUser);
        break;
    }
    default:
        return false;
    }

    return app.getShardManager().lookup().getShardIndex(sAccountID) == mShardID;
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
            if (to.items().items().count(sle->key()))
            {
                LogicError("RawStateTable::ltACCOUNT_ROOT Contract account erase action conflict with other action");
            }
            else
            {
                to.rawErase(sle);
            }
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
            //else if (item.first == detail::RawStateTable::Action::erase)
            //{
            //    auto& preSle = item.second;
            //    JLOG(j.warn()) << "RawStateTable::ltACCOUNT_ROOT account "
            //        << toBase58(preSle->getAccountID(sfAccount))
            //        << " deleted on other shard";
            //}
            else
            {
                LogicError("RawStateTable::ltACCOUNT_ROOT replace action conflict with insert action");
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

void MicroLedger::applyRippleState(
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
            LogicError("RawStateTable::ltRIPPLE_STATE insert action conflict with other action");
        }
        else
        {
            to.rawInsert(sle);
        }
        break;
    }
    case detail::RawStateTable::Action::erase:
    {
        auto const& iter = to.items().items().find(sle->key());
        if (iter != to.items().items().end())
        {
            auto& item = iter->second;
            if (item.first == detail::RawStateTable::Action::replace)
            {
                to.rawErase(sle);
            }
            else
            {
                LogicError("RawStateTable::ltRIPPLE_STATE erase action conflict with other action");
            }
        }
        else
        {
            to.rawErase(sle);
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

                preSle->setFieldH256(sfPreviousTxnID, sle->getFieldH256(sfPreviousTxnID));

                if (sameShard(sle, app))
                {
                    preSle->setFieldU32(sfFlags, sle->getFieldU32(sfFlags));
                    preSle->setFieldAmount(sfLowLimit, sle->getFieldAmount(sfLowLimit));
                    preSle->setFieldAmount(sfHighLimit, sle->getFieldAmount(sfHighLimit));
                    if (sle->isFieldPresent(sfLowNode))
                        preSle->setFieldU64(sfLowNode, sle->getFieldU64(sfLowNode));
                    if (sle->isFieldPresent(sfLowQualityIn))
                        preSle->setFieldU32(sfLowQualityIn, sle->getFieldU32(sfLowQualityIn));
                    if (sle->isFieldPresent(sfLowQualityOut))
                        preSle->setFieldU32(sfLowQualityOut, sle->getFieldU32(sfLowQualityOut));
                    if (sle->isFieldPresent(sfHighNode))
                        preSle->setFieldU64(sfHighNode, sle->getFieldU64(sfHighNode));
                    if (sle->isFieldPresent(sfHighQualityIn))
                        preSle->setFieldU32(sfHighQualityIn, sle->getFieldU32(sfHighQualityIn));
                    if (sle->isFieldPresent(sfHighQualityOut))
                        preSle->setFieldU32(sfHighQualityOut, sle->getFieldU32(sfHighQualityOut));
                    if (sle->isFieldPresent(sfMemos))
                        preSle->setFieldArray(sfMemos, sle->getFieldArray(sfMemos));
                }
            }
            else if (item.first == detail::RawStateTable::Action::erase)
            {
                JLOG(j.warn()) << "RawStateTable::ltRIPPLE_STATE "
                    << " deleted on other shard";
            }
            else
            {
                LogicError("RawStateTable::ltRIPPLE_STATE replace action conflict with insert action");
            }
        }
        else
        {
            auto const& base = to.read(Keylet{ sle->getType(), sle->key() });

            auto& priorBlance = base->getFieldAmount(sfBalance);
            auto& deltaBalance = sle->getFieldAmount(sfBalance);
            sle->setFieldAmount(sfBalance, priorBlance + deltaBalance);
            to.rawReplace(sle);
        }
        break;
    }
    default:
        break;
    }

    //JLOG(j.info()) << "apply ripple state time used : " << utcTimeUs() - st << "us";
}

void MicroLedger::applyDirNode(
    OpenView& to,
    detail::RawStateTable::Action action,
    std::shared_ptr<SLE>& sle,
    beast::Journal& j) const
{
    //auto st = utcTimeUs();

    switch (sle->getFlags())
    {
    case lsfAddIndex:
    {
        std::vector<uint256> addIndexes = sle->getFieldV256(sfIndexes).value();
        dirAdd(to, sle, addIndexes, j);
        break;
    }
    case lsfDeleteIndex:
    {
        std::vector<uint256> deleteIndexes = sle->getFieldV256(sfIndexes).value();
        dirDelete(to, sle, deleteIndexes, j);
        break;
    }
    case lsfAddIndex | lsfDeleteIndex:
    {
        auto const& base = to.base().read(Keylet{ sle->getType(), sle->key() });
        assert(base);
        if (!base) LogicError("RawStateTable::ltDIR_NODE replace base sle is not found");
        auto const& priorIndexes = base->getFieldV256(sfIndexes);
        std::vector<uint256> addIndexes, deleteIndexes;
        std::vector<uint256> const& samDifference = sle->getFieldV256(sfIndexes).value();
        for (auto const& key : samDifference)
        {
            auto it = std::find(priorIndexes.begin(), priorIndexes.end(), key);
            if (it == priorIndexes.end())
            {
                addIndexes.push_back(key);
            }
            else
            {
                deleteIndexes.push_back(key);
            }
        }
        if (addIndexes.size())
        {
            dirAdd(to, sle, addIndexes, j);
        }
        if (deleteIndexes.size())
        {
            dirDelete(to, sle, deleteIndexes, j);
        }
        break;
    }
    default:
        break;
    }

    //JLOG(j.info()) << "apply dir node time used : " << utcTimeUs() - st << "us";
}

void MicroLedger::applyEscrow(
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
        if (!to.items().items().count(sle->key()))
        {
            assert(sameShard(sle, app));
            to.rawInsert(sle);
        }
        else
        {
            LogicError("RawStateTable::ltESCROW insert action conflict with other action");
        }
        break;
    }
    case detail::RawStateTable::Action::erase:
    {
        auto const& iter = to.items().items().find(sle->key());
        if (iter == to.items().items().end())
        {
            to.rawErase(sle);
        }
        else
        {
            auto& item = iter->second;
            if (item.first != detail::RawStateTable::Action::erase)
            {
                LogicError("RawStateTable::ltESCROW erase action conflict with other action");
            }
        }
        break;
    }
    case detail::RawStateTable::Action::replace:
    {
        LogicError("RawStateTable::ltESCROW could never occur replace action");
        break;
    }
    default:
        break;
    }

    //JLOG(j.info()) << "apply escrow used : " << utcTimeUs() - st << "us";
}

void MicroLedger::applyTableList(
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
        if (to.items().items().count(sle->key()))
        {
            LogicError("RawStateTable::ltTABLELIST insert action conflict with other action");
        }
        else
        {
            to.rawInsert(sle);
        }
        break;
    }
    case detail::RawStateTable::Action::erase:
    {
        LogicError("RawStateTable::ltTABLELIST erase action can't happend");
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
                auto &preSle = item.second;

                bool sameShard = this->sameShard(sle, app);
                auto& priorTableEntries = preSle->peekFieldArray(sfTableEntries);
                auto const& curTableEntries = sle->getFieldArray(sfTableEntries);

                for (auto priorTable = priorTableEntries.begin(); priorTable != priorTableEntries.end(); )
                {
                    auto curTable = getTableEntry(curTableEntries, priorTable->getFieldH160(sfNameInDB));
                    if (curTable)
                    {
                        std::uint32_t priorTxnLrgSeq = priorTable->getFieldU32(sfTxnLgrSeq);
                        std::uint32_t curTxnLrgSeq = curTable->getFieldU32(sfTxnLgrSeq);
                        if (curTxnLrgSeq >= priorTxnLrgSeq)
                        {
                            if (curTxnLrgSeq > priorTxnLrgSeq)
                            {
                                priorTable->setFieldU32(sfTxnLgrSeq, curTxnLrgSeq);
                                priorTable->setFieldH256(sfTxnLedgerHash, curTable->getFieldH256(sfTxnLedgerHash));
                                priorTable->setFieldU32(sfPreviousTxnLgrSeq, curTable->getFieldU32(sfPreviousTxnLgrSeq));
                                priorTable->setFieldH256(sfPrevTxnLedgerHash, curTable->getFieldH256(sfPrevTxnLedgerHash));
                            }
                            priorTable->setFieldH256(sfTxCheckHash, curTable->getFieldH256(sfTxCheckHash));

                            if (sameShard)
                            {
                                // Table owner did rename, recreate or grant table action.
                                Blob const& curTableName = curTable->getFieldVL(sfTableName);
                                std::uint32_t curCreateLgrSeq = curTable->getFieldU32(sfCreateLgrSeq);
                                STArray const& curUsers = curTable->getFieldArray(sfUsers);
                                if (priorTable->getFieldVL(sfTableName) != curTableName)
                                {
                                    priorTable->setFieldVL(sfTableName, curTableName);
                                }
                                if (curCreateLgrSeq > priorTable->getFieldU32(sfCreateLgrSeq))
                                {
                                    priorTable->setFieldU32(sfCreateLgrSeq, curCreateLgrSeq);
                                    priorTable->setFieldH256(sfCreatedLedgerHash, curTable->getFieldH256(sfCreatedLedgerHash));
                                    priorTable->setFieldH256(sfCreatedTxnHash, curTable->getFieldH256(sfCreatedTxnHash));
                                }
                                if (priorTable->getFieldArray(sfUsers) != curUsers)
                                {
                                    priorTable->setFieldArray(sfUsers, curUsers);
                                }
                            }
                        }
                    }
                    else if (sameShard)
                    {
                        // Table owner deleted table.
                        priorTable = priorTableEntries.erase(priorTable);
                        continue;
                    }
                    else
                    {
                        // MicroLedger which contains preSle is same shard with table owner Account.
                        // And this table entry is created on the MicroLedger.
                    }
                    priorTable++;
                }

                // Check whether the owner created tables.
                if (sameShard)
                {
                    for (auto const& curTable : curTableEntries)
                    {
                        auto priorTable = getTableEntry(priorTableEntries, curTable.getFieldH160(sfNameInDB));
                        if (!priorTable)
                        {
                            priorTableEntries.push_back(curTable);
                        }
                    }
                }

                preSle->setFieldH256(sfPreviousTxnID, sle->getFieldH256(sfPreviousTxnID));
            }
            else
            {
                LogicError("RawStateTable::ltTABLELIST replace action conflict with other action");
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

void MicroLedger::applyFeeSetting(
    OpenView& to,
    std::shared_ptr<SLE>& sle,
    beast::Journal& j) const
{
    //auto st = utcTimeUs();

    auto feeShardVoting = to.getFeeShardVoting();

    assert(feeShardVoting);
    if (feeShardVoting)
    {
        feeShardVoting->baseFeeVote.addVote(sle->getFieldU64(sfBaseFee));
        //sle->getFieldU32(sfReferenceFeeUnits);
        feeShardVoting->baseReserveVote.addVote(sle->getFieldU32(sfReserveBase));
        feeShardVoting->incReserveVote.addVote(sle->getFieldU32(sfReserveIncrement));
        feeShardVoting->dropsPerByteVote.addVote(sle->getFieldU64(sfDropsPerByte));
    }

    //JLOG(j.info()) << "apply table list time used : " << utcTimeUs() - st << "us";
}

void MicroLedger::applyAmendments(
    OpenView& to,
    std::shared_ptr<SLE>& sle,
    beast::Journal& j) const
{
    //auto st = utcTimeUs();

    auto amendmentSet = to.getAmendmentSet();

    assert(amendmentSet);
    if (amendmentSet && sle->isFieldPresent(sfMajorities))
    {
        std::set<uint256> ballot;
        const STArray &majorities = sle->getFieldArray(sfMajorities);
        for (auto const& majority : majorities)
        {
            ballot.insert(majority.getFieldH256(sfAmendment));
        }

        amendmentSet->tally(ballot);
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
    {
        if (!to.items().items().count(sle->key()))
        {
            to.rawInsert(sle);
        }
        break;
    }
    case detail::RawStateTable::Action::erase:
    {
        if (!to.items().items().count(sle->key()))
        {
            to.rawErase(sle);
        }
        break;
    }
    case detail::RawStateTable::Action::replace:
    {
        to.rawReplace(sle);
        break;
    }
    }

    //JLOG(j.info()) << "apply other commons sle time used : " << utcTimeUs() - st << "us";
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
        case ltRIPPLE_STATE:
            applyRippleState(to, stateDelta.second.first, sle, j, app);
            break;
        case ltDIR_NODE:
            applyDirNode(to, stateDelta.second.first, sle, j);
            break;
        case ltESCROW:
            applyEscrow(to, stateDelta.second.first, sle, j, app);
            break;
        case ltTABLELIST:
            applyTableList(to, stateDelta.second.first, sle, j, app);
            break;
        case ltFEE_SETTINGS:
            applyFeeSetting(to, sle, j);
            break;
        case ltAMENDMENTS:
            applyAmendments(to, sle, j);
            break;
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
    mShardCount = m.shardcount();
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
