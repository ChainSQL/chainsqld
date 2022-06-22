
#include <ripple/protocol/STValidationSet.h>
#include <ripple/protocol/STArray.h>


namespace ripple {


STValidationSet::STValidationSet(
    SerialIter& sit,
    PublicKey const& publicKey,
    ManifestCache& cache)
    : STObject(validationSetFormat(), sit, sfValidationSet)
    , mPublic(publicKey)
    , mSet(std::vector<std::shared_ptr<STValidation>>())

{
     // This is our own public key and it should always be valid.
    if (!publicKeyType(publicKey))
        LogicError("Invalid STValidationSet public key");
    mValidationSeq = getFieldU32(sfSequence);
    mValidationHash = getFieldH256(sfLedgerHash);
    auto validations = getFieldArray(sfValidations);
    for (auto const& item : validations)
    {
        if (item.getFieldVL(sfPublicKey).size() == 0)
            continue;
        Blob const& publickey = item.getFieldVL(sfPublicKey);
        PublicKey const publicKey{makeSlice(publickey)};

        Blob const& raw = item.getFieldVL(sfRaw);
        SerialIter sit(makeSlice(raw));
        mSet.emplace_back(std::make_shared<STValidation>(
            std::ref(sit), publicKey, [&](PublicKey const& pk) {
                    return calcNodeID(cache.getMasterKey(pk));
                }));
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
