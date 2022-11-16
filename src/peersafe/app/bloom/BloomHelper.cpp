
#include <peersafe/app/bloom/Bloom.h>
#include <peersafe/app/bloom/BloomHelper.h>
#include <eth/api/utils/Helpers.h>
#include <ripple/protocol/digest.h>

namespace ripple {


BloomHelper::BloomHelper()
{
}

void
BloomHelper::clearLogCache()
{
    vecLogs_.clear();
}

void
BloomHelper::addContractLog(AccountID const& address,Json::Value const& log)
{
    if (log.size() > 0)
        vecLogs_.push_back(std::make_pair(address,log));
}

uint2048
BloomHelper::calcBloom()
{
    Bloom bloom;
    for (auto const& jvLog : vecLogs_)
    {
        auto txLogs = parseContractLogs(jvLog.second, "");
        for (auto const& log : txLogs)
        {
            bloom.add(Slice(jvLog.first.data(),jvLog.first.size()));
            for (auto const& topic : log["topics"])
            {
                auto hash = from_hex_text<uint256>(topic.asString().substr(2));
                bloom.add(Slice(hash.data(),hash.size()));
            }
        }
    }
    return std::move(bloom.value());
}

}