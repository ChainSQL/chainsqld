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

#include <peersafe/app/table/TableStatusDBSQLite.h>
#include <ripple/basics/ToString.h>
#include <boost/format.hpp>


namespace ripple {

    TableStatusDBSQLite::TableStatusDBSQLite(DatabaseCon* dbconn, Application * app, beast::Journal& journal) : TableStatusDB(dbconn,app,journal)
    {
    }

    TableStatusDBSQLite::~TableStatusDBSQLite() {

    }

    bool TableStatusDBSQLite::CreateSnycDB(DatabaseCon::Setup setup)
    {   
        bool ret = false;
        try {
            LockedSociSession sql_session = databasecon_->checkoutDb();            

            std::string sql(
                "CREATE TABLE IF NOT EXISTS SyncTableState (\
                Owner               CHARACTER(64) ,  \
                TableName           CHARACTER(64),   \
                TableNameInDB       CHARACTER(64),   \
                TxnLedgerHash       CHARACTER(64),   \
                TxnLedgerSeq        CHARACTER(64),   \
                LedgerHash          CHARACTER(64),   \
                LedgerSeq           CHARACTER(64),   \
                TxnUpdateHash       CHARACTER(64),   \
                deleted             CHARACTER(64),   \
                AutoSync            CHARACTER(64),   \
		        TxnLedgerTime		CHARACTER(64),   \
                PreviousCommit      CHARACTER(64),   \
				ChainId				CHARACTER(64),  \
                primary key  (Owner,TableNameInDB))");

            *sql_session << sql;

            //check the table whether exist
            boost::optional<std::string> pTableName;
            sql = "SELECT COUNT(*) FROM sqlite_master where type = 'table' and name = 'SyncTableState'";
            soci::statement st = (sql_session->prepare << sql, soci::into(pTableName));

            ret = st.execute(true);
        }
        catch (std::exception const& e)
        {
            JLOG(journal_.error()) <<
                "InitFromDB exception" << e.what();
        }

        return ret;
    }

    bool TableStatusDBSQLite::isNameInDBExist(std::string TableName, std::string Owner, bool delCheck,std::string &TableNameInDB)
    {
        bool ret = false;
        try
        {
            auto db(databasecon_->checkoutDb());

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

            soci::statement st = (db->prepare << sql
                , soci::into(tableNameInDB_));

            if (tableNameInDB_ && !tableNameInDB_.value().empty())
            {
                ret = true;
                TableNameInDB = tableNameInDB_.value();
            }
        }
        catch (std::exception const& e)
        {
            JLOG(journal_.error()) <<
                "isNameInDBExist exception" << e.what();
        }

        return ret;
    }

    bool TableStatusDBSQLite::InitDB(DatabaseCon::Setup setup)
    {
        return CreateSnycDB(setup);
    }

    bool TableStatusDBSQLite::ReadSyncDB(std::string nameInDB, LedgerIndex &txnseq,
        uint256 &txnhash, LedgerIndex &seq, uint256 &hash, uint256 &txnupdatehash)
    {
        bool ret = false;

        try {
            auto db(databasecon_->checkoutDb());            

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
            boost::optional<std::string> TxnUpdateHash;
            boost::optional<std::string> status;

            soci::statement st = (db->prepare << sql
                , soci::into(TxnLedgerHash)
                , soci::into(TxnLedgerSeq)
                , soci::into(LedgerHash)
                , soci::into(LedgerSeq)
                , soci::into(TxnUpdateHash));

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
                if (TxnUpdateHash && !TxnUpdateHash.value().empty())
                    txnupdatehash = from_hex_text<uint256>(TxnUpdateHash.value());
                ret = true;
            }
        }
        catch (std::exception const& e)
        {
            JLOG(journal_.error()) <<
                "InitFromDB exception" << e.what();
        }

        return ret;
    }

    bool TableStatusDBSQLite::GetMaxTxnInfo(std::string TableName, std::string Owner, LedgerIndex &TxnLedgerSeq, uint256 &TxnLedgerHash)
    {
        bool ret = false;
        try
        {
            auto db(databasecon_->checkoutDb());            

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
            soci::statement st =
                (db->prepare << sql,
                    soci::into(txnLedgerHash_),
                    soci::into(txnLedgerSeq_));

            st.execute();

            if (st.fetch())
            {
                if (txnLedgerSeq_ != boost::none && !txnLedgerSeq_.value().empty())
                {
                    TxnLedgerSeq = std::stoi(txnLedgerSeq_.value());
                    TxnLedgerHash = from_hex_text<uint256>(txnLedgerHash_.value());
                }  
                ret = true;
            }
            else
                ret = false;
        }
        catch (std::exception const& e)
        {
            JLOG(journal_.error()) <<
                "GetMaxTxnInfo exception" << e.what();
        }
        return ret;
    }

    bool TableStatusDBSQLite::InsertSnycDB(std::string TableName, std::string TableNameInDB, std::string Owner, LedgerIndex LedgerSeq,uint256 LedgerHash, bool IsAutoSync,std::string TxnLedgerTime, uint256 chainId)
    {
        bool ret = false;
        try
        {
            auto db(databasecon_->checkoutDb());            

            std::string sql(
                "INSERT OR REPLACE INTO SyncTableState "
                "(Owner, TableName,TableNameInDB,LedgerSeq,LedgerHash,deleted,AutoSync,TxnLedgerTime,ChainID) VALUES('");

            sql += Owner;
            sql += "','";
            sql += TableName;
            sql += "','";
            sql += TableNameInDB;
            sql += "','";
            sql += std::to_string(LedgerSeq);
            sql += "','";
            sql += to_string(LedgerHash);
            sql += "','";
            sql += std::to_string(0);
            sql += "','";
            sql += std::to_string(IsAutoSync ? 1 : 0);
            sql += "','";
			sql += TxnLedgerTime;
			sql += "','";
			sql += to_string(chainId);
            sql += "');";
            *db << sql;
            
            ret = true;
        }
        catch (std::exception const& e)
        {
            JLOG(journal_.error()) <<
                "InsertSnycDB exception" << e.what();
        }

        return ret;
    }

    bool TableStatusDBSQLite::GetAutoListFromDB(uint256 chainId, std::list<std::tuple<std::string, std::string, std::string, bool> > &list)
    {
        bool ret = false;
        try
        {
            auto db(databasecon_->checkoutDb());            

			static std::string  sql = boost::str(boost::format(
				(R"(select Owner,TableName,TxnLedgerTime from SyncTableState
            WHERE ChainId = '%s' AND AutoSync = '1' AND deleted = '0' ORDER BY TxnLedgerSeq DESC;)"))
				% to_string(chainId));

            boost::optional<std::string> Owner_;
            boost::optional<std::string> TableName_;
            boost::optional<std::string> TxnLedgerTime_;

            soci::statement st =
                (db->prepare << sql,
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

                    std::tuple<std::string, std::string, std::string, bool>tp = make_tuple(owner, tablename, time, isAutoSync);
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

    bool TableStatusDBSQLite::UpdateStateDB(const std::string & owner, const std::string & tablename, const bool &isAutoSync)
    {
		bool ret = false;
		try {
			LockedSociSession sql_session = databasecon_->checkoutDb();
			std::string sql = boost::str(boost::format(
				(R"(UPDATE SyncTableState SET AutoSync = '%s'
                WHERE Owner = '%s' AND TableName = '%s';)"))
				% to_string(isAutoSync)
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
    bool TableStatusDBSQLite::RenameRecord(AccountID accountID, std::string TableNameInDB, std::string TableName)
    {
        bool ret = false;
      
        std::string Owner = to_string(accountID);
        try
        {
            auto db(databasecon_->checkoutDb());            

            static std::string updateVal(
                R"sql(UPDATE SyncTableState SET TableName = :TableName
                WHERE Owner = :Owner AND TableNameInDB = :TableNameInDB;)sql");

            *db << updateVal,
                soci::use(TableName),
                soci::use(Owner),
                soci::use(TableNameInDB);
                        
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

	soci_ret TableStatusDBSQLite::UpdateSyncDB(AccountID accountID, std::string TableName, std::string TableNameInDB)
    {
		soci_ret ret = soci_success;
        try {
            std::string Owner = to_string(accountID);
            auto db(databasecon_->checkoutDb());            

            static std::string updateVal(
                R"sql(UPDATE SyncTableState SET TableNameInDB = :TableNameInDB
                WHERE Owner = :Owner AND TableName = :TableName;)sql");

            *db << updateVal,
                soci::use(TableNameInDB),
                soci::use(Owner),
                soci::use(TableName);
                       
            ret = soci_success;
        }
        catch (std::exception const& e)
        {
            JLOG(journal_.error()) <<
                "UpdateSyncDBImpl exception" << e.what();
			ret = soci_exception;
        }

        return ret;
    }

    bool TableStatusDBSQLite::DeleteRecord(AccountID accountID, std::string TableName)
    {
        bool ret = false;
      
        std::string Owner = to_string(accountID);
        try
        {
            auto db(databasecon_->checkoutDb());            

            static std::string deleteVal(
                R"sql(DELETE FROM  SyncTableState
                WHERE Owner = :Owner AND TableName = :TableName;)sql");

            *db << deleteVal,
                soci::use(Owner),
                soci::use(TableName);

            ret = true;

        }
        catch (std::exception const& e)
        {
            JLOG(journal_.error()) <<
                "DeleteRecord exception" << e.what();
        }

        return ret;
    }

    bool TableStatusDBSQLite::IsExist(AccountID accountID, std::string TableNameInDB)
    {
        bool ret = false;
       
        std::string Owner = to_string(accountID);
        try
        {
            auto db(databasecon_->checkoutDb());            

            static std::string const prefix(
                R"(SELECT LedgerSeq from SyncTableState
            WHERE )");

            std::string sql = boost::str(boost::format(
                prefix +
                (R"(TableNameInDB = '%s' AND Owner = '%s';)"))
                % TableNameInDB
                % Owner);

            boost::optional<std::string> LedgerSeq;

            soci::statement st = (db->prepare << sql
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

	soci_ret TableStatusDBSQLite::UpdateSyncDB(const std::string &Owner, const std::string &TableNameInDB, const std::string &TxnLedgerHash,
        const std::string &TxnLedgerSeq, const std::string &LedgerHash, const std::string &LedgerSeq, const std::string &TxnUpdateHash,
        const std::string &TxnLedgerTime, const std::string &PreviousCommit)
    {
		soci_ret ret = soci_success;

       try
        {
            auto db(databasecon_->checkoutDb());
            
            static std::string updateVal(
                R"sql(UPDATE SyncTableState SET TxnLedgerHash = :TxnLedgerHash, TxnLedgerSeq = :TxnLedgerSeq,LedgerHash = :LedgerHash, LedgerSeq = :LedgerSeq,TxnUpdateHash = :TxnUpdateHash,TxnLedgerTime = :TxnLedgerTime,PreviousCommit = :PreviousCommit
                WHERE Owner = :Owner AND TableNameInDB = :TableNameInDB;)sql");

            *db << updateVal,
                soci::use(TxnLedgerHash),
                soci::use(TxnLedgerSeq),
                soci::use(LedgerHash),
                soci::use(LedgerSeq),
                soci::use(TxnUpdateHash), 
                soci::use(TxnLedgerTime),
                soci::use(PreviousCommit),
                soci::use(Owner),
                soci::use(TableNameInDB);
                        
        }
        catch (std::exception const& e)
        {
            JLOG(journal_.error()) <<
                "UpdateSyncDB exception" << e.what();
			ret = soci_exception;
        }
        
        return ret;
    }

	soci_ret TableStatusDBSQLite::UpdateSyncDB(const std::string &Owner, const std::string &TableNameInDB, const std::string &LedgerHash,
        const std::string &LedgerSeq, const std::string &PreviousCommit)
    {
		soci_ret ret = soci_success;
        
        try
        {
            auto db(databasecon_->checkoutDb());
            
            static std::string updateVal(
                R"sql(UPDATE SyncTableState SET LedgerHash = :LedgerHash, LedgerSeq = :LedgerSeq,PreviousCommit = :PreviousCommit
                WHERE Owner = :Owner AND TableNameInDB = :TableNameInDB;)sql");

            *db << updateVal,
                soci::use(LedgerHash),
                soci::use(LedgerSeq),
                soci::use(PreviousCommit),
                soci::use(Owner),
                soci::use(TableNameInDB);
        }
        catch (std::exception const& e)
        {
            JLOG(journal_.error()) <<
                "UpdateSyncDB exception" << e.what();
        }

        return ret;
    }

	soci_ret TableStatusDBSQLite::UpdateSyncDB(const std::string &Owner, const std::string &TableNameInDB,
        const std::string &TxnUpdateHash, const std::string &PreviousCommit)
    {
		soci_ret ret = soci_success;
  
        try {
            auto db(databasecon_->checkoutDb());            

            static std::string updateVal(
                R"sql(UPDATE SyncTableState SET TxnUpdateHash = :TxnUpdateHash,PreviousCommit = :PreviousCommit
        WHERE Owner = :Owner AND TableNameInDB = :TableNameInDB;)sql");

            *db << updateVal,
                soci::use(TxnUpdateHash),
				soci::use(PreviousCommit),
                soci::use(Owner),
                soci::use(TableNameInDB);
                       
        }
        catch (std::exception const& e)
        {
            JLOG(journal_.error()) <<
                "UpdateSyncDB exception" << e.what();
			ret = soci_exception;
        }

        return ret;
    }

	soci_ret TableStatusDBSQLite::UpdateSyncDB(const std::string &Owner, const std::string &TableNameInDB,
        bool bDel, const std::string &PreviousCommit)
    {
        
		soci_ret ret = soci_success;
        
        try {
            auto db(databasecon_->checkoutDb());            

            static std::string updateVal(
                R"sql(UPDATE SyncTableState SET deleted = :deleted,PreviousCommit = :PreviousCommit
        WHERE Owner = :Owner AND TableNameInDB = :TableNameInDB;)sql");

            *db << updateVal,
                soci::use(std::to_string(bDel)),
				soci::use(PreviousCommit),
                soci::use(Owner),
                soci::use(TableNameInDB);
        }
        catch (std::exception const& e)
        {
            JLOG(journal_.error()) <<
                "UpdateSyncDB exception" << e.what();
			ret = soci_exception;
        }
        
        return ret;
    }

}
