#pragma once

#include <memory>
#include <vector>
#include <tuple>

#include <ripple/basics/base_uint.h>
#include <ripple/app/ledger/Ledger.h>

#include <peersafe/schema/Schema.h>
#include <peersafe/app/bloom/Matcher.h>

namespace ripple {

class Filter {
public:
    using pointer = std::shared_ptr<Filter>;
    
    static pointer newBlockFilter(Schema& schame,
                                  const uint256& blockHash,
                                  const std::vector<uint160>& addresses,
                                  const std::vector<std::vector<uint256>>& topics);
    
    static pointer newRangeFilter(Schema& schame,
                                  const LedgerIndex& from,
                                  const LedgerIndex& to,
                                  const std::vector<uint160>& addresses,
                                  const std::vector<std::vector<uint256>>& topics);
    
    std::tuple<Json::Value, bool> Logs();
    
    Filter() = delete;
    Filter(Schema& schame,
           const std::vector<uint160>& addresses,
           const std::vector<std::vector<uint256>>& topics);
    ~Filter();
    
private:
    
    std::tuple<Json::Value, bool> blockLogs(const Ledger* ledger);
    std::tuple<Json::Value, bool> checkMatches(const Ledger* ledger);
    std::tuple<Json::Value, bool> unindexedLogs(const LedgerIndex& end);
    std::tuple<Json::Value, bool> indexedLogs(const LedgerIndex& end);
    
    std::tuple<uint32_t, uint32_t> bloomStatus();
    
    Schema& schame_;
    uint256 blockhash_;
    Matcher::pointer matcher_;
    LedgerIndex from_;
    LedgerIndex to_;
    std::vector<uint160> addresses_;
    std::vector<std::vector<uint256>> topics_;
};

} // namespace ripple

