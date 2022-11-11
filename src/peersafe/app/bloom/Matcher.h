#pragma once

#include <memory>
#include <vector>
#include <array>
#include <tuple>

#include <ripple/basics/Slice.h>
#include <ripple/json/json_value.h>
#include <ripple/protocol/Protocol.h>

namespace ripple {

class Matcher {
public:
    using pointer = std::shared_ptr<Matcher>;
    using bloomIndexes = std::array<uint32_t, 3>;
    
    static pointer newMatcher(const uint32_t sectionSize,
                              const std::vector<std::vector<Slice>>& filters);
    
    Matcher() = delete;
    Matcher(const uint32_t sectionSize,
            const std::vector<std::vector<bloomIndexes>>& filters);
    ~Matcher();
    
    std::tuple<Json::Value, bool> execute(const LedgerIndex& from,
                                          const LedgerIndex& to);
    
private:
    static bloomIndexes calcBloomIndexes(const Slice& s);
    
    uint32_t sectionSize_;
    std::vector<std::vector<bloomIndexes>> filters_;
};

} // namespace ripple
