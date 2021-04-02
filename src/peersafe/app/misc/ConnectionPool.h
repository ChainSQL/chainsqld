#ifndef PEERSAFE_CONNECTION_POOL_H_INCLUDED
#define PEERSAFE_CONNECTION_POOL_H_INCLUDED

#include <memory>
#include <peersafe/app/sql/TxStore.h>
#include <peersafe/schema/Schema.h>
#include <ripple/basics/chrono.h>
#include <peersafe/core/Tuning.h>

namespace ripple {

struct ConnectionUnit
{
    using clock_type = beast::abstract_clock<std::chrono::steady_clock>;
    ConnectionUnit(Schema& app)
    {
        store_ = nullptr;
        locked_ = false;
        conn_ = std::make_shared<TxStoreDBConn>(app.config());
        if (conn_ == nullptr)
            return;
        store_ = std::make_shared<TxStore>(
            conn_->GetDBConn(), app.config(), app.journal("RPCHandler"));
        last_access_ = stopwatch().now();
    }
    void
    lock()
    {
        locked_ = true;
    }
    void
    unlock()
    {
        locked_ = false;
    }
    bool
    islocked()
    {
        return locked_;
    }
    void
    touch()
    {
        last_access_ = stopwatch().now();
    }
    bool 
    expired()
    {
        clock_type::duration maxAge(
            std::chrono::seconds{CONNECTION_TIMEOUT});
        auto whenExpire = last_access_ + maxAge;
        return stopwatch().now() > whenExpire;
    }

    std::shared_ptr<TxStore> store_;
    std::shared_ptr<TxStoreDBConn> conn_;
    clock_type::time_point last_access_;
    bool locked_;
};

class ConnectionPool
{
public:
public:
    ConnectionPool(Schema& app) : app_(app)
    {
    }

    std::shared_ptr<ConnectionUnit>
    getAvailable()
    {
        std::lock_guard lock(mtx_);
        for (auto unit : vecPool_)
        {
            if (!unit->islocked())
            {
                unit->lock();
                unit->touch();
                return unit;
            }  
        }
        auto unit = std::make_shared<ConnectionUnit>(app_);
        unit->lock();
        if (vecPool_.size() < MAX_CONNECTION_IN_POOL)
        {
            vecPool_.push_back(unit);
        }
        return unit;
    }

    void
    sweep()
    {
        std::lock_guard lock(mtx_);
        auto it = vecPool_.begin();
        while ( it != vecPool_.end())
        {
            auto& unit = *it;
            if (!unit->islocked() && unit->expired())
            {
                it = vecPool_.erase(it);
            }
            else
            {
                it++;
            }
        }
    }
    private:
    std::vector<std::shared_ptr<ConnectionUnit>> vecPool_;
    std::mutex mtx_;
    Schema& app_;
};
}
#endif