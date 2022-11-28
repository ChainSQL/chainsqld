#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/ledger/TransactionMaster.h>
#include <ripple/basics/Slice.h>
#include <ripple/basics/Blob.h>
#include <ripple/json/json_value.h>
#include <ripple/shamap/SHAMap.h>

#include <peersafe/app/bloom/Filter.h>
#include <peersafe/app/bloom/FilterHelper.h>
#include <peersafe/app/bloom/Bloom.h>
#include <peersafe/app/bloom/BloomManager.h>
#include <peersafe/app/bloom/BloomIndexer.h>

#include <eth/api/utils/Helpers.h>

namespace ripple {

bool bloomFilter(const uint2048& bloom,
                 const std::vector<uint160>& addresses,
                 const std::vector<std::vector<uint256>>& topics) {
    std::size_t size = addresses.size();
    if(size > 0) {
        bool included = false;
        for(auto i = 0; i < size; i++) {
            auto address = addresses[i];
            if(bloomLookup(bloom, Slice(address.data(), address.size()))) {
                included = true;
                break;
            }
        }
        if (!included) {
            return false;
        }
    }
    
    std::size_t topics_size = topics.size();
    if(topics_size > 0) {
        for(auto i = 0; i < topics_size; i++) {
            const std::vector<uint256>& sub = topics[i];
            std::size_t sub_size = sub.size();
            bool included = (sub_size == 0);
            for(auto j = 0; j < sub_size; j++) {
                auto topic = sub[j];
                if(bloomLookup(bloom, Slice(topic.data(), topic.size()))) {
                    included = true;
                    break;
                }
            }
            if(!included) {
                return false;
            }
        }
    }
    
    return true;
}

std::tuple<Json::Value, bool>
getTxsFrom(const Ledger* ledger) {
    Json::Value txs(Json::arrayValue);
    try {
        const SHAMap& txMap = ledger->txMap();
        for(auto const& item: txMap) {
            txs.append("0x" + to_string(item.key()));
        }
    } catch (std::exception& e) {
        return std::make_tuple(txs, false);
    }
    return std::make_tuple(txs, true);
}

Filter::Filter(Schema& schame)
:schame_(schame)
,blockhash_(beast::zero)
,matcher_(nullptr)
,from_(0)
,to_(0)
,addresses_()
,topics_() {
    
}

Filter::Filter(Schema& schame,
               const std::vector<uint160>& addresses,
               const std::vector<std::vector<uint256>>& topics)
:schame_(schame)
,blockhash_(beast::zero)
,matcher_(nullptr)
,from_(0)
,to_(0)
,addresses_(addresses)
,topics_(topics) {
    
}

Filter::~Filter() {
    
}

Filter::pointer
Filter::newFilter(Schema& schame) {
    Filter::pointer f = std::make_shared<Filter>(schame);
    return f;
}

Filter::pointer
Filter::newBlockFilter(Schema& schame,
                       const uint256& blockHash,
                       const std::vector<uint160>& addresses,
                       const std::vector<std::vector<uint256>>& topics) {
    Filter::pointer f = std::make_shared<Filter>(schame, addresses, topics);
    if (f) {
        f->blockhash_ = blockHash;
    }
    return f;
}

Filter::pointer
Filter::newRangeFilter(Schema& schame,
                       const LedgerIndex& from,
                       const LedgerIndex& to,
                       const std::vector<uint160>& addresses,
                       const std::vector<std::vector<uint256>>& topics) {
    std::vector<std::vector<Slice>> filters;
    
    std::vector<Slice> addressesFilter;
    for(auto const& address: addresses) {
        addressesFilter.push_back(Slice(address.data(), address.size()));
    }
    if(!addressesFilter.empty()) {
        filters.push_back(addressesFilter);
    }
    
    
    for(auto const& topic: topics) {
        std::vector<Slice> topicsFilter;
        for(auto const& t: topic) {
            topicsFilter.push_back(Slice(t.data(), t.size()));
        }
        if(!topicsFilter.empty()) {
            filters.push_back(topicsFilter);
        }
    }

    Filter::pointer f = std::make_shared<Filter>(schame, addresses, topics);
    if(f) {
        uint32_t sectionSize = 0;
        uint32_t sections = 0;
        std::tie(sectionSize, sections) = f->bloomStatus();
        
        f->matcher_ = Matcher::newMatcher(schame, sectionSize, filters);
        f->from_ = from;
        f->to_ = to;
    }
    return f;
}

std::tuple<Json::Value, bool> Filter::Logs() {
    if(blockhash_ != beast::zero) {
        auto ledger = schame_.getLedgerMaster().getLedgerByHash(blockhash_);
        if(ledger == nullptr) {
            return std::make_tuple(Json::Value(), false);
        }
        return blockLogs(ledger.get());
    }
    
    uint32_t size = 0;
    uint32_t sectons = 0;
    std::tie(size, sectons) = bloomStatus();
    auto curSectionRange = schame_.getBloomManager().getSectionRange(sectons - 1);
    uint32_t indexed = std::get<1>(curSectionRange);
    
    Json::Value logs(Json::arrayValue);
    bool bok = false;
    if(indexed >= from_) {
        if(indexed > to_) {
            std::tie(logs, bok) = indexedLogs(to_);
        } else {
            std::tie(logs, bok) = indexedLogs(indexed);
        }
        if(!bok) {
            return std::make_tuple(logs, bok);
        }
    }
    
    Json::Value uindexed;
    std::tie(uindexed, bok) = unindexedLogs(to_);
    if(bok) {
        for(auto const& l: uindexed) {
            logs.append(l);
        }
    }
    
    return std::make_tuple(logs, true);
}

std::tuple<Json::Value, bool> Filter::blockLogs(const Ledger* ledger) {
    if(bloomFilter(ledger->info().bloom, addresses_, topics_)) {
        return checkMatches(ledger);
    }
    return std::make_tuple(Json::Value(), false);
}

std::tuple<Json::Value, bool> Filter::checkMatches(const Ledger* ledger) {
    auto result = Helper::getLogsByLedger(schame_, ledger);
    if(!std::get<1>(result)) {
        return result;
    }

    Json::Value filtered = Helper::filterLogs(std::get<0>(result), 0, 0, addresses_, topics_);
    return std::make_tuple(filtered, true);
}

std::tuple<Json::Value, bool> Filter::unindexedLogs(const LedgerIndex& end) {
    Json::Value logs(Json::arrayValue);
    for(auto seq = from_; seq <= to_; seq++) {
        auto block = schame_.getLedgerMaster().getLedgerBySeq(seq);
        if (block == nullptr) {
            continue;
        }
        auto result = blockLogs(block.get());
        if(!std::get<1>(result)) {
            continue;
        }
        for(auto const& log: std::get<0>(result)) {
            logs.append(log);
        }
    }
    return std::make_tuple(logs, true);
}

std::tuple<Json::Value, bool> Filter::indexedLogs(const LedgerIndex& end) {
    Json::Value logs(Json::arrayValue);
    std::vector<LedgerIndex> matchedLedgers = matcher_->execute(from_, to_);
    for(auto const& seq: matchedLedgers) {
        auto block = schame_.getLedgerMaster().getLedgerBySeq(seq);
        if (block == nullptr) {
            continue;
        }
        
        auto result = blockLogs(block.get());
        if(!std::get<1>(result)) {
            continue;
        }
        
        for(auto const& log: std::get<0>(result)) {
            logs.append(log);
        }
    }
    from_ = end + 1;
    return std::make_tuple(logs, true);
}

std::tuple<uint32_t, uint32_t> Filter::bloomStatus() {
    return schame_.getBloomManager().bloomIndexer().bloomStatus();
}

std::tuple<Json::Value, bool>
Filter::getPendingTransactions() {
    auto closed = schame_.getLedgerMaster().getClosedLedger();
    if(closed == nullptr) {
        return std::make_tuple(Json::Value(Json::arrayValue), false);
    }
    
    return getTxsFrom(closed.get());
}

}
