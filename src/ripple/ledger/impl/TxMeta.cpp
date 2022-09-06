//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012, 2013 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include <ripple/basics/contract.h>
#include <ripple/ledger/TxMeta.h>
#include <ripple/basics/Log.h>
#include <ripple/basics/contract.h>
#include <ripple/json/to_string.h>
#include <ripple/ledger/TxMeta.h>
#include <ripple/protocol/STAccount.h>
#include <ripple/basics/StringUtilities.h>
#include <string>

namespace ripple {

template <class T>
TxMeta::TxMeta(
    uint256 const& txid,
    std::uint32_t ledger,
    T const& data,
    CtorHelper)
    : mTransactionID(txid), mLedger(ledger), mNodes(sfAffectedNodes, 32)
{
    SerialIter sit(makeSlice(data));

    STObject obj(sit, sfMetadata);
    try
    {
        mResult = obj.getFieldU16(sfTransactionResult);
    }
    catch (const std::exception&)
    {
        //For compatibility with older-consensus 
        // SF_U8 const sfTransactionResultOld(STI_UINT8, 3, "TransactionResult");
        mResult = obj.getFieldU8(sfTransactionResultOld);
    }
    mIndex = obj.getFieldU32 (sfTransactionIndex);
    mNodes = * dynamic_cast<STArray*> (&obj.getField (sfAffectedNodes));
    if (obj.isFieldPresent(sfContractDetailMsg))
    {
        Blob msgBlob = obj.getFieldVL(sfContractDetailMsg);
        mContractDetailMsg = std::string(msgBlob.begin(), msgBlob.end());
    }
    if (obj.isFieldPresent(sfContractLogs))
    {
        contractLogData = obj.getFieldVL(sfContractLogs);
    }
    if (obj.isFieldPresent(sfContractTxs))
    {
        contractTxsData = obj.getFieldVL(sfContractTxs);
    }

    if (obj.isFieldPresent(sfDeliveredAmount))
        setDeliveredAmount(obj.getFieldAmount(sfDeliveredAmount));
}

TxMeta::TxMeta(uint256 const& txid, std::uint32_t ledger, STObject const& obj)
    : mTransactionID(txid)
    , mLedger(ledger)
    , mNodes(obj.getFieldArray(sfAffectedNodes))
{
	try {
		mResult = obj.getFieldU16(sfTransactionResult);
	}
	catch (std::exception const&)
	{
		//For compatibility with older-consensus versions
		// SF_U8 const sfTransactionResultOld(STI_UINT8, 3, "TransactionResult");
		mResult = obj.getFieldU8(sfTransactionResultOld);
	}
    
    mIndex = obj.getFieldU32 (sfTransactionIndex);

    if (obj.isFieldPresent(sfContractDetailMsg))
    {
         Blob msgBlob = obj.getFieldVL(sfContractDetailMsg);
         mContractDetailMsg = std::string(msgBlob.begin(), msgBlob.end());
    }
    if (obj.isFieldPresent(sfContractLogs))
    {
        contractLogData = obj.getFieldVL(sfContractLogs);
    }
    if (obj.isFieldPresent(sfContractTxs))
    {
        contractTxsData = obj.getFieldVL(sfContractTxs);
    }

    auto affectedNodes =
        dynamic_cast<STArray const*>(obj.peekAtPField(sfAffectedNodes));
    assert(affectedNodes);
    if (affectedNodes)
        mNodes = *affectedNodes;

    if (obj.isFieldPresent(sfDeliveredAmount))
        setDeliveredAmount(obj.getFieldAmount(sfDeliveredAmount));
}

TxMeta::TxMeta(uint256 const& txid, std::uint32_t ledger, Blob const& vec)
    : TxMeta(txid, ledger, vec, CtorHelper())
{
}

TxMeta::TxMeta(
    uint256 const& txid,
    std::uint32_t ledger,
    std::string const& data)
    : TxMeta(txid, ledger, data, CtorHelper())
{
}

TxMeta::TxMeta(uint256 const& transactionID, std::uint32_t ledger)
    : mTransactionID(transactionID)
    , mLedger(ledger)
    , mIndex(static_cast<std::uint32_t>(-1))
    , mResult(255)
    , mContractDetailMsg("")
    , mNodes(sfAffectedNodes)
{
    mNodes.reserve(32);
}

void
TxMeta::setAffectedNode(
    uint256 const& node,
    SField const& type,
    std::uint16_t nodeType)
{
    // make sure the node exists and force its type
    for (auto& n : mNodes)
    {
        if (n.getFieldH256(sfLedgerIndex) == node)
        {
            n.setFName(type);
            n.setFieldU16(sfLedgerEntryType, nodeType);
            return;
        }
    }

    mNodes.push_back(STObject(type));
    STObject& obj = mNodes.back();

    assert(obj.getFName() == type);
    obj.setFieldH256(sfLedgerIndex, node);
    obj.setFieldU16(sfLedgerEntryType, nodeType);
}

boost::container::flat_set<AccountID>
TxMeta::getAffectedAccounts(beast::Journal j) const
{
    boost::container::flat_set<AccountID> list;
    list.reserve(10);

    // This code should match the behavior of the JS method:
    // Meta#getAffectedAccounts
    for (auto const& it : mNodes)
    {
        int index = it.getFieldIndex(
            (it.getFName() == sfCreatedNode) ? sfNewFields : sfFinalFields);

        if (index != -1)
        {
            const STObject* inner =
                dynamic_cast<const STObject*>(&it.peekAtIndex(index));
            assert(inner);
            if (inner)
            {
                for (auto const& field : *inner)
                {
                    if (auto sa = dynamic_cast<STAccount const*>(&field))
                    {
                        assert(!sa->isDefault());
                        if (!sa->isDefault())
                            list.insert(sa->value());
                    }
                    else if (
                        (field.getFName() == sfLowLimit) ||
                        (field.getFName() == sfHighLimit) ||
                        (field.getFName() == sfTakerPays) ||
                        (field.getFName() == sfTakerGets))
                    {
                        const STAmount* lim =
                            dynamic_cast<const STAmount*>(&field);

                        if (lim != nullptr)
                        {
                            auto issuer = lim->getIssuer();

                            if (issuer.isNonZero())
                                list.insert(issuer);
                        }
                        else
                        {
                            JLOG(j.fatal()) << "limit is not amount "
                                            << field.getJson(JsonOptions::none);
                        }
                    }
                }
            }
        }
    }

    return list;
}

STObject&
TxMeta::getAffectedNode(SLE::ref node, SField const& type)
{
    uint256 index = node->key();
    for (auto& n : mNodes)
    {
        if (n.getFieldH256(sfLedgerIndex) == index)
            return n;
    }
    mNodes.push_back(STObject(type));
    STObject& obj = mNodes.back();

    assert(obj.getFName() == type);
    obj.setFieldH256(sfLedgerIndex, index);
    obj.setFieldU16(sfLedgerEntryType, node->getFieldU16(sfLedgerEntryType));

    return obj;
}

STObject&
TxMeta::getAffectedNode(uint256 const& node)
{
    for (auto& n : mNodes)
    {
        if (n.getFieldH256(sfLedgerIndex) == node)
            return n;
    }
    assert(false);
    Throw<std::runtime_error>("Affected node not found");
    return *(mNodes.begin());  // Silence compiler warning.
}

STObject
TxMeta::getAsObject() const
{
    STObject metaData(sfTransactionMetaData);
    assert(mResult != 255);
    metaData.setFieldU16(sfTransactionResult, mResult);
    if(!mContractDetailMsg.empty()) 
        metaData.setFieldVL(sfContractDetailMsg, Slice(mContractDetailMsg.data(), mContractDetailMsg.size()));
    if (!contractLogData.empty())
        metaData.setFieldVL(sfContractLogs, contractLogData);
    if (!contractTxsData.empty())
        metaData.setFieldVL(sfContractTxs, contractTxsData);
    metaData.setFieldU32(sfTransactionIndex, mIndex);
    metaData.emplace_back(mNodes);
    if (hasDeliveredAmount())
        metaData.setFieldAmount(sfDeliveredAmount, getDeliveredAmount());
    return metaData;
}

void
TxMeta::addRaw(Serializer& s, STer result, std::uint32_t index)
{
    mResult = TERtoInt (result.ter);
    mContractDetailMsg = result.msg;
    mIndex = index;
    //assert ((mResult == tesSUCCESS) || ((mResult >= tecCLAIM) && (mResult < tefFAILURE)));

    mNodes.sort([](STObject const& o1, STObject const& o2) {
        return o1.getFieldH256(sfLedgerIndex) < o2.getFieldH256(sfLedgerIndex);
    });

	STObject obj = getAsObject();
	//sub tx
	if (!contractTxsData.empty())
	{
		obj.setFieldVL(sfContractTxs, contractTxsData);
		contractTxsData.clear();
	}
    //log
    if (!contractLogData.empty())
    {
        contractLogData.clear();
    }
    obj.add (s);
}

void TxMeta::setContractFieldData(STTx const& tx)
{
    if (!tx.checkChainsqlContractType(tx.getTxnType()))  return;

    //set sub tx info
    auto vecTxs = tx.getSubTxs();

    if (!contractTxsData.empty())  contractTxsData.clear();
    //
    Json::Value vec;
    for (auto tx : vecTxs) {
        vec.append(tx.getJson(JsonOptions::none));
    }
    if (vec.size() > 0)
    {
        contractTxsData = ripple::strCopy(vec.toStyledString());
    }

    //set log info
    auto jsonLog = tx.getLogs();
    if (!contractLogData.empty())  contractLogData.clear();

    if (jsonLog.size() > 0)
    {
        contractLogData = ripple::strCopy(jsonLog.toStyledString());
    }

}

}  // namespace ripple
