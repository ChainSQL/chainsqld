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

#include <peersafe/app/table/TableStatusDBMySQL.h>
#include <ripple/basics/ToString.h>
#include <boost/format.hpp>

namespace ripple {

    TableStatusDBMySQL::TableStatusDBMySQL(DatabaseCon* dbconn, Application * app, beast::Journal& journal) : TableStatusDB(dbconn,app,journal)
    {
    }

    TableStatusDBMySQL::~TableStatusDBMySQL() {

    }

    bool TableStatusDBMySQL::InitDB(DatabaseCon::Setup setup)
    {   
        return CreateSnycDB(setup);
    }

    bool TableStatusDBMySQL::ReadSyncDB(std::string nameInDB, LedgerIndex &txnseq,
        uint256 &txnhash, LedgerIndex &seq, uint256 &hash, uint256 &txnupdatehash)
    {
        bool ret = false;
        try
        {
            LockedSociSession sql_session = databasecon_->checkoutDb();

            static std::string const prefix(
                R"(select TxnLedgerHash,TxnLedgerSeq,LedgerHash,LedgerSeq,TxnUpdateHash from SyncTableState
            WHERE )");

            std::string sql = boost::str(boost::format(
                prefix +
                (R"(TableNameInDB = '%s';)"))
                % nameInDB);

            boost::optional<std::string> TxnLedgerSeq;
            boost::optional<std::string> TxnLedgerHash;
            boost::optional<std::string> LedgerSeq;
            boost::optional<std::string> LedgerHash;
            boost::optional<std::string> TxnUpdatehash;
            boost::optional<std::string> status;            

            soci::statement st = (sql_session->prepare << sql
                , soci::into(TxnLedgerHash)
                , soci::into(TxnLedgerSeq)
                , soci::into(LedgerHash)
                , soci::into(LedgerSeq)
                , soci::into(TxnUpdatehash));

            bool dbret = st.execute(true);

            if (dbret)
            {
                if (TxnLedgerSeq != boost::none && !TxnLedgerSeq.value().empty())
                    txnseq = std::stoi(TxnLedgerSeq.value());
                if (TxnLedgerHash && !TxnLedgerHash.value().empty())
                    txnhash = from_hex_text<uint256>(TxnLedgerHash.value());
                if (LedgerSeq && !LedgerSeq.value().empty())
                    seq = std::stoi(LedgerSeq.value());
                if (LedgerHash && !LedgerHash.value().empty())
                    hash = from_hex_text<uint256>(LedgerHash.value());
                if (TxnUpdatehash && !TxnUpdatehash.value().empty())
                    txnupdatehash = from_hex_text<uint256>(TxnUpdatehash.value());
                ret = true;
            }
        }
        catch (std::exception const& e)
        {
            JLOG(journal_.error()) <<
            "ReadSyncDB exception" << e.what();
        }

        return ret;
    }

    bool TableStatusDBMySQL::GetMaxTxnInfo(std::string TableName, std::string Owner, LedgerIndex &TxnLedgerSeq, uint256 &TxnLedgerHash)
    {
        bool ret = false;
        try
        {
            LockedSociSession sql_session = databasecon_->checkoutDb();

            static std::string const prefix(
                R"(select TxnLedgerHash,TxnLedgerSeq from SyncTableState
            WHERE )");

            std::string sql = boost::str(boost::format(
                prefix +
                (R"(TableName = '%s' AND Owner = '%s' ORDER BY TxnLedgerSeq DESC;)"))
                % TableName
                % Owner);

            boost::optional<std::string> txnLedgerHash_;
            boost::optional<std::string> txnLedgerSeq_;

            soci::statement st = (sql_session->prepare << sql,
                    soci::into(txnLedgerHash_),
                    soci::into(txnLedgerSeq_));

            st.execute();

            while (st.fetch())
            {
                if (txnLedgerSeq_ != boost::none && !txnLedgerSeq_.value().empty())
                {
                    TxnLedgerSeq = std::stoi(txnLedgerSeq_.value());
                    TxnLedgerHash = from_hex_text<uint256>(txnLedgerHash_.value());
                }
                ret = true;
            }
        }
        catch (std::exception const& e)
        {
            JLOG(journal_.error()) <<
                "GetMaxTxnInfo exception" << e.what();
        }
        return ret;
        
    }

    bool TableStatusDBMySQL::InsertSnycDB(std::string TableName, std::string TableNameInDB, std::string Owner, LedgerIndex LedgerSeq, uint256 LedgerHash,bool IsAutoSync,std::string TxnLedgerTime, uint256 chainId)
    {
        bool ret = false;
        try
        {
            LockedSociSession sql_session = databasecon_->checkoutDb();

            std::string sql(
                "INSERT INTO SyncTableState "
                "(Owner, TableName, TableNameInDB,LedgerSeq,LedgerHash,deleted,AutoSync,TxnLedgerTime,ChainID) VALUES('");

            sql += Owner;
            sql += "','";
            sql += TableName;
            sql += "','";
            sql += TableNameInDB;
            sql += "','";
            sql += to_string(LedgerSeq);
            sql += "','";
            sql += to_string(LedgerHash);
            sql += "','";
            sql += to_string(0);
            sql += "','";
            sql += to_string(IsAutoSync? 1 : 0);
            sql += "','";
            sql += TxnLedgerTime;
			sql += "','";
			sql += to_string(chainId);
            sql += "');";

            soci::statement st = (sql_session->prepare << sql);

            st.execute();
            ret = true;
        }
        catch (std::exception const& e)
        {
            JLOG(journal_.error()) <<
                "InsertSnycDB exception" << e.what();
        }
        return ret;
    }

    bool TableStatusDBMySQL::CreateSnycDB(DatabaseCon::Setup setup)
    {
        bool ret = false;
        
        LockedSociSession sql_session = databasecon_->checkoutDb();

        //1.check the synctable whether exists.
        bool bExist = false;
        try {            
            std::string sql = "SELECT Owner FROM SyncTableState";
            boost::optional<std::string> owner;
            soci::statement st = (sql_session->prepare << sql, soci::into(owner));
            st.execute(true);
            bExist = true;
        }
        catch (std::exception const& e)
        {
            JLOG(journal_.warn()) << "the first query owner form synctablestate exception" << e.what();
            bExist = false;
        }
        if (bExist) return true;

        //2.not exist ,create it
        try {            
            std::string sql(
                "CREATE TABLE SyncTableState(" \
                "Owner             CHARACTER(64) NOT NULL, " \
                "TableName         CHARACTER(64),          " \
                "TableNameInDB     CHARACTER(64) NOT NULL, " \
                "TxnLedgerHash     CHARACTER(64),          " \
                "TxnLedgerSeq      CHARACTER(64),          " \
                "LedgerHash        CHARACTER(64),          " \
                "LedgerSeq         CHARACTER(64),          " \
                "TxnUpdateHash     CHARACTER(64),          " \
                "deleted           CHARACTER(64),          " \
                "AutoSync          CHARACTER(64),          " \
                "TxnLedgerTime     CHARACTER(64),          " \
                "PreviousCommit    CHARACTER(64),          " \
				"ChainId		   CHARACTER(64),		   " \
                "PRIMARY KEY(Owner,TableNameInDB))         "
            );
            soci::statement st = (sql_session->prepare << sql);

            ret = st.execute();
        }
        catch (std::exception const& e)
        {
            JLOG(journal_.error()) << "CreateSnycDB exception" << e.what();
            ret = false;
        }

        //3.check again to guarantee synctable be created successfully
        try {            
            std::string sql = "SELECT Owner FROM SyncTableState";
            boost::optional<std::string> owner;
            soci::statement st = (sql_session->prepare << sql, soci::into(owner));
            st.execute(true);
            ret = true;
        }
        catch (std::exception const& e)
        {
            JLOG(journal_.warn()) << "the second query owner form synctablestate exception" << e.what();
            ret = false;
        }
        
        return ret;
    }
    bool TableStatusDBMySQL::isNameInDBExist(std::string TableName, std::string Owner, bool delCheck, std::string &TableNameInDB)
    {
        bool ret = false;
        try
        {
            LockedSociSession sql_session = databasecon_->checkoutDb();
            static std::string const prefix(
                R"(SELECT TableNameInDB from SyncTableState
            WHERE )");

            std::string sql = boost::str(boost::format(
                prefix +
                (R"(TableName = '%s' AND Owner = '%s')"))
                % TableName
                % Owner);

            if (delCheck)
                sql = sql + " AND deleted = 0;";
            else
                sql = sql + ";";

            boost::optional<std::string> tableNameInDB_;
           
            soci::statement st = (sql_session->prepare << sql
                , soci::into(tableNameInDB_));

            bool dbret = st.execute(true);
            
            if (dbret)
            {
                if (tableNameInDB_ && !tableNameInDB_.value().empty())
                    TableNameInDB = tableNameInDB_.value();

                ret = true;
            }
        }
        catch (std::exception const& e)
        {
            JLOG(journal_.error()) <<
                "isNameInDBExist exception" << e.what();
        }

        return ret;
    }

    bool TableStatusDBMySQL::RenameRecord(AccountID accountID, std::string TableNameInDB, std::string TableName)
    {
        bool ret = false;
        std::string Owner = to_string(accountID);
        try
        {
            LockedSociSession sql_session = databasecon_->checkoutDb();
            std::string sql = boost::str(boost::format(
                (R"(UPDATE SyncTableState SET TableName = '%s'
                WHERE Owner = '%s' AND TableNameInDB = '%s';)"))
                % TableName
                % Owner
                % TableNameInDB);

            soci::statement st =
                (sql_session->prepare << sql);

            st.execute();
            ret = true;
        }
        catch (std::exception const& e)
        {
            JLOG(journal_.error()) <<
            "RenameRecord exception" << e.what();
			ret = false;
        }

        return ret;
    }

	soci_ret TableStatusDBMySQL::UpdateSyncDB(AccountID accountID, std::string TableName, std::string TableNameInDB)
    {
		soci_ret ret = soci_success;
        try {
            std::string Owner = to_string(accountID);

            LockedSociSession sql_session = databasecon_->checkoutDb();

            std::string sql = boost::str(boost::format(
                (R"(UPDATE SyncTableState SET TableNameInDB = '%s'
                WHERE Owner = '%s' AND TableName = '%s';)"))
                % TableNameInDB
                % Owner
                % TableName);

            soci::statement st = sql_session->prepare << sql;

            st.execute(true);
        }
        catch (std::exception const& e)
        {
            JLOG(journal_.error()) <<
            "UpdateSyncDB exception" << e.what();
			ret = soci_exception;
        }

        return ret;
    }

    bool TableStatusDBMySQL::DeleteRecord(AccountID accountID, std::string TableName)
    {
        bool ret = false;
        std::string Owner = to_string(accountID);
        try
        {
            LockedSociSession sql_session = databasecon_->checkoutDb();

            std::string deleteVal = boost::str(boost::format(
                (R"(DELETE FROM  SyncTableState
                WHERE Owner = '%s' AND TableName = '%s';)"))
                % Owner
                % TableName);

            soci::statement st = sql_session->prepare << deleteVal;

            bool dbret = st.execute(true);

            if (dbret)
                ret = true;
            ret = true;
        }
        catch (std::exception const& e)
        {
            JLOG(journal_.error()) <<
            "DeleteRecord exception" << e.what();
        }

        return ret;
    }

    bool TableStatusDBMySQL::IsExist(AccountID accountID, std::string TableNameInDB)
    {
        bool ret = false;
        std::string Owner = to_string(accountID);
        try
        {
            LockedSociSession sql_session = databasecon_->checkoutDb();
            static std::string const prefix(
                R"(SELECT LedgerSeq from SyncTableState
            WHERE )");

            std::string sql = boost::str(boost::format(
                prefix +
                (R"(TableNameInDB = '%s' AND Owner = '%s';)"))
                % TableNameInDB
                % Owner);

            boost::optional<std::string> LedgerSeq;

            soci::statement st = (sql_session->prepare << sql
                , soci::into(LedgerSeq));

            bool dbret = st.execute(true);

            if (dbret)//if have records
            {
                ret = true;
            }
        }
        catch (std::exception const& e)
        {
            JLOG(journal_.error()) <<
            "isExist exception" << e.what();
        }
        return ret;
    }

	soci_ret TableStatusDBMySQL::UpdateSyncDB(const std::string &Owner, const std::string &TableNameInDB, const std::string &TxnLedgerHash,
        const std::string &TxnLedgerSeq, const std::string &LedgerHash, const std::string &LedgerSeq, const std::string &TxnUpdateHash,
        const std::string &TxnLedgerTime, const std::string &PreviousCommit)
    {
		soci_ret ret = soci_success;
        try
        {
            LockedSociSession sql_session = databasecon_->checkoutDb();
            std::string sql = boost::str(boost::format(
                (R"(UPDATE SyncTableState SET TxnLedgerHash = :TxnLedgerHash, TxnLedgerSeq = :TxnLedgerSeq,LedgerHash = :LedgerHash, LedgerSeq = :LedgerSeq,TxnUpdateHash = :TxnUpdateHash,TxnLedgerTime = :TxnLedgerTime,PreviousCommit = :PreviousCommit
                WHERE Owner = :Owner AND TableNameInDB = :TableNameInDB;)")));

            soci::statement st = (sql_session->prepare << sql,
                soci::use(TxnLedgerHash),
                soci::use(TxnLedgerSeq),
                soci::use(LedgerHash),
                soci::use(LedgerSeq),
                soci::use(TxnUpdateHash),
                soci::use(TxnLedgerTime),
                soci::use(PreviousCommit),
                soci::use(Owner),
                soci::use(TableNameInDB));

            st.execute(true);
        }
        catch (std::exception const& e)
        {
            JLOG(journal_.error()) <<
            "UpdateSyncDB exception" << e.what();
			ret = soci_exception;
        }
        return ret;
    }

    soci_ret TableStatusDBMySQL::UpdateSyncDB(const std::string &Owner, const std::string &TableNameInDB, const std::string &LedgerHash,
        const std::string &LedgerSeq, const std::string &PreviousCommit)
    {
		soci_ret ret = soci_success;
        try
        {
            LockedSociSession sql_session = databasecon_->checkoutDb();

            std::string sql = boost::str(boost::format(
                (R"(UPDATE SyncTableState SET LedgerHash = :LedgerHash, LedgerSeq = :LedgerSeq,PreviousCommit = :PreviousCommit
                WHERE Owner = :Owner AND TableNameInDB = :TableNameInDB;)")));

            soci::statement st = ((*sql_session).prepare << sql,
                soci::use(LedgerHash),
                soci::use(LedgerSeq),
                soci::use(PreviousCommit),
                soci::use(Owner),
                soci::use(TableNameInDB));

            st.execute(true);

            //if (dbret)//if have records
            //{
            //    ret = soci_success;
            //}
        }
        catch (std::exception const& e)
        {
            JLOG(journal_.error()) <<
            "UpdateSyncDB exception" << e.what();
			ret = soci_exception;
        }
        return ret;
    }

	soci_ret TableStatusDBMySQL::UpdateSyncDB(const std::string &Owner, const std::string &TableNameInDB,
        const std::string &TxnUpdateHash, const std::string &PreviousCommit)
    {
		soci_ret ret = soci_success;
        try {            
            LockedSociSession sql_session = databasecon_->checkoutDb();
            std::string sql = boost::str(boost::format(
                (R"(UPDATE SyncTableState SET TxnUpdateHash = '%s'
                WHERE Owner = '%s' AND TableNameInDB = '%s';)"))
                % TxnUpdateHash
                % Owner
                % TableNameInDB);

            soci::statement st = (*sql_session).prepare << sql;

            bool dbret = st.execute(true);

            if (dbret)//if have records
            {
                ret = soci_success;
            }
        }
        catch (std::exception const& e)
        {
            JLOG(journal_.error()) <<
            "UpdateSyncDB exception" << e.what();
			ret = soci_exception;
        }

        return ret;
    }

	soci_ret TableStatusDBMySQL::UpdateSyncDB(const std::string &Owner, const std::string &TableNameInDB,
        bool bDel, const std::string &PreviousCommit)
    {
		soci_ret ret = soci_success;
        try {
            LockedSociSession sql_session = databasecon_->checkoutDb();

            std::string sql = boost::str(boost::format(
                (R"(UPDATE SyncTableState SET deleted = '%s'
                WHERE Owner = '%s' AND TableNameInDB = '%s';)"))
                % to_string(bDel ? 1:0)
                % Owner
                % TableNameInDB);

            soci::statement st = ((*sql_session).prepare << sql);

            st.execute(true);
        }
        catch (std::exception const& e)
        {
            JLOG(journal_.error()) <<
            "UpdateSyncDB exception" << e.what();
			ret = soci_exception;
        }

        return ret;
    }

    bool TableStatusDBMySQL::GetAutoListFromDB(uint256 chainId, std::list<std::tuple<std::string, std::string, std::string, bool> > &list)
    {
        bool ret = false;
        try
        {
            LockedSociSession sql_session = databasecon_->checkoutDb();

			static std::string  sql = boost::str(boost::format(
				(R"(select Owner,TableName,TxnLedgerTime from SyncTableState
            WHERE ChainId = '%s' AND AutoSync = '1' AND deleted = '0' ORDER BY TxnLedgerSeq DESC;)"))
				% to_string(chainId));

            boost::optional<std::string> Owner_;
            boost::optional<std::string> TableName_;
            boost::optional<std::string> TxnLedgerTime_;

            soci::statement st =
                ((*sql_session).prepare << sql,
                    soci::into(Owner_),
                    soci::into(TableName_),
                    soci::into(TxnLedgerTime_));

            st.execute();

            bool isAutoSync = true;

            while (st.fetch())
            {
                std::string owner;
                std::string tablename;
                std::string time;

                if (Owner_ != boost::none && !Owner_.value().empty())
                {
                    owner = Owner_.value();
                    tablename = TableName_.value();
                    time = TxnLedgerTime_.value();

                    std::tuple<std::string, std::string, std::string, bool>tp = make_tuple(owner, tablename, time,isAutoSync);
                    list.push_back(tp);
                }
                ret = true;
            }

        }
        catch (std::exception const& e)
        {
            JLOG(journal_.error()) <<
                "GetAutoListFromDB exception" << e.what();
        }
        return ret;
    }

    bool TableStatusDBMySQL::UpdateStateDB(const std::string & owner, const std::string & tablename, const bool &isAutoSync)
    {
        bool ret = false;
        try {
            LockedSociSession sql_session = databasecon_->checkoutDb();
            std::string sql = boost::str(boost::format(
                (R"(UPDATE SyncTableState SET AutoSync = '%s'
                WHERE Owner = '%s' AND TableName = '%s';)"))
                % to_string(isAutoSync ? 1 : 0)
                % owner
                % tablename);

            soci::statement st = ((*sql_session).prepare << sql);

            bool dbret = st.execute();

            if (dbret)
                ret = true;
        }
        catch (std::exception const& e)
        {
            JLOG(journal_.error()) <<
                "UpdateSyncDB exception" << e.what();
        }

        return ret;
    }
}
