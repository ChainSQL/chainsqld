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

#ifndef RIPPLE_APP_TABLE_TABLESTATUSDBMYSQL_H_INCLUDED
#define RIPPLE_APP_TABLE_TABLESTATUSDBMYSQL_H_INCLUDED

#include <peersafe/app/table/TableStatusDB.h>

namespace ripple {

class TableStatusDBMySQL : public TableStatusDB
{
public:
    TableStatusDBMySQL(
        DatabaseCon* dbconn,
        Schema* app,
        beast::Journal& journal);
    ~TableStatusDBMySQL();

    bool
    InitDB(DatabaseCon::Setup setup) override;

    bool
    ReadSyncDB(
        std::string nameInDB,
        LedgerIndex& txnseq,
        uint256& txnhash,
        LedgerIndex& seq,
        uint256& hash,
        uint256& txnupdatehash) override;

    bool
    GetMaxTxnInfo(
        std::string TableName,
        std::string Owner,
        LedgerIndex& TxnLedgerSeq,
        uint256& TxnLedgerHash) override;

    bool
    InsertSnycDB(
        std::string TableName,
        std::string TableNameInDB,
        std::string Owner,
        LedgerIndex LedgerSeq,
        uint256 LedgerHash,
        bool IsAutoSync,
        std::string TxnLedgerTime,
        uint256 chainId) override;

    bool
    CreateSnycDB(DatabaseCon::Setup setup) override;

    bool
    isNameInDBExist(
        std::string TableName,
        std::string Owner,
        bool delCheck,
        std::string& TableNameInDB) override;

    bool
    RenameRecord(
        AccountID accountID,
        std::string TableNameInDB,
        std::string TableName) override;

    soci_ret
    UpdateSyncDB(
        AccountID accountID,
        std::string TableName,
        std::string TableNameInDB) override;

    bool
    DeleteRecord(AccountID accountID, std::string TableName) override;

    bool
    IsExist(AccountID accountID, std::string TableNameInDB) override;

    soci_ret
    UpdateSyncDB(
        const std::string& Owner,
        const std::string& TableNameInDB,
        const std::string& TxnLedgerHash,
        const std::string& TxnLedgerSeq,
        const std::string& LedgerHash,
        const std::string& LedgerSeq,
        const std::string& TxnUpdateHash,
        const std::string& TxnLedgerTime,
        const std::string& PreviousCommit) override;

    soci_ret
    UpdateSyncDB(
        const std::string& Owner,
        const std::string& TableNameInDB,
        const std::string& LedgerHash,
        const std::string& LedgerSeq,
        const std::string& PreviousCommit) override;

    soci_ret
    UpdateSyncDB(
        const std::string& Owner,
        const std::string& TableNameInDB,
        const std::string& TxnUpdateHash,
        const std::string& PreviousCommit) override;

    soci_ret
    UpdateSyncDB(
        const std::string& Owner,
        const std::string& TableNameInDB,
        bool bDel,
        const std::string& PreviousCommit) override;

    bool
    GetAutoListFromDB(
        uint256 chainId,
        std::list<std::tuple<std::string, std::string, std::string, bool>>&
            tuple) override;

    bool
    UpdateStateDB(
        const std::string& owner,
        const std::string& tablename,
        const bool& isAutoSync) override;

    DatabaseCon*  GetDatabaseConn() override;
};

}
#endif
