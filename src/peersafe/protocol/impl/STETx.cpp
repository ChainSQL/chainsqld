//
//  STETx.cpp
//  chainsqld
//
//  Created by luleigreat on 2022/8/9.
//

#include <ripple/protocol/HashPrefix.h>
#include <ripple/protocol/Protocol.h>
#include <peersafe/protocol/STETx.h>
#include <utility>

namespace ripple {

STETx::STETx(SerialIter& sit, CommonKey::HashType hashType) noexcept(false)
{
    setFName(sfTransaction);
    int length = sit.getBytesLeft();

    if ((length < txMinSizeBytes) || (length > txMaxSizeBytes))
        Throw<std::runtime_error>("Transaction length invalid");

    if (set(sit))
        Throw<std::runtime_error>("Transaction contains an object terminator");

    tx_type_ = ttETH_TX;

    //Todo:
    //Decode RLP and set fields

    //Todo:
    //Calculate tx-hash
    tid_ = getHash(HashPrefix::transactionID, hashType);

    pTxs_ = nullptr;
    paJsonLog_ = nullptr;
}

std::pair<bool, std::string>
STETx::checkSign(RequireFullyCanonicalSig requireCanonicalSig) const
{
    std::pair<bool, std::string> ret{false, ""};
    try
    {
        //Todo:
        //check signature
    }
    catch (std::exception const&)
    {
        ret = {false, "Internal signature check failure."};
    }
    return ret;
}

bool
isEthTx(STObject const& tx)
{
    auto t = tx[~sfTransactionType];
    if (!t)
        return false;
    auto tt = safe_cast<TxType>(*t);
    return tt == ttETH_TX;
}

}  // namespace ripple