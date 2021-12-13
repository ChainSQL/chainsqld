//------------------------------------------------------------------------------
/*
 This file is part of chainsqld: https://github.com/chainsql/chainsqld
 Copyright (c) 2016-2018 Peersafe Technology Co., Ltd.

        chainsqld is free software: you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation, either version 3 of the License, or
        (at your option) any later version.

        chainsqld is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.
        You should have received a copy of the GNU General Public License
        along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
 */
//==============================================================================

#ifndef RIPPLE_APP_MISC_TXNDBCONN_H_INCLUDED
#define RIPPLE_APP_MISC_TXNDBCONN_H_INCLUDED

#include <ripple/core/DatabaseCon.h>


namespace ripple {

class TxnDBCon
{
public:
    template <std::size_t N, std::size_t M>
    TxnDBCon(
        DatabaseCon::Setup const& setup,
        std::string const& dbName,
        std::array<char const*, N> const& pragma,
        std::array<char const*, M> const& initSQL,
        DatabaseCon::CheckpointerSetup const& checkpointerSetup)
    {
        conn_read_ = std::make_unique<DatabaseCon>(setup, dbName, pragma, initSQL, checkpointerSetup);
        conn_write_ = std::make_unique<DatabaseCon>(setup, dbName, pragma, initSQL, checkpointerSetup);
    }

    soci::session&
    getSession()
    {
        return conn_write_->getSession();
    }

    LockedSociSession
    checkoutDb()
    {
        return std::move(conn_write_->checkoutDb());
    }

    LockedSociSession
    checkoutDbRead()
    {
        return std::move(conn_read_->checkoutDb());
    }

    void
    setHasTxResult(bool bSet)
    {
        hasTxResult_ = bSet;
    }

    bool
    hasTxResult()
    {
        return hasTxResult_;
    }

    DatabaseCon&
    connRead()
    {
        return *conn_read_;
    }

    DatabaseCon&
    connWrite()
    {
        return *conn_write_;
    }

private:
    std::unique_ptr<DatabaseCon> conn_read_;
    std::unique_ptr<DatabaseCon> conn_write_;
    bool hasTxResult_ = false;
};
}  // namespace ripple
#endif