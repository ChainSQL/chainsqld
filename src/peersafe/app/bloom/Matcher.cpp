#include <ripple/protocol/digest.h>

#include <peersafe/app/bloom/Matcher.h>

namespace ripple {

Matcher::Matcher(const uint32_t sectionSize,
                 const std::vector<std::vector<Matcher::bloomIndexes>>& filters)
: sectionSize_(sectionSize)
, filters_(filters) {
    
}

Matcher::~Matcher() {
    
}

Matcher::bloomIndexes
Matcher::calcBloomIndexes(const Slice& s) {
    Matcher::bloomIndexes idxs;
    auto hash = sha512Half<CommonKey::sha3>(s);
    auto bytes = hash.data();
    for(auto i = 0; i < idxs.size(); i++) {
        idxs[i] = (uint(bytes[2*i])<<8)&2047 + uint(bytes[2*i+1]);
    }
    return  idxs;
}

Matcher::pointer
Matcher::newMatcher(const uint32_t sectionSize,
                    const std::vector<std::vector<Slice>>& filters) {
    std::vector<std::vector<Matcher::bloomIndexes>> bloomFilters;
    for(auto const& filter: filters) {
        if(filter.empty())
            continue;
        std::vector<Matcher::bloomIndexes> indexs;
        for(auto const& f: filter) {
            indexs.push_back(calcBloomIndexes(f));
        }
        bloomFilters.push_back(indexs);
    }
    Matcher::pointer matcher = std::make_shared<Matcher>(sectionSize, bloomFilters);
    return matcher;
}

std::tuple<Json::Value, bool>
Matcher::execute(const LedgerIndex& from,
                 const LedgerIndex& to) {
    return std::make_tuple(Json::Value(), false);
}

}
