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

#ifndef RIPPLE_APP_MISC_JSON2SQL_H_INCLUDED
#define RIPPLE_APP_MISC_JSON2SQL_H_INCLUDED

#include <memory>
#include <string>
#include <utility>

#include <ripple/json/json_value.h>
#include <ripple/json/Object.h>
#include <ripple/json/json_writer.h>
#include <ripple/protocol/STTx.h>
#include <ripple/core/DatabaseCon.h>

namespace ripple {

class BuildSQL;
class DatabaseCon;

class STTx2SQL {
public:
	STTx2SQL(const std::string& db_type);
	STTx2SQL(const std::string& db_type, DatabaseCon* dbconn);
	~STTx2SQL();

    static bool IsTableExistBySelect(DatabaseCon* dbconn, std::string sTable);

	static bool ConvertCondition2SQL(const Json::Value& condition, std::string& sql);

	using MapRule = std::map<std::string, Json::Value>;
	// convert json into sql
	std::pair<int /*retcode*/, std::string /*sql*/> ExecuteSQL(const ripple::STTx& tx, 
		const std::string& operationRule = "",
		bool verifyAffectedRows = false);

private:
	STTx2SQL() {};
	int GenerateCreateTableSql(const Json::Value& raw, BuildSQL *buildsql);
	//int GenerateRenameTableSql(const Json::Value& tx_json, std::string& sql);
	int GenerateInsertSql(const Json::Value& raw, BuildSQL *buildsql);
	//int GenerateUpdateSql(const Json::Value& raw, BuildSQL *buildsql);
	int GenerateDeleteSql(const Json::Value& raw, BuildSQL *buildsql);
	int GenerateSelectSql(const Json::Value& raw, BuildSQL *buildsql);

	std::pair<bool, std::string> handle_assert_statement(const Json::Value& raw, BuildSQL *buildsql);
	bool assert_result(const soci::rowset<soci::row>& records, const Json::Value& expect);

	bool check_raw(const Json::Value& raw, const uint16_t optype);
	std::pair<bool, std::string> check_optionalRule(const std::string& optionalRule);

	std::string db_type_;
	DatabaseCon* db_conn_;
}; // STTx2SQL

}
#endif // RIPPLE_APP_MISC_JSON2SQL_H_INCLUDED
