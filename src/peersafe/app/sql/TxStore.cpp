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

#include <vector>
#include <string>

#include <peersafe/app/sql/TxStore.h>
#include <peersafe/app/sql/STTx2SQL.h>
#include <ripple/json/Output.h>
#include <ripple/protocol/JsonFields.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/net/RPCErr.h>
#include <ripple/rpc/impl/RPCHelpers.h>
#include <boost/algorithm/string.hpp>

namespace ripple {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// class TxStoreDBConn
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TxStoreDBConn::TxStoreDBConn(const Config& cfg)
: databasecon_(nullptr) {
	DatabaseCon::Setup setup = ripple::setup_SyncDatabaseCon(cfg);
	std::pair<std::string, bool> result_type = setup.sync_db.find("type");
	std::string database_name, dbType; 
    std::pair<std::string, bool> database = setup.sync_db.find("db");
 
	if (result_type.second == false || result_type.first.empty()) 
	{

	}
	else if (result_type.first.compare("sqlite")==0) {
		std::pair<std::string, bool> database = setup.sync_db.find("db");
		database_name = "chainsql";
		if (database.second)
			database_name = database.first;
        if (result_type.first.compare("sqlite") == 0)
            database_name += ".db";
        dbType = "sqlite";
	}
    else
    {
        if (database.second)
            database_name = database.first;
        dbType = "mycat";
    }
    int count = 0;

    while (count < 3)
    {
        try
        {   
            count++;
            databasecon_ = std::make_shared<DatabaseCon>(setup, database_name, nullptr, 0, dbType);
        }
        catch (...)
        {
            databasecon_ = NULL;
        }
        if (databasecon_)
            break;
    }
}

TxStoreDBConn::~TxStoreDBConn() {
	if (databasecon_)
		databasecon_.reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// class TxStoreTransaction
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TxStoreTransaction::TxStoreTransaction(TxStoreDBConn* storeDBConn)
    : tr_(std::make_shared<soci::transaction>(storeDBConn->GetDBConn()->getSession()))
{

}

TxStoreTransaction::~TxStoreTransaction() {
	if (tr_)
		tr_.reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// class TxStore
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//TxStore::TxStore(const Config& cfg)
//: cfg_(cfg)
//, databasecon_(nullptr) {
//	DatabaseCon::Setup setup = ripple::setup_SyncDatabaseCon(cfg_);
//	std::pair<std::string, bool> result_type = setup.sync_db.find("type");
//	std::string database_name;
//	if (result_type.second == false || result_type.first.empty() || result_type.first.compare("sqlite") == 0) {
//		std::pair<std::string, bool> database = setup.sync_db.find("db");
//		database_name = "ripple";
//		if (database.second)
//			database_name = database.first;
//	}
//	databasecon_ = std::make_shared<DatabaseCon>(setup, database_name, nullptr, 0);
//}

TxStore::TxStore(DatabaseCon* dbconn, const Config& cfg, const beast::Journal& journal)
: cfg_(cfg)
, db_type_()
, databasecon_(dbconn)
, journal_(journal) {
	const ripple::Section& sync_db = cfg_.section("sync_db");
	std::pair<std::string, bool> result = sync_db.find("type");
	if (result.second)
		db_type_ = result.first;
}

TxStore::~TxStore() {
	
}

std::pair<bool, std::string> TxStore::Dispose(const STTx& tx, const std::string& sOperationRule /* = "" */, bool bVerifyAffectedRows /* = false */) {
	std::pair<bool, std::string> ret = {false, "inner error"};
	do {
		if (databasecon_ == nullptr) {
			ret = { false, "database occupy error" };
			break;
		}

		// filter type
		if (tx.getTxnType() != ttTABLELISTSET
			&& tx.getTxnType() != ttSQLSTATEMENT 
            && tx.getTxnType() != ttSQLTRANSACTION) {
			ret = { false, "tx's type is unexcepted" };
			break;
		}

		STTx2SQL tx2sql(db_type_, databasecon_);
		std::pair<int, std::string> result = tx2sql.ExecuteSQL(tx, sOperationRule, bVerifyAffectedRows);
		if (result.first != 0) {
			std::string errmsg = std::string("Execute failure." + result.second);
			ret = { false,  errmsg};
			JLOG(journal_.error()) << errmsg;
			break;
		} else {
			JLOG(journal_.info()) << "Execute sucess." + result.second;
            ret = { true, "success" };
		}
	} while (0);
	return ret;
}

std::pair<bool, std::string> TxStore::DropTable(const std::string& tablename) {
	std::pair<bool, std::string> result = { false, "inner error" };
	if (databasecon_ == nullptr) {
		result = {false, "Can't connect db."};
		return result;
	}
	std::string sql_str = std::string("drop table if exists t_") + tablename;
	soci::session& sql = *databasecon_->checkoutDb();
	sql << sql_str;
	return {true, "success"};
}

}	// namespace ripple
