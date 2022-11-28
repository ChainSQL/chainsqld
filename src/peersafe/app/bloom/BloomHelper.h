#pragma once

#include <vector>
#include <ripple/json/json_value.h>
#include <ripple/basics/base_uint.h>
#include <ripple/protocol/AccountID.h>

namespace ripple {
class BloomHelper
{
public:
    BloomHelper();

    void clearLogCache();
    void addContractLog(AccountID const& address,Json::Value const& log);
    uint2048 calcBloom();

private:
    std::vector<std::pair<AccountID,Json::Value>> vecLogs_;
};

}  // namespace ripple
