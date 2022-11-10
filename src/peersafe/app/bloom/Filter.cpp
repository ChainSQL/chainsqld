#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/basics/Slice.h>
#include <ripple/json/json_value.h>
#include <ripple/shamap/SHAMap.h>
#include <ripple/app/ledger/TransactionMaster.h>
#include <ripple/basics/Blob.h>
#include <ripple/json/json_reader.h>

#include <peersafe/app/bloom/Filter.h>
#include <peersafe/app/bloom/Bloom.h>

#include <eth/api/utils/Helpers.h>

namespace ripple {

bool bloomFilter(
    const uint2048& bloom,
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
            for(auto j = 0; i < sub_size; j++) {
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
getLogsByLedger(Schema& schame, const Ledger* ledger) {
    Json::Value result(Json::arrayValue);
    try {
        const SHAMap& txMap = ledger->txMap();
        int index = 0;
        for(auto const& item: txMap) {
            Json::Value value;
            value["transactionHash"] = "0x" + to_string(item.key());
            value["transactionIndex"] = std::to_string(++index);
            
            auto txn = schame.getMasterTransaction().fetch(item.key());
            if(!txn) {
                continue;
            }
            
            value["blockHash"] = "0x" + to_string(ledger->info().hash);
            value["blockNumber"] = txn->getLedger();
            
            auto tx = txn->getSTransaction();
            if (tx->isFieldPresent(sfContractAddress)) {
                uint160 toAccount = uint160(tx->getAccountID(sfContractAddress));
                value["to"] = "0x" + to_string(toAccount);
            } else {
                value["to"] = Json::nullValue;
            }
            
            if(!txn->getMeta().empty()) {
                auto meta = std::make_shared<TxMeta>(
                    txn->getID(), txn->getLedger(), txn->getMeta());
                if(meta == nullptr) {
                    return std::make_tuple(result, false);
                }
                Blob logData = meta->getContractLogData();
                if(!logData.empty())
                {
                    std::string logDataStr = std::string(logData.begin(), logData.end());
                    Json::Value logs;
                    Json::Reader().parse(logDataStr, logs);
                    Json::Value parsedLog = parseContractLogs(logs, value);
                    result.append(parsedLog);
                }
            }
        }
    } catch (std::exception& e) {
        return std::make_tuple(result, false);
    }
    return std::make_tuple(result, true);
}


bool includes(const std::vector<uint160>& addresses, const uint160& address) {
    for(auto const& a: addresses) {
        if (a == address) {
            return true;
        }
    }
    return false;
}

Json::Value filterLogs(const Json::Value& unfilteredLogs,
                       const std::uint32_t from,
                       const std::uint32_t to,
                       const std::vector<uint160>& addresses,
                       const std::vector<std::vector<uint256>>& topics) {
    Json::Value result(Json::arrayValue);
Logs:
    for (auto const& log : unfilteredLogs) {
        LedgerIndex seq = log["blockNumber"].asUInt();
        if(from != -1 && from >= 0 && from > seq) {
            continue;
        }
        if(to != -1 && to >=0 && to < seq) {
            continue;
        }
        
        uint160 address = from_hex_text<uint160>(log["address"].asString());
        if(addresses.size() > 0 && !includes(addresses, address)) {
            continue;
        }
        
        Json::Value topicsObject = log["topics"];
        assert(topicsObject.isArray());
        // If the to filtered topics is greater than the amount of topics in logs, skip.
        if(topics.size() > topicsObject.size()) {
            continue;
        }
        
        int topics_index = 0;
        for(auto const& sub: topics) {
            // empty rule set == wildcard
            bool match = sub.size() == 0;
            for(auto const& topic: sub) {
                std::string hex_topic = "0x" + toLowerStr(to_string(topic));
                if(topicsObject[topics_index].asString() == hex_topic) {
                    match = true;
                    break;
                }
            }
            if(!match) {
                goto Logs;
            }
        }
        result.append(log);
    }
    return result;
}

Filter::Filter(
    Schema& schame,
    const std::vector<uint160>& addresses,
    const std::vector<std::vector<uint256>>& topics)
:schame_(schame)
,blockhash_(beast::zero)
,addresses_(addresses)
,topics_(topics) {
    
}

Filter::~Filter() {
    
}

Filter::pointer Filter::newBlockFilter(
    Schema& schame,
    const uint256& blockHash,
    const std::vector<uint160>& addresses,
    const std::vector<std::vector<uint256>>& topics) {
    Filter::pointer f = std::make_shared<Filter>(schame, addresses, topics);
    if (f) {
        f->blockhash_ = blockHash;
    }
    return f;
}

std::tuple<Json::Value, bool> Filter::Logs() {
    if(blockhash_ != beast::zero) {
        auto ledger = schame_.getLedgerMaster().getLedgerByHash(blockhash_);
        if(ledger) {
            return blockLogs(ledger.get());
        }
    }
    
    return std::make_tuple(Json::Value(), false);
}

std::tuple<Json::Value, bool> Filter::blockLogs(const Ledger* ledger) {
    if(bloomFilter(ledger->info().bloom, addresses_, topics_)) {
        return checkMatches(ledger);
    }
    return std::make_tuple(Json::Value(), false);
}

std::tuple<Json::Value, bool> Filter::checkMatches(const Ledger* ledger) {
    auto result = getLogsByLedger(schame_, ledger);
    if(!std::get<1>(result)) {
        return result;
    }
    
    Json::Value filtered = filterLogs(std::get<0>(result), -1, -1, addresses_, topics_);
    return std::make_tuple(filtered, true);
}

}
