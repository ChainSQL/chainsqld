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

#ifndef RIPPLE_APP_MISC_TXSTORE_H_INCLUDED
#define RIPPLE_APP_MISC_TXSTORE_H_INCLUDED

#include <memory>
#include <string>
#include <utility>

#include <ripple/app/tx/impl/ApplyContext.h>
#include <ripple/core/DatabaseCon.h>
#include <ripple/rpc/Context.h>
#include <ripple/json/json_value.h>
#include <ripple/basics/Log.h>

namespace ripple {

class TxStoreDBConn {
public:
	TxStoreDBConn(const Config& cfg);
	~TxStoreDBConn();

	DatabaseCon* GetDBConn() {
		if (databasecon_)
			return databasecon_.get();
		return nullptr;
	}

private:
	std::shared_ptr<DatabaseCon> databasecon_;
};

class TxStoreTransaction {
public:
	TxStoreTransaction(TxStoreDBConn* storeDBConn);
	~TxStoreTransaction();

	soci::transaction* GetTransaction() {
		if (tr_)
			return tr_.get();
		return nullptr;
	}

	void commit() {
		tr_->commit();
	}

    void rollback() {
        tr_->rollback();
    }

private:
	std::shared_ptr<soci::transaction> tr_;
};

class TxStore {
public:
	//TxStore(const Config& cfg);
	TxStore(DatabaseCon* dbconn, const Config& cfg, const beast::Journal& journal);
	~TxStore();

	// dispose one transaction
	std::pair<bool,std::string> Dispose(const STTx& tx,const std::string& operationRule = "", bool verifyAffectedRows = false);
	std::pair<bool, std::string> DropTable(const std::string& tablename);

	Json::Value txHistory(RPC::Context& context);
    Json::Value txHistory(Json::Value& tx_json);
    Json::Value txHistory(std::string sql);
	std::pair<std::vector<std::vector<Json::Value>>, std::string> txHistory2d(RPC::Context& context);
	std::pair<std::vector<std::vector<Json::Value>>, std::string> txHistory2d(Json::Value& tx_json);

	DatabaseCon* getDatabaseCon();
private:
	const Config& cfg_;
	std::string db_type_;
	DatabaseCon* databasecon_;
	beast::Journal journal_;
};	// class TxStore

}	// namespace ripple
#endif // RIPPLE_APP_MISC_TXSTORE_H_INCLUDED
