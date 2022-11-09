#ifndef ETH_API_HELPERS_UTIL_H_INCLUDED
#define ETH_API_HELPERS_UTIL_H_INCLUDED

#include <ripple/basics/Slice.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/ErrorCodes.h>
#include <boost/format.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <eth/vm/utils/keccak.h>
#include <eth/tools/FixedHash.h>
#include <peersafe/app/util/Common.h>
#include <unordered_set>

namespace ripple {

class STTx;
class OpenView;
class TransactionSkeleton;

const std::uint64_t weiPerDrop = std::uint64_t(1e12);
const std::uint64_t compressDrop = std::uint64_t(1e3);
const std::uint64_t weiPerDropWithFeature = std::uint64_t(1e9);


std::string inline dropsToWeiHex(uint64_t drops)
{
    boost::multiprecision::uint128_t wei;
    boost::multiprecision::multiply(wei, drops, weiPerDrop);
    return toHexString(wei);
}

std::string inline compressDrops2Str(
    uint64_t drops,
    bool bGasPriceFeatureEnabled)
{
    boost::multiprecision::uint128_t cDrops;
    // 1e9 = 1e(-3) * weiPerDrop
    if (bGasPriceFeatureEnabled)
        boost::multiprecision::multiply(cDrops, drops, weiPerDropWithFeature);
    else
        boost::multiprecision::multiply(cDrops, drops, weiPerDrop);

    return toHexString(cDrops);
}

std::string
ethAddrChecksum(std::string addr);

Json::Value
formatEthError(int code);

Json::Value
formatEthError(int code, std::string const& msg);

Json::Value
formatEthError(int code, error_code_i rpcCode);

void
ethLdgIndex2chainsql(Json::Value& params, std::string ledgerIndexStr);

uint64_t
getChainID(std::shared_ptr<OpenView const> const& ledger);

eth::h160
addressFromSecret(std::string const& secret);

TransactionSkeleton
toTransactionSkeleton(Json::Value const& _json);

Json::Value
parseContractLogs(Json::Value const& jvLogs, Json::Value const& jvResult = Json::nullValue);

}  // namespace ripple

#endif
