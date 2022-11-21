#include <ripple/app/ledger/TransactionMaster.h>
#include <ripple/json/json_reader.h>

#include <peersafe/app/bloom/FilterHelper.h>

#include <eth/api/utils/Helpers.h>

namespace ripple {
namespace Helper {

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
            
            auto tx = txn->getSTransaction();
            auto type = tx->getFieldU16(sfTransactionType);
            if (type != ttETH_TX && type != ttCONTRACT)
                continue;

            value["blockHash"] = "0x" + to_string(ledger->info().hash);
            value["blockNumber"] = txn->getLedger();
            if (tx->isFieldPresent(sfContractAddress)) {
                uint160 toAccount = uint160(tx->getAccountID(sfContractAddress));
                value["to"] = "0x" + to_string(toAccount);
            } else {
                value["to"] = Json::nullValue;
            }

            std::string contractAddress =
                "0x" + to_string(uint160(*getContractAddress(*tx)));
            if(!txn->getMeta().empty()) {
                auto meta = std::make_shared<TxMeta>(txn->getID(),
                                                     txn->getLedger(),
                                                     txn->getMeta());
                if(meta == nullptr) {
                    return std::make_tuple(result, false);
                }
                Blob logData = meta->getContractLogData();
                if(!logData.empty())
                {
                    std::string logDataStr = std::string(logData.begin(), logData.end());
                    Json::Value logs;
                    Json::Reader().parse(logDataStr, logs);
                    Json::Value parsedLog = parseContractLogs(logs,contractAddress, value);
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

Json::Value
filterLogs(const Json::Value& unfilteredLogs,
           const std::uint32_t from,
           const std::uint32_t to,
           const std::vector<uint160>& addresses,
           const std::vector<std::vector<uint256>>& topics) {
    Json::Value result(Json::arrayValue);
Logs:
    for (auto const& logs : unfilteredLogs) {
        for(auto const& log: logs) {
            LedgerIndex seq = log["blockNumber"].asUInt();
            if(from > 0 && from > seq) {
                continue;
            }
            if(to > 0 && to < seq) {
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
    }
    return result;
}

} // namespace Helper
} // namespace ripple
