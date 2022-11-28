#include <ripple/basics/base_uint.h>
#include <ripple/core/JobQueue.h>
#include <ripple/protocol/SecretKey.h>
#include <ripple/ledger/ReadView.h>

#include <peersafe/app/bloom/FilterApi.h>
#include <peersafe/app/bloom/FilterHelper.h>

#define PERIOD_CHECK 30s
#define DEADLINE 600

namespace ripple {

FilterApi::FilterWrapper::FilterWrapper(int32_t _type)
: type(_type)
, filter(nullptr)
, mutex()
, blockHashs()
, logs(Json::arrayValue)
, deadline(std::chrono::steady_clock::now()) {
    
}

Json::Value
FilterApi::FilterWrapper::result() {
    // update deadline
    deadline = std::chrono::steady_clock::now();
    if(type == NewPendingFilter) {
        return std::get<0>(filter->getPendingTransactions());
    } else if (type == NewFilter) {
        Json::Value returnLogs;
        {
            std::lock_guard<std::mutex> locl(mutex);
            returnLogs = logs;
            logs = Json::Value(Json::arrayValue);
        }
        return Helper::filterLogs(returnLogs,
                                  filter->fromBlock(),
                                  filter->toBlock(),
                                  filter->addresses(),
                                  filter->topics());
    } else if (type == NewBlockFilter) {
        std::vector<std::string> hashs;
        {
            std::lock_guard<std::mutex> locl(mutex);
            blockHashs.swap(hashs);
        }
        Json::Value logs(Json::arrayValue);
        for(auto const& hash : hashs) {
            logs.append(hash);
        }
        return logs;
    }
    return Json::Value(Json::arrayValue);
}

void
FilterApi::FilterWrapper::onPubLedger(std::shared_ptr<ReadView const> const& lpAccepted) {
    std::lock_guard<std::mutex> locl(mutex);
    blockHashs.push_back("0x" + to_string(lpAccepted->info().hash));
}

void
FilterApi::FilterWrapper::appendLogs(const Json::Value& logs) {
    std::lock_guard<std::mutex> locl(mutex);
    for(auto const& l : logs) {
        this->logs.append(l);
    }
}

bool
FilterApi::FilterWrapper::isDeadline() const {
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - deadline).count();
    return duration > DEADLINE;
}

FilterApi::FilterApi(Schema& schame)
: mutex_()
, filters_()
, schame_(schame)
, checkTimer_(schame.getIOService()){
    setCheckTimer();
}

FilterApi::~FilterApi() {
    checkTimer_.cancel();
    filters_.clear();
}

std::string
FilterApi::installPendingTransactionFilter() {
    std::string id;
    FilterWrapper::pointer wrapper = std::make_shared<FilterWrapper>(FilterApi::FilterWrapper::NewPendingFilter);
    if(!wrapper) {
        return id;
    }
    assert(wrapper->type == FilterApi::FilterWrapper::NewPendingFilter);
    wrapper->filter = Filter::newFilter(schame_);
    if(wrapper->filter == nullptr) {
        return id;
    }
    
    id = newID();
    std::lock_guard<std::mutex> lock(mutex_);
    filters_[id] = wrapper;
    
    return id;
}

std::string
FilterApi::installFilter(const FilterQuery& query) {
    std::string id;
    FilterWrapper::pointer wrapper = std::make_shared<FilterWrapper>(FilterApi::FilterWrapper::NewFilter);
    assert(wrapper->type == FilterApi::FilterWrapper::NewFilter);
    if(query.blockHash != beast::zero) {
        wrapper->filter = Filter::newBlockFilter(schame_,
                                                query.blockHash,
                                                query.addresses,
                                                query.topics);
    } else {
        wrapper->filter = Filter::newRangeFilter(schame_,
                                                query.fromBlock,
                                                query.toBlock,
                                                query.addresses,
                                                query.topics);
    }
    
    if(wrapper->filter == nullptr) {
        return id;
    }
    
    id = newID();
    std::lock_guard<std::mutex> lock(mutex_);
    filters_[id] = wrapper;
    
    return id;
}

std::string
FilterApi::installBlockFilter() {
    std::string id;
    FilterWrapper::pointer wrapper = std::make_shared<FilterWrapper>(FilterApi::FilterWrapper::NewBlockFilter);
    assert(wrapper->type == FilterApi::FilterWrapper::NewBlockFilter);
    wrapper->filter = Filter::newFilter(schame_);
    if(wrapper->filter == nullptr) {
        return id;
    }
    
    id = newID();
    std::lock_guard<std::mutex> lock(mutex_);
    filters_[id] = wrapper;
    
    return id;
}

FilterApi::FilterWrapper::pointer
FilterApi::getFilter(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = filters_.find(id);
    if(it == std::end(filters_)) {
        return {};
    }
    return it->second;
}

bool FilterApi::uninstallFilter(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = filters_.find(id);
    if(it == std::end(filters_)) {
        return false;
    }
    filters_.erase(it);
    return true;
}

void
FilterApi::onPubLedger(std::shared_ptr<ReadView const> const& lpAccepted) {
    schame_
        .getJobQueue()
        .addJob(jtFilterAPI,
                "FilterAPI",
                [lpAccepted, this](Job&) {
            handlePubLedger(lpAccepted);
        },schame_.doJobCounter());
}

void
FilterApi::handlePubLedger(std::shared_ptr<ReadView const> const& lpAccepted) {
    std::map<std::string, FilterWrapper::pointer> filters;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        filters = filters_;
    }
    
    for(auto const& it : filters) {
        FilterApi::FilterWrapper::pointer filter = it.second;
        if(filter->type == FilterApi::FilterWrapper::NewBlockFilter) {
            filter->onPubLedger(lpAccepted);
        } else if (filter->type == FilterApi::FilterWrapper::NewFilter) {
            auto ledger = dynamic_cast<const Ledger*>(lpAccepted.get());
            if(ledger == nullptr) {
                assert(ledger);
                continue;
            }
            auto result = Helper::getLogsByLedger(schame_, ledger);
            if(std::get<1>(result)) {
                filter->appendLogs(std::get<0>(result));
            }
        }
    }
}

std::string
FilterApi::newID() {
    SecretKey key = randomSecretKey();
    return key.to_string();
}

void FilterApi::setCheckTimer() {
    auto checkTimer = [&](boost::system::error_code const& e) {
        if((e.value() == boost::system::errc::success)) {
            std::map<std::string, FilterWrapper::pointer> filters;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                filters.swap(filters_);
            }
            
            for(auto it = filters.begin(); it != filters.end();) {
                if(it->second->isDeadline()) {
                    it = filters.erase(it);
                } else {
                    it++;
                }
            }
            
            if(filters.empty() == false) {
                std::lock_guard<std::mutex> lock(mutex_);
                for(auto const& it : filters) {
                    filters_[it.first] = it.second;
                }
            }
            setCheckTimer();
        }
    };
    
    using namespace std::chrono_literals;
    checkTimer_.expires_from_now(PERIOD_CHECK);
    checkTimer_.async_wait(std::move(checkTimer));
}

} // namespace ripple
