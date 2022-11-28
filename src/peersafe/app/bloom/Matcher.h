#pragma once

#include <memory>
#include <vector>
#include <array>
#include <tuple>
#include <map>

#include <ripple/basics/Slice.h>
#include <ripple/basics/Blob.h>
#include <ripple/json/json_value.h>
#include <ripple/protocol/Protocol.h>

#include <peersafe/schema/Schema.h>

namespace ripple {

class Matcher {
public:
    using pointer = std::shared_ptr<Matcher>;
    using bloomIndexes = std::array<uint32_t, 3>;
    
    static pointer newMatcher(Schema& schame,
                              const uint32_t sectionSize,
                              const std::vector<std::vector<Slice>>& filters);
    
    static bloomIndexes calcBloomIndexes(const Slice& s);
    
    Matcher() = delete;
    Matcher(Schema& schame,
            const uint32_t sectionSize,
            const std::vector<std::vector<bloomIndexes>>& filters);
    ~Matcher();
    
    std::vector<LedgerIndex> execute(const LedgerIndex& from,
                                     const LedgerIndex& to);
    
    
    
private:
    Blob subMatch(const Blob& next,
                  const uint32_t section,
                  const std::vector<bloomIndexes>& bloom);
    
    Schema& schame_;
    uint32_t sectionSize_;
    std::vector<std::vector<bloomIndexes>> filters_;
};

} // namespace ripple
