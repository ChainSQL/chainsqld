#pragma once

#include <map>
#include <mutex>
#include <chrono>

#include <boost/asio.hpp>

#include <ripple/basics/base_uint.h>
#include <ripple/json/json_value.h>

#include <peersafe/schema/Schema.h>
#include <peersafe/app/bloom/Filter.h>

namespace ripple {

class ReadView;

struct FilterQuery {
    uint256 blockHash;
    LedgerIndex fromBlock;
    LedgerIndex toBlock;
    std::vector<uint160> addresses;
    std::vector<std::vector<uint256>> topics;
};

class FilterApi {
public:
    
    class FilterWrapper {
    public:
        using pointer = std::shared_ptr<FilterWrapper>;
        
        enum {
            NewBlockFilter = 0,
            NewFilter,
            NewPendingFilter
        };
        
        FilterWrapper() = delete;
        FilterWrapper(int32_t type);
        
        int32_t type;
        Filter::pointer filter;
        
        Json::Value result();
        
        void onPubLedger(std::shared_ptr<ReadView const> const& lpAccepted);
        void appendLogs(const Json::Value& logs);
        
        bool isDeadline() const;
        
    private:
        std::mutex mutex;
        std::vector<std::string> blockHashs;
        Json::Value logs;
        std::chrono::steady_clock::time_point deadline;
    };
    
    FilterApi() = delete;
    FilterApi(Schema& schame);
    ~FilterApi();
    
    std::string installPendingTransactionFilter();
    std::string installFilter(const FilterQuery& query);
    std::string installBlockFilter();
    
    FilterWrapper::pointer getFilter(const std::string& id);
    
    bool uninstallFilter(const std::string& id);
    
    void onPubLedger(std::shared_ptr<ReadView const> const& lpAccepted);
    
private:
    std::string newID();
    void handlePubLedger(std::shared_ptr<ReadView const> const& lpAccepted);
    void setCheckTimer();
    
    std::mutex mutex_;
    std::map<std::string, FilterWrapper::pointer> filters_;
    Schema& schame_;
    boost::asio::steady_timer checkTimer_;
};

} // namespace ripple
