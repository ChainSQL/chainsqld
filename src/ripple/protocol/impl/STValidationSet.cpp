
#include <ripple/protocol/STValidationSet.h>
#include <ripple/protocol/STArray.h>


namespace ripple {


STValidationSet::STValidationSet(
    SerialIter& sit,
    PublicKey const& publicKey,
    ManifestCache& cache)
    : STObject(validationSetFormat(), sit, sfValidationSet)
    , mPublic(publicKey)

{
     // This is our own public key and it should always be valid.
    if (!publicKeyType(publicKey))
        LogicError("Invalid STValidationSet public key");
    assert(mNodeID.isNonZero());
    mValidationSeq = getFieldU32(sfSequence);
    mValidationHash = getFieldH256(sfLedgerHash);
    auto validations = getFieldArray(sfValidations);
    for (auto const& item : validations)
    {
        if (item.getFieldVL(sfPublicKey).size() == 0)
            continue;
        PublicKey const publicKey{makeSlice(item.getFieldVL(sfPublicKey))};
        SerialIter sit(makeSlice(item.getFieldVL(sfRaw)));
        mSet.emplace_back(std::make_shared<STValidation>(
            std::ref(sit), publicKey, [&](PublicKey const& pk) {
                    return calcNodeID(cache.getMasterKey(pk));
                }));
        /*mSet.emplace_back(std::make_shared<STValidation>(
            item.getFieldVL(sfRaw), publicKey, [&](PublicKey const& pk) {
                        return calcNodeID(pk);
                    }));*/
    }
}

STValidationSet::STValidationSet(
    std::uint32_t validationSeq,
    uint256 validationHash,
    PublicKey const& publicKey,
    valSet set)
    : STObject(validationSetFormat(), sfValidationSet)
    , mValidationSeq(validationSeq)
    , mValidationHash(validationHash)
    , mPublic(publicKey)
    , mSet(set)
{
    // This is our own public key and it should always be valid.
    if (!publicKeyType(publicKey))
        LogicError("Invalid STValidationSet public key");

    setFieldU32(sfSequence, mValidationSeq);
    setFieldH256(sfLedgerHash,mValidationHash);
    if (set.size() > 0)
    {
        STArray validations ;
        for (auto const& item : set)
        {
            STObject obj(sfNewFields);
            obj.setFieldVL(sfPublicKey, item->getSignerPublic());
            obj.setFieldVL(sfRaw, item->getSerialized());
            validations.push_back(obj);
        }
        setFieldArray(sfValidations, validations);
    }
}

//std::shared_ptr<RCLTxSet>
//STValidationSet::getTxSet(Schema& app) const
//{
//    if (isFieldPresent(sfTransactions))
//    {
//        auto initialSet = std::make_shared<SHAMap>(
//            SHAMapType::TRANSACTION, app.getNodeFamily());
//        initialSet->setUnbacked();
//
//        auto& txs = getFieldArray(sfTransactions);
//        for (auto& obj : txs)
//        {
//            uint256 txID = obj.getFieldH256(sfTransactionHash);
//            auto raw = obj.getFieldVL(sfRaw);
//            Serializer s(raw.data(), raw.size());
//            initialSet->addItem(
//                SHAMapItem(txID, std::move(s)), true, false);
//        }
//        return std::make_shared<RCLTxSet>(initialSet);
//    }
//    else
//    {
//        return nullptr;
//    }
//}

Blob STValidationSet::getSerialized() const
{
    Serializer s;
    add(s);
    return s.peekData();
}

const STValidationSet::valSet& 
STValidationSet::getValSet()
{
    return mSet;
}


SOTemplate const& STValidationSet::validationSetFormat()
{
    static SOTemplate const format
    {
        { sfSequence,         soeREQUIRED },  
        { sfLedgerHash,       soeREQUIRED },
        { sfValidations,      soeREQUIRED },

    };
    return format;
}

} // ripple
