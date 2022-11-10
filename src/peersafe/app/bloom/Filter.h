#pragma once

#include <memory>
#include <vector>
#include <tuple>

#include <ripple/basics/base_uint.h>
#include <ripple/app/ledger/Ledger.h>

#include <peersafe/schema/Schema.h>

namespace ripple {

class Filter {
public:
    using pointer = std::shared_ptr<Filter>;
    
    static pointer newBlockFilter(
        Schema& schame,
        const uint256& blockHash,
        const std::vector<uint160>& addresses,
        const std::vector<std::vector<uint256>>& topics);
    
    std::tuple<Json::Value, bool> Logs();
    
    Filter() = delete;
    ~Filter();
private:
    Filter(
        Schema& schame,
        const std::vector<uint160>& addresses,
        const std::vector<std::vector<uint256>>& topics);
    
    std::tuple<Json::Value, bool> blockLogs(const Ledger* ledger);
    std::tuple<Json::Value, bool> checkMatches(const Ledger* ledger);
    
    Schema& schame_;
    uint256 blockhash_;
    std::vector<uint160> addresses_;
    std::vector<std::vector<uint256>> topics_;
};

} // namespace ripple

