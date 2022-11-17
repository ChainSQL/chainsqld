#include <ripple/protocol/digest.h>

#include <peersafe/app/bloom/BloomManager.h>
#include <peersafe/app/bloom/BloomIndexer.h>
#include <peersafe/app/bloom/Matcher.h>

namespace ripple {

Blob AndBlob(const Blob& a, const Blob& b) {
    assert(a.size() == b.size());
    Blob result;
    std::size_t size = a.size();
    result.resize(size);
    for(std::size_t i = 0; i < size; i++) {
        result[i] = a[i] & b[i];
    }
    return result;
}

Blob OrBlob(const Blob& a, const Blob& b) {
    assert(a.size() == b.size());
    Blob result;
    std::size_t size = a.size();
    result.resize(size);
    for(std::size_t i = 0; i < size; i++) {
        result[i] = a[i] | b[i];
    }
    return result;
}

Matcher::Matcher(Schema& schame,
                 const uint32_t sectionSize,
                 const std::vector<std::vector<Matcher::bloomIndexes>>& filters)
: schame_(schame)
, sectionSize_(sectionSize)
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
        uint32_t x = (uint32_t(bytes[2*i])<<8)&2047;
        uint32_t y = uint32_t(bytes[2*i+1]);
        idxs[i] = x + y;
    }
    return  idxs;
}

Matcher::pointer
Matcher::newMatcher(Schema& schame,
                    const uint32_t sectionSize,
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
    Matcher::pointer matcher = std::make_shared<Matcher>(schame, sectionSize, bloomFilters);
    return matcher;
}

std::vector<LedgerIndex>
Matcher::execute(const LedgerIndex& from,
                 const LedgerIndex& to) {
    uint32_t fromSections = schame_.getBloomManager().getSectionBySeq(from);
    uint32_t toSections = schame_.getBloomManager().getSectionBySeq(to);
    
    std::map<uint32_t, Blob> partialMatch;
    for(uint32_t section = fromSections; section <= toSections; section++) {
        Blob next(sectionSize_/8, 0xFF);
        for(auto const& bloom: filters_) {
            next = subMatch(next, section, bloom);
        }
        partialMatch[section] = next;
    }
    
    std::vector<LedgerIndex> matchedLedgers;
    for(auto const& match: partialMatch) {
        LedgerIndex sectionStart = 0;
        LedgerIndex sectionEnd = 0;
        std::tie(sectionStart, sectionEnd) = schame_.getBloomManager().getSectionRange(match.first);
        
        LedgerIndex first = sectionStart;
        if(from > first) {
            first = from;
        }
    
        LedgerIndex last = sectionEnd;
        if(to < last) {
            last = to;
        }
        
        // Iterate over all the blocks in the section
        // and return the matching ones
        first = first - sectionStart;
        last = last - sectionStart;
        for(auto i = first; i <= last; i ++) {
            // Skip the entire byte if no matches
            // are found inside (and we're processing an entire byte!)
            unsigned char next = match.second[i/8];
            if(next == 0) {
                if((i % 8) == 0) {
                    i += 7;
                }
                continue;
            }
            
            // Some bit it set, do the actual submatching
            uint32_t bit = 7 - i%8;
            bool matched = (next & (1 << bit)) != 0;
            if(matched) {
                matchedLedgers.push_back(i + 1);
            }
        }
    }
    
    return matchedLedgers;
}

Blob Matcher::subMatch(const Blob& next,
                       const uint32_t section,
                       const std::vector<bloomIndexes>& bloom) {
    Blob orResult;
    for(auto const index: bloom) {
        Blob andResult;
        for(uint32_t const& bit: index) {
            Blob blob = schame_.getBloomManager().bloomIndexer().getBloomBits(bit, section);
            if(!blob.empty()) {
                assert(sectionSize_/8 == blob.size());
            }
            
            if(andResult.empty()) {
                andResult = blob;
            } else {
                andResult = AndBlob(andResult, blob);
            }
        }
        
        if(orResult.empty()) {
            orResult = andResult;
        } else {
            orResult = OrBlob(orResult, andResult);
        }
    }
    
    if(orResult.empty()) {
        orResult.resize(sectionSize_/8);
    }
    
    if(!next.empty()) {
        orResult = AndBlob(orResult, next);
    }
    return orResult;
}

}
