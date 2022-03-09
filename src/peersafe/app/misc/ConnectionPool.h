#ifndef PEERSAFE_CONNECTION_POOL_H_INCLUDED
#define PEERSAFE_CONNECTION_POOL_H_INCLUDED

#include <memory>
#include <peersafe/app/sql/TxStore.h>
#include <peersafe/schema/Schema.h>
#include <ripple/basics/chrono.h>
#include <peersafe/core/Tuning.h>

namespace ripple {

class ConnectionPool;

struct ConnectionUnit
{
public:
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
    
    std::shared_ptr<TxStore> store_;
    std::shared_ptr<TxStoreDBConn> conn_;
    
private:
    friend class ConnectionPool;
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

    clock_type::time_point last_access_;
    bool locked_;
};

class ConnectionPool
{
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
        if (count () < MAX_CONNECTION_IN_POOL)
        {
            vecPool_.push_back(unit);
        }
        return unit;
    }
    
    void
    releaseConnection(const std::shared_ptr<ConnectionUnit>& conn) {
        if (conn && conn->islocked()) {
            conn->unlock();
        }
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
    int
    count()
    {
        return vecPool_.size();
    }

private:
    std::vector<std::shared_ptr<ConnectionUnit>> vecPool_;
    std::mutex mtx_;
    Schema& app_;
};
}
#endif
