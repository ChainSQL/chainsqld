//------------------------------------------------------------------------------
/*
This file is part of rippled: https://github.com/ripple/rippled
Copyright (c) 2012, 2013 Ripple Labs Inc.

Permission to use, copy, modify, and/or distribute this software for any
purpose  with  or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include <cstring>
#include <algorithm>
#include <list>
#include <memory>

#include <BeastConfig.h>
#include <ripple/core/Config.h>
#include <ripple/protocol/Sign.h>
#include <ripple/protocol/STTx.h>
#include <ripple/protocol/STParsedJSON.h>
#include <ripple/protocol/types.h>
#include <ripple/json/to_string.h>
#include <ripple/json/json_reader.h>
#include <ripple/json/Output.h>
#include <ripple/core/Config.h>
#include <ripple/resource/Fees.h>
#include <ripple/rpc/impl/RPCHelpers.h>
#include <ripple/basics/base_uint.h>
#include <ripple/basics/Log.h>
#include <ripple/beast/unit_test.h>
#include <peersafe/app/sql/SQLConditionTree.h>
#include <peersafe/app/sql/STTx2SQL.h>
#include <peersafe/app/sql/TxStore.h>
#include <test/jtx.h>

#include <boost/algorithm/string.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/format.hpp>
#include <iostream>
using namespace std;
using boost::multiprecision::cpp_dec_float_50;

namespace ripple {

class SuiteSink : public beast::Journal::Sink
{
	std::string partition_;
	beast::unit_test::suite& suite_;

public:
	SuiteSink(std::string const& partition,
		beast::severities::Severity threshold,
		beast::unit_test::suite& suite)
		: Sink(threshold, false)
		, partition_(partition + " ")
		, suite_(suite)
	{
	}

	// For unit testing, always generate logging text.
	bool active(beast::severities::Severity level) const override
	{
		return true;
	}

	void
		write(beast::severities::Severity level,
			std::string const& text) override
	{
		using namespace beast::severities;
		std::string s;
		switch (level)
		{
		case kTrace:    s = "TRC:"; break;
		case kDebug:    s = "DBG:"; break;
		case kInfo:     s = "INF:"; break;
		case kWarning:  s = "WRN:"; break;
		case kError:    s = "ERR:"; break;
		default:
		case kFatal:    s = "FTL:"; break;
		}

		// Only write the string if the level at least equals the threshold.
		if (level >= threshold())
			suite_.log << s << partition_ << text << std::endl;
	}
};



class SuiteLogs : public Logs
{
	beast::unit_test::suite& suite_;

public:
	explicit
		SuiteLogs(beast::unit_test::suite& suite)
		: Logs(beast::severities::kError)
		, suite_(suite)
	{
	}

	~SuiteLogs() override = default;

	std::unique_ptr<beast::Journal::Sink>
		makeSink(std::string const& partition,
			beast::severities::Severity threshold) override
	{
		return std::make_unique<SuiteSink>(partition, threshold, suite_);
	}
};
std::unique_ptr<ripple::Logs> logs_;

class Transaction2Sql_test : public beast::unit_test::suite {
public:
	Transaction2Sql_test()
	: txstore_dbconn_(nullptr)
	, txstore_(nullptr)
	, config_()
	, table_name_("1c2f5e6095324e2e08838f221a72ab4f") {
		logs_ = std::make_unique<SuiteLogs>(*this);
	}

	~Transaction2Sql_test() {
		txstore_.reset();
	}

	void init_env() {
		std::string args = arg();
		size_t npos = args.find("conf=");
		if (npos == std::string::npos) {
			JLOG(logs_->journal("Transaction2Sql").error()) << "not specify conf";
			exit(-1);
		}
		//std::string strconf("F:\\work\\rippled\\build\\msvc.debug.nounity\\rippled.cfg");
		try {
			std::string strconf = args.substr(npos + 4 + 1);
			config_.setup(strconf, true, false, false);
			txstore_dbconn_ = std::make_shared<TxStoreDBConn>(config_);
			txstore_ = std::make_shared<TxStore>(txstore_dbconn_->GetDBConn(), config_, logs_->journal("TxStore"));
		}
		catch (const soci::soci_error& e) {
			JLOG(logs_->journal("Transaction2Sql").error()) << "init sql failed." << e.what();
			exit(-1);
		}
		catch (...) {
			JLOG(logs_->journal("Transaction2Sql").error()) << "init sql failed. unkown reason";
			exit(-1);
		}
	}

	void test_fixbug_RR207() {
		auto f = [this](const std::string& raw, const uint16_t optype) {
			const auto keypair = randomKeyPair(KeyType::ed25519);
			ripple::uint128 hex_table = ripple::from_hex_text<ripple::uint128>(table_name_);
			STTx tx(ttTABLELISTSET, [this, &hex_table, &keypair, raw, optype](STObject &obj) {
				//obj.setAccountID(sfUser, calcAccountID(keypair.first));
				set_AccountID(obj);
				obj.setFieldVL(sfSigningPubKey, keypair.first.slice());

				set_tables(obj);

				obj.setFieldU16(sfOpType, optype); // delete record
				ripple::Blob blob;
				blob.assign(raw.begin(), raw.end());
				obj.setFieldVL(sfRaw, blob);
			});
			std::pair<bool, std::string> ret;
			tx.sign(keypair.first, keypair.second);
			std::string text = tx.getFullText();
			TxStoreTransaction tr(txstore_dbconn_.get());
			ret = txstore_->Dispose(tx);
			tr.commit();
			return ret;
		};
		// create table
		BEAST_EXPECT(f("[\"1234\"]", 1).first == false);
		BEAST_EXPECT(f("[1234]", 1).first == false);
		BEAST_EXPECT(f("[]", 1).first == false);
		BEAST_EXPECT(f("[{}]", 1).first == false);
		BEAST_EXPECT(f("[{a}]", 9).first == false);
		BEAST_EXPECT(f("[{a},b]", 9).first == false);
		BEAST_EXPECT(f("[[]]", 1).first == false);

		// insert a record
		BEAST_EXPECT(f("[\"1234\"]", 6).first == false);
		BEAST_EXPECT(f("[1234]", 6).first == false);
		BEAST_EXPECT(f("[]", 6).first == false);
		BEAST_EXPECT(f("[{}]", 6).first == false);
		BEAST_EXPECT(f("[{a}]", 9).first == false);
		BEAST_EXPECT(f("[{a},b]", 9).first == false);
		BEAST_EXPECT(f("[[]]", 6).first == false);

		// update a record
		BEAST_EXPECT(f("[\"1234\"]", 8).first == false);
		BEAST_EXPECT(f("[1234]", 8).first == false);
		BEAST_EXPECT(f("[]", 8).first == false);
		BEAST_EXPECT(f("[{}]", 8).first == false);
		BEAST_EXPECT(f("[{a}]", 9).first == false);
		BEAST_EXPECT(f("[{a},b]", 9).first == false);
		BEAST_EXPECT(f("[[]]", 8).first == false);

		// delete a record
		BEAST_EXPECT(f("[\"1234\"]", 9).first == false);
		BEAST_EXPECT(f("[1234]", 9).first == false);
		BEAST_EXPECT(f("[]", 9).first == false);
		BEAST_EXPECT(f("[{}]", 9).first == false);
		BEAST_EXPECT(f("[{a}]", 9).first == false);
		BEAST_EXPECT(f("[{a},b]", 9).first == false);
		BEAST_EXPECT(f("[[]]", 9).first == false);

		// assert 
		BEAST_EXPECT(f("[\"1234\"]", 10).first == false);
		BEAST_EXPECT(f("[1234]", 10).first == false);
		BEAST_EXPECT(f("[]", 10).first == false);
		BEAST_EXPECT(f("[{}]", 10).first == false);
		BEAST_EXPECT(f("[{a}]", 10).first == false);
		BEAST_EXPECT(f("[{a},b]", 10).first == false);
		BEAST_EXPECT(f("[[]]", 10).first == false);
	}

	void test_crashfix_on_select() {
		{
			std::string query = "[[],{\"$and\":[{\"$order\":[{\"id\" : -1}]},{\"id\":{\"$ge\" : 3}}]}]";
			Json::Value result = getRecords(query);
			BEAST_EXPECT(result[jss::status] == "failure");
		}
	}

	void test_CreateTableTransaction() {
		const auto keypair = randomKeyPair(KeyType::ed25519);
		ripple::uint128 hex_table = ripple::from_hex_text<ripple::uint128>(table_name_);
		

		STTx tx(ttTABLELISTSET, [this, &hex_table, &keypair](STObject &obj) {
			//obj.setAccountID(sfUser, calcAccountID(keypair.first));
			set_AccountID(obj);
			obj.setFieldVL(sfSigningPubKey, keypair.first.slice());

			set_tables(obj);

			obj.setFieldU16(sfOpType, 1); // create table
			std::string raw = "[{\"field\":\"id\",\"type\":\"int\",\"PK\":1},{\"field\":\"cash\",\"type\":\"float\"},\
{\"field\":\"name\",\"type\":\"varchar\",\"length\":100,\"index\":1},{\"field\":\"comment\",\"type\":\"text\"},\
{\"field\":\"deci\",\"type\":\"decimal\",\"length\":16,\"accuracy\":2},{\"field\":\"datetime\",\"type\":\"datetime\"},\
{\"field\":\"ch\",\"type\":\"char\"},{\"field\":\"ch2\",\"type\":\"char\",\"length\":16, \"index\":1},\
{\"field\":\"date_field\",\"type\":\"date\"}]";
			ripple::Blob blob;
			blob.assign(raw.begin(), raw.end());
			obj.setFieldVL(sfRaw, blob);
			
			//ripple::Blob rule;
			//rule.assign(optionalRule.begin(), optionalRule.end());
			//obj.setFieldVL(sfOperationRule, rule);
		});

		std::string optionalRule = "{\"OperationRule\":{\"Insert\":{\"Condition\":{\"account\":\"$account\",\
\"txid\":\"$tx_hash\"},\"Count\":{\"AccountField\":\"account\",\"CountLimit\":5}},\
\"Update\":{\"Condition\":{\"$or\":[{\"age\":{\"$le\":28}},{\"id\":2}]},\"Fields\":[\"age\"]},\
\"Delete\":{\"Condition\":{\"account\":\"$account\"}},\"Get\":{\"Condition\":{\"id\":{\"$ge\":3}}}}}";
		tx.sign(keypair.first, keypair.second);
		std::string text = tx.getFullText();
		TxStoreTransaction tr(txstore_dbconn_.get());
		BEAST_EXPECT(txstore_->Dispose(tx, optionalRule).first == true);
		
		tr.commit();
	}

	void test_CreateTableForeginTransaction() {
		const auto keypair = randomKeyPair(KeyType::ed25519);
		ripple::uint128 hex_table = ripple::from_hex_text<ripple::uint128>(table_name_);
		STTx tx(ttTABLELISTSET, [this, &hex_table, &keypair](STObject &obj) {
			//obj.setAccountID(sfUser, calcAccountID(keypair.first));
			set_AccountID(obj);
			obj.setFieldVL(sfSigningPubKey, keypair.first.slice());

			set_tables(obj);

			obj.setFieldU16(sfOpType, 1); // create table
			std::string raw = "[{\"field\":\"id\",\"type\":\"int\",\"FK\":1,\"REFERENCES\":{\"table\":\"user\",\"field\":\"id\"}},\
{\"field\":\"cashid\",\"type\":\"int\",\"FK\":1,\"REFERENCES\":{\"table\":\"case\",\"field\":\"id\"}},\
{\"field\":\"xid\",\"type\":\"int\",\"PK\":1,\"NN\":1}]";
			ripple::Blob blob;
			blob.assign(raw.begin(), raw.end());
			obj.setFieldVL(sfRaw, blob);
		});
		tx.sign(keypair.first, keypair.second);
		std::string text = tx.getFullText();
		TxStoreTransaction tr(txstore_dbconn_.get());
		BEAST_EXPECT(txstore_->Dispose(tx).first == true);
		
		tr.commit();
	}

	void test_DropTableTransaction() {
		const auto keypair = randomKeyPair(KeyType::ed25519);
		ripple::uint128 hex_table = ripple::from_hex_text<ripple::uint128>(table_name_);
		STTx tx(ttTABLELISTSET, [this, &hex_table, &keypair](STObject &obj) {
			set_AccountID(obj);
			obj.setFieldVL(sfSigningPubKey, keypair.first.slice());

			set_tables(obj);

			obj.setFieldU16(sfOpType, 2); // drop table
		});
		tx.sign(keypair.first, keypair.second);

		TxStoreTransaction tr(txstore_dbconn_.get());
		BEAST_EXPECT(txstore_->Dispose(tx).first == true);
		tr.commit();
	}

	void test_DeleteRecordTransaction() {
		{
			const auto keypair = randomKeyPair(KeyType::ed25519);
			ripple::uint128 hex_table = ripple::from_hex_text<ripple::uint128>(table_name_);
			STTx tx(ttSQLSTATEMENT, [this, &hex_table, &keypair](STObject &obj) {
				set_OwnerID(obj);
				//obj.setAccountID(sfOwner, calcAccountID(keypair.first));
				obj.setFieldU16(sfOpType, 9); // delete record 
				set_tables(obj);

				std::string raw = "[{\"name\":{\"$regex\":\"/^test/\"}}]";
				ripple::Blob blob;
				blob.assign(raw.begin(), raw.end());
				obj.setFieldVL(sfRaw, blob);

			});
			tx.sign(keypair.first, keypair.second);

			TxStoreTransaction tr(txstore_dbconn_.get());
			BEAST_EXPECT(txstore_->Dispose(tx).first == true);
			tr.commit();

			std::string query = "[[\"id\",\"name\"], {\"id\":{\"$ge\":1}}]";
			Json::Value result = getRecords(query);
			std::string result_string = Json::jsonAsString(result);
			std::string expected_sql = "{\"lines\":[{\"id\":1,\"name\":\"test1\"},{\"id\":2,\"name\":\"test2\"},{\"id\":3,\"name\":\"test3\"}],\"status\":\"success\"}";
			BEAST_EXPECT(result_string == expected_sql);
		}

		{
			const auto keypair = randomKeyPair(KeyType::ed25519);
			ripple::uint128 hex_table = ripple::from_hex_text<ripple::uint128>(table_name_);
			STTx tx(ttSQLSTATEMENT, [this, &hex_table, &keypair](STObject &obj) {
				set_OwnerID(obj);
				//obj.setAccountID(sfOwner, calcAccountID(keypair.first));
				obj.setFieldU16(sfOpType, 9); // delete record 
				set_tables(obj);

				std::string raw = "[{\"name\":{\"$regex\":\"/test1/\"}}]";
				ripple::Blob blob;
				blob.assign(raw.begin(), raw.end());
				obj.setFieldVL(sfRaw, blob);

			});
			tx.sign(keypair.first, keypair.second);

			TxStoreTransaction tr(txstore_dbconn_.get());
			BEAST_EXPECT(txstore_->Dispose(tx).first == true);
			tr.commit();

			std::string query = "[[\"id\",\"name\"], {\"id\":{\"$ge\":1}}]";
			Json::Value result = getRecords(query);
			std::string result_string = Json::jsonAsString(result);
			std::string expected_sql = "{\"lines\":[{\"id\":2,\"name\":\"test2\"},{\"id\":3,\"name\":\"test3\"}],\"status\":\"success\"}";
			BEAST_EXPECT(result_string == expected_sql);
		}

		{
			const auto keypair = randomKeyPair(KeyType::ed25519);
			ripple::uint128 hex_table = ripple::from_hex_text<ripple::uint128>(table_name_);
			STTx tx(ttSQLSTATEMENT, [this, &hex_table, &keypair](STObject &obj) {
				set_OwnerID(obj);
				//obj.setAccountID(sfOwner, calcAccountID(keypair.first));
				obj.setFieldU16(sfOpType, 9); // delete record 
				set_tables(obj);

				std::string raw = "[{\"id\":{\"$ge\":1}}]";
				ripple::Blob blob;
				blob.assign(raw.begin(), raw.end());
				obj.setFieldVL(sfRaw, blob);

			});
			tx.sign(keypair.first, keypair.second);

			TxStoreTransaction tr(txstore_dbconn_.get());
			BEAST_EXPECT(txstore_->Dispose(tx).first == true);
			tr.commit();

			std::string query = "[[\"*\"]]";
			Json::Value result = getRecords(query);
			std::string result_string = Json::jsonAsString(result);
			std::string excepted_query = "{\"lines\":null,\"status\":\"success\"}";
			BEAST_EXPECT(result_string == excepted_query);
		}
	}

	void test_InsertRecordTransaction() {
		const auto keypair = randomKeyPair(KeyType::ed25519);
		ripple::uint128 hex_table = ripple::from_hex_text<ripple::uint128>(table_name_);
		STTx tx(ttSQLSTATEMENT, [this, &hex_table, &keypair](STObject &obj) {
			set_OwnerID(obj);
			obj.setFieldU16(sfOpType, 6); // insert one record 
			set_tables(obj);
			std::string raw = "[{\"id\":1,\"name\":\"test1\",\"cash\":100.00,\"comment\":\"zxc\",\"deci\":200.00000,\"datetime\":\"2017/1/11 9:42:54\"},\
{\"id\":2,\"name\":\"test2\",\"cash\":200.00,\"comment\":\"u.s\", \"deci\":300.00000,\"datetime\":\"2017/1/11 9:42:54\"},\
{\"id\":3,\"name\":\"test3\",\"cash\":300.00,\"comment\":\"u.s\", \"deci\":300.01,\"datetime\":\"2017/06/19 20:42:54\",\"ch\":\"testchar\",\"date_field\":\"2017/06/19\"}]";
			ripple::Blob blob;
			blob.assign(raw.begin(), raw.end());
			obj.setFieldVL(sfRaw, blob);
		});

		tx.sign(keypair.first, keypair.second);

		TxStoreTransaction tr(txstore_dbconn_.get());
		BEAST_EXPECT(txstore_->Dispose(tx).first == true);
		tr.commit();

		std::string query = "[[\"id\",\"name\"]]";
		Json::Value result = getRecords(query);
		std::string result_string = Json::jsonAsString(result);
		std::string excepted_query = "{\"lines\":[{\"id\":1,\"name\":\"test1\"},{\"id\":2,\"name\":\"test2\"},{\"id\":3,\"name\":\"test3\"}],\"status\":\"success\"}";
		BEAST_EXPECT(result_string == excepted_query);

		query = "[[\"id\",\"name\"],{\"id\":{\"$ge\":1}}]";
		result = getRecords(query);
		result_string = Json::jsonAsString(result);
		BEAST_EXPECT(result_string == excepted_query);

		query = "[[\"id\",\"name\"],{\"id\":{\"$ge\":1}},{\"$order\":[{\"id\":\"desc\"}],\"$limit\":{\"index\":0,\"total\":1}}]";
		result = getRecords(query);
		result_string = Json::jsonAsString(result);
		excepted_query = "{\"lines\":[{\"id\":3,\"name\":\"test3\"}],\"status\":\"success\"}";
		BEAST_EXPECT(result_string == excepted_query);
	}

	void test_UpdateRecordTransaction() {
		const auto keypair = randomKeyPair(KeyType::ed25519);
		ripple::uint128 hex_table = ripple::from_hex_text<ripple::uint128>(table_name_);
		STTx tx(ttSQLSTATEMENT, [this, &hex_table, &keypair](STObject &obj) {
			set_OwnerID(obj);
			obj.setFieldU16(sfOpType, 8); // update one record 
			set_tables(obj);
			std::string raw = "[{\"cash\":500.00,\"comment\":\"currency\"},{\"$or\":[{\"id\":{\"$eq\":1}},{\"id\":2}]}]";
			ripple::Blob blob;
			blob.assign(raw.begin(), raw.end());
			obj.setFieldVL(sfRaw, blob);
			
		});

		tx.sign(keypair.first, keypair.second);

		TxStoreTransaction tr(txstore_dbconn_.get());
		BEAST_EXPECT(txstore_->Dispose(tx).first == true);
		tr.commit();

		std::string query = "[[\"cash\",\"comment\"],{\"id\":{\"$gt\":1}}]";
		//std::string query = "[[\"cash\",\"comment\"],{\"$or\":[{\"$and\":[{\"id\":{\"$ge\":1}}, {\"id\":{\"$le\":2}}]},{\"cash\":\"u.s\"}]}]";
		Json::Value result = getRecords(query);
		std::string result_string = Json::jsonAsString(result);
		std::string excepted_query = "{\"lines\":[{\"cash\":500.0,\"comment\":\"currency\"},{\"cash\":300.0,\"comment\":\"u.s\"}],\"status\":\"success\"}";
		BEAST_EXPECT(result_string == excepted_query);
	}

	void test_SelectRecord() {
		// 测试 desc/asc 关键字解析, 如果 非 desc/asc 关键字的字符串都会处理成 asc
		{
			std::string field_str = "[[\"id\",\"name\"],{\"id\":{\"$ge\":1}},{\"$limit\":{\"index\":0,\"total\":10},\"$order\":[{\"id\":\"1\"}]}]";
			Json::Value result = getRecords(field_str);
			BEAST_EXPECT(boost::iequals(result["status"].asString(), "success"));
		}

		// 测试 desc/asc 关键字解析, 如果 非 -1/1 数字都会处理成 asc
		{
			std::string field_str = "[[\"id\",\"name\"],{\"id\":{\"$ge\":1}},{\"$limit\":{\"index\":0,\"total\":10},\"$order\":[{\"id\":100}]}]";
			Json::Value result = getRecords(field_str);
			BEAST_EXPECT(boost::iequals(result["status"].asString(), "success"));
		}

		// $order 关键字错误
		{
			std::string field_str = "[[\"id\",\"name\"],{\"id\":{\"$ge\":1}},{\"$limit\":{\"index\":0,\"total\":10},\"order\":[{\"id\":1}]}]";
			Json::Value result = getRecords(field_str);
			BEAST_EXPECT(result.isMember("error"));
		}

		// $limit 关键字错误
		{
			std::string field_str = "[[\"id\",\"name\"],{\"id\":{\"$ge\":1}},{\"limit\":{\"index\":0,\"total\":10},\"$order\":[{\"id\":1}]}]";
			Json::Value result = getRecords(field_str);
			BEAST_EXPECT(result.isMember("error"));
		}

		// $limit 和 $order 关键字错误
		{
			std::string field_str = "[[\"id\",\"name\"],{\"id\":{\"$ge\":1}},{\"limit\":{\"index\":0,\"total\":10},\"order\":[{\"id\":1}]}]";
			Json::Value result = getRecords(field_str);
			BEAST_EXPECT(result.isMember("error"));
		}

		{
			std::string field_str = "[[\"id\",\"name\"],{\"id\":{\"$ge\":1}},{\"$limit\":{\"index\":0,\"total\":10},\"$order\":[{\"id\":\"desc\"},{\"name\":\"asc\"}]}]";
			Json::Value result = getRecords(field_str);
			std::string result_sql = Json::jsonAsString(result);
			std::string expected_sql = "{\"lines\":[{\"id\":3,\"name\":\"test3\"},{\"id\":2,\"name\":\"test2\"},{\"id\":1,\"name\":\"test1\"}],\"status\":\"success\"}";
			BEAST_EXPECT(boost::iequals(expected_sql, result_sql));
		}

		{

			std::string field_str = "[[\"id\",\"name\"],{\"id\":{\"$in\":[1,2]}}]";
			Json::Value result = getRecords(field_str);
			std::string result_sql = Json::jsonAsString(result);
			std::string expected_sql = "{\"lines\":[{\"id\":1,\"name\":\"test1\"},{\"id\":2,\"name\":\"test2\"}],\"status\":\"success\"}";
			BEAST_EXPECT(boost::iequals(expected_sql, result_sql));
		}

		{

			std::string field_str = "[[\"id\",\"name\"],{\"id\":{\"$nin\":[1,2]}}]";
			Json::Value result = getRecords(field_str);
			std::string result_sql = Json::jsonAsString(result);
			std::string expected_sql = "{\"lines\":[{\"id\":3,\"name\":\"test3\"}],\"status\":\"success\"}";
			BEAST_EXPECT(boost::iequals(expected_sql, result_sql));
		}

		{

			std::string field_str = "[[\"id\",\"name\"],{\"name\":{\"$regex\":\"/test/\"}}]";
			Json::Value result = getRecords(field_str);
			std::string result_sql = Json::jsonAsString(result);
			std::string expected_sql = "{\"lines\":[{\"id\":1,\"name\":\"test1\"},{\"id\":2,\"name\":\"test2\"},{\"id\":3,\"name\":\"test3\"}],\"status\":\"success\"}";
			BEAST_EXPECT(boost::iequals(expected_sql, result_sql));
		}

		{
			std::string field_str = "[[\"id\",\"name\"],{\"name\":{\"$regex\":\"/^test/\"}}]";
			Json::Value result = getRecords(field_str);
			std::string result_sql = Json::jsonAsString(result);
			std::string expected_sql = "{\"lines\":null,\"status\":\"success\"}";
			BEAST_EXPECT(boost::iequals(expected_sql, result_sql));
		}

		// 比较字段名相等
		{
			std::string field_str = "[[\"id\",\"name\"],{\"name\":{\"$eq\":\"$.ch\"}}]";
			Json::Value result = getRecords(field_str);
			std::string result_sql = Json::jsonAsString(result);
			std::string expected_sql = "{\"lines\":null,\"status\":\"success\"}";
			BEAST_EXPECT(boost::iequals(expected_sql, result_sql));
		}
	}

	Json::Value getRecords(const std::string& query_fields) {
		using namespace test::jtx;
		Env env(*this);

		auto& app = env.app();
		Resource::Charge loadType = Resource::feeReferenceRPC;
		Resource::Consumer c;
		RPC::Context context{ beast::Journal(),{}, app, loadType,
			app.getOPs(), app.getLedgerMaster(), c, Role::USER,{} };

		Json::Value obj;
		Json::Value params;
		Json::Value p;
		p["offline"] = true;
		Json::Value tx_json;
		tx_json["Owner"] = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
		Json::Value tables;
		Json::Value t;
		Json::Value tv;
		tv["TableName"] = "000000001c2f5e6095324e2e08838f221a72ab4f";
		t["Table"] = tv;
		tables.append(t);
		tx_json["Tables"] = tables;

		tx_json["Raw"] = query_fields;
		p["tx_json"] = tx_json;

		context.params = p;
		std::string str_json = Json::jsonAsString(context.params);
		Json::Value result = txstore_->txHistory(context);
		return result;
	}

	void test_join_select() {
		// create both tables user and order_goods
		{
			std::string user_table = "CREATE TABLE IF NOT EXISTS t_user (uid int(11) NOT NULL,username varchar(30) NOT NULL,password char(32) NOT NULL);";
			std::string order_goods = "CREATE TABLE IF NOT EXISTS t_order_goods (oid int(11) NOT NULL,uid int(11) NOT NULL,name varchar(50) NOT NULL,buytime int(11) NOT NULL);";
			DatabaseCon* db = txstore_dbconn_->GetDBConn();
			soci::session& s = db->getSession();
			s << user_table;
			s << order_goods;
			// insert records
			s << "insert into t_user (uid, username, password) values(1,'jingtian','123')";
			s << "insert into t_user (uid, username, password) values(2,'wangxiaoer','123')";
			s << "insert into t_user (uid, username, password) values(3,'peersafe','123')";

			s << "insert into t_order_goods (oid, uid, name, buytime) values(1,2,'apple',12333222)";
			s << "insert into t_order_goods (oid, uid, name, buytime) values(2,3,'iphone',12333222)";
			s << "insert into t_order_goods (oid, uid, name, buytime) values(3,7,'xor',12333222)";
		}
		// inner join
		{
			using namespace test::jtx;
			Env env(*this);

			auto& app = env.app();
			Resource::Charge loadType = Resource::feeReferenceRPC;
			Resource::Consumer c;
			RPC::Context context{ beast::Journal(),{}, app, loadType,
				app.getOPs(), app.getLedgerMaster(), c, Role::USER,{} };

			Json::Value obj;
			Json::Value params;
			Json::Value p;
			p["offline"] = true;
			Json::Value tx_json;
			tx_json["Owner"] = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";

			// tables
			Json::Value tables;
			{
				Json::Value t;
				Json::Value tv;
				tv["TableName"] = "user";
				tv["Alias"] = "u";
				tv["join"] = "inner";
				t["Table"] = tv;
				tables.append(t);
			}
			{
				Json::Value t;
				Json::Value tv;
				tv["TableName"] = "order_goods";
				tv["Alias"] = "o";
				t["Table"] = tv;
				tables.append(t);
			}
			tx_json["Tables"] = tables;

			std::string field_str = "[[\"u.uid\",\"u.username\",\"u.password\",\"o.name\",\"o.buytime\"],{\"$join\":{\"u.uid\":{\"$eq\":\"o.uid\"}}}]";
			tx_json["Raw"] = field_str;
			p["tx_json"] = tx_json;

			context.params = p;
			std::string str_json = Json::jsonAsString(context.params);
			Json::Value result = txstore_->txHistory(context);
			std::string real_result = Json::jsonAsString(result);
			std::string expected_resul = "{\"lines\":[{\"buytime\":12333222,\"name\":\"apple\",\"password\":\"123\",\"uid\":2,\"username\":\"wangxiaoer\"},{\"buytime\":12333222,\"name\":\"iphone\",\"password\":\"123\",\"uid\":3,\"username\":\"peersafe\"}],\"status\":\"success\"}";
			BEAST_EXPECT(boost::iequals(expected_resul, real_result));
		}

		// left join
		{
			using namespace test::jtx;
			Env env(*this);

			auto& app = env.app();
			Resource::Charge loadType = Resource::feeReferenceRPC;
			Resource::Consumer c;
			RPC::Context context{ beast::Journal(),{}, app, loadType,
				app.getOPs(), app.getLedgerMaster(), c, Role::USER,{} };

			Json::Value obj;
			Json::Value params;
			Json::Value p;
			p["offline"] = true;
			Json::Value tx_json;
			tx_json["Owner"] = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";

			// tables
			Json::Value tables;
			{
				Json::Value t;
				Json::Value tv;
				tv["TableName"] = "user";
				tv["Alias"] = "u";
				tv["join"] = "left";
				t["Table"] = tv;
				tables.append(t);
			}
			{
				Json::Value t;
				Json::Value tv;
				tv["TableName"] = "order_goods";
				tv["Alias"] = "o";
				t["Table"] = tv;
				tables.append(t);
			}
			tx_json["Tables"] = tables;

			std::string field_str = "[[\"u.uid\",\"u.username\",\"u.password\",\"o.name\",\"o.buytime\"],{\"$join\":{\"u.uid\":{\"$eq\":\"o.uid\"}}}]";
			tx_json["Raw"] = field_str;
			p["tx_json"] = tx_json;

			context.params = p;
			std::string str_json = Json::jsonAsString(context.params);
			Json::Value result = txstore_->txHistory(context);
			std::string real_result = Json::jsonAsString(result);
			std::string expected_resul = "{\"lines\":[{\"buytime\":12333222,\"name\":\"apple\",\"password\":\"123\",\"uid\":2,\"username\":\"wangxiaoer\"},{\"buytime\":12333222,\"name\":\"iphone\",\"password\":\"123\",\"uid\":3,\"username\":\"peersafe\"},{\"buytime\":0,\"name\":\"null\",\"password\":\"123\",\"uid\":1,\"username\":\"jingtian\"}],\"status\":\"success\"}";
			BEAST_EXPECT(boost::iequals(expected_resul, real_result));
		}

		// left join with where conditions
		{
			using namespace test::jtx;
			Env env(*this);

			auto& app = env.app();
			Resource::Charge loadType = Resource::feeReferenceRPC;
			Resource::Consumer c;
			RPC::Context context{ beast::Journal(),{}, app, loadType,
				app.getOPs(), app.getLedgerMaster(), c, Role::USER,{} };

			Json::Value obj;
			Json::Value params;
			Json::Value p;
			p["offline"] = true;
			Json::Value tx_json;
			tx_json["Owner"] = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";

			// tables
			Json::Value tables;
			{
				Json::Value t;
				Json::Value tv;
				tv["TableName"] = "user";
				tv["Alias"] = "u";
				tv["join"] = "left";
				t["Table"] = tv;
				tables.append(t);
			}
			{
				Json::Value t;
				Json::Value tv;
				tv["TableName"] = "order_goods";
				tv["Alias"] = "o";
				t["Table"] = tv;
				tables.append(t);
			}
			tx_json["Tables"] = tables;

			std::string field_str = "[[\"u.uid\",\"u.username\",\"u.password\",\"o.name\",\"o.buytime\"],{\"$join\":{\"u.uid\":{\"$eq\":\"o.uid\"}}}, {\"buytime\":{\"$ne\":0}}]";
			tx_json["Raw"] = field_str;
			p["tx_json"] = tx_json;

			context.params = p;
			std::string str_json = Json::jsonAsString(context.params);
			Json::Value result = txstore_->txHistory(context);
			std::string real_result = Json::jsonAsString(result);
			std::string expected_resul = "{\"lines\":[{\"buytime\":12333222,\"name\":\"apple\",\"password\":\"123\",\"uid\":2,\"username\":\"wangxiaoer\"},{\"buytime\":12333222,\"name\":\"iphone\",\"password\":\"123\",\"uid\":3,\"username\":\"peersafe\"}],\"status\":\"success\"}";
			BEAST_EXPECT(boost::iequals(expected_resul, real_result));
		}

		// group by
		{
			using namespace test::jtx;
			Env env(*this);

			auto& app = env.app();
			Resource::Charge loadType = Resource::feeReferenceRPC;
			Resource::Consumer c;
			RPC::Context context{ beast::Journal(),{}, app, loadType,
				app.getOPs(), app.getLedgerMaster(), c, Role::USER,{} };

			Json::Value obj;
			Json::Value params;
			Json::Value p;
			p["offline"] = true;
			Json::Value tx_json;
			tx_json["Owner"] = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";

			// tables
			Json::Value tables;
			Json::Value tv, t;
			tv["TableName"] = "order_goods";
			t["Table"] = tv;
			tables.append(t);
			tx_json["Tables"] = tables;

			std::string field_str = "[[\"oid\",\"sum(buytime) as buytime\"],{\"oid\":1},{\"$group\":[\"oid\"]}]";
			tx_json["Raw"] = field_str;
			p["tx_json"] = tx_json;

			context.params = p;
			std::string str_json = Json::jsonAsString(context.params);
			Json::Value result = txstore_->txHistory(context);
			std::string real_result = Json::jsonAsString(result);
			std::string expected_resul = "{\"lines\":[{\"buytime\":12333222.0,\"oid\":1}],\"status\":\"success\"}";
			BEAST_EXPECT(boost::iequals(expected_resul, real_result));
		}

		// having
		{
			using namespace test::jtx;
			Env env(*this);

			auto& app = env.app();
			Resource::Charge loadType = Resource::feeReferenceRPC;
			Resource::Consumer c;
			RPC::Context context{ beast::Journal(),{}, app, loadType,
				app.getOPs(), app.getLedgerMaster(), c, Role::USER,{} };

			Json::Value obj;
			Json::Value params;
			Json::Value p;
			p["offline"] = true;
			Json::Value tx_json;
			tx_json["Owner"] = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";

			// tables
			Json::Value tables;
			Json::Value tv, t;
			tv["TableName"] = "order_goods";
			t["Table"] = tv;
			tables.append(t);
			tx_json["Tables"] = tables;

			std::string field_str = "[[\"oid\",\"sum(buytime) as buytime\"],{\"oid\":1},{\"$group\":[\"oid\"], \"$having\":{\"buytime\":{\"$gt\":1000}}}]";
			tx_json["Raw"] = field_str;
			p["tx_json"] = tx_json;

			context.params = p;
			std::string str_json = Json::jsonAsString(context.params);
			Json::Value result = txstore_->txHistory(context);
			std::string real_result = Json::jsonAsString(result);
			std::string expected_resul = "{\"lines\":[{\"buytime\":12333222.0,\"oid\":1}],\"status\":\"success\"}";
			BEAST_EXPECT(boost::iequals(expected_resul, real_result));	
		}

		// drop tables
		{
			DatabaseCon* db = txstore_dbconn_->GetDBConn();
			soci::session& s = db->getSession();
			s << "drop tables if exists t_user";
			s << "drop tables if exists t_order_goods";
		}
	}

	void test_assert_transaction() {
		// test exists tables
		{
			const auto keypair = randomKeyPair(KeyType::ed25519);
			ripple::uint128 hex_table = ripple::from_hex_text<ripple::uint128>(table_name_);
			STTx tx(ttSQLSTATEMENT, [this, &hex_table, &keypair](STObject &obj) {
				set_OwnerID(obj);
				obj.setFieldU16(sfOpType, 10); // assert one record 
				set_tables(obj);
				std::string raw = "[{\"$IsExisted\":1}]";
				ripple::Blob blob;
				blob.assign(raw.begin(), raw.end());
				obj.setFieldVL(sfRaw, blob);

			});

			tx.sign(keypair.first, keypair.second);

			TxStoreTransaction tr(txstore_dbconn_.get());
			BEAST_EXPECT(txstore_->Dispose(tx).first == true);
			tr.commit();
		}

		// test rowcounts
		{
			const auto keypair = randomKeyPair(KeyType::ed25519);
			ripple::uint128 hex_table = ripple::from_hex_text<ripple::uint128>(table_name_);
			STTx tx(ttSQLSTATEMENT, [this, &hex_table, &keypair](STObject &obj) {
				set_OwnerID(obj);
				obj.setFieldU16(sfOpType, 10); // assert one record 
				set_tables(obj);
				std::string raw = "[{\"$RowCount\":3}, {\"id\":{\"$ge\":1}}]";
				ripple::Blob blob;
				blob.assign(raw.begin(), raw.end());
				obj.setFieldVL(sfRaw, blob);

			});

			tx.sign(keypair.first, keypair.second);

			TxStoreTransaction tr(txstore_dbconn_.get());
			BEAST_EXPECT(txstore_->Dispose(tx).first == true);
			tr.commit();
		}

		{

			const auto keypair = randomKeyPair(KeyType::ed25519);
			ripple::uint128 hex_table = ripple::from_hex_text<ripple::uint128>(table_name_);
			STTx tx(ttSQLSTATEMENT, [this, &hex_table, &keypair](STObject &obj) {
				set_OwnerID(obj);
				obj.setFieldU16(sfOpType, 10); // assert one record 
				set_tables(obj);
				std::string raw = "[{\"id\":1}, {\"id\":{\"$eq\":1}}]";
				//std::string raw = "[{\"$or\":[{\"name\":\"peersafe\"},{\"id\":1}]}, {\"id\":{\"$ge\":1}}]";
				//std::string raw = "[{\"$and\":[{\"name\":\"peersafe\"},{\"id\":1}]}, {\"id\":{\"$ge\":1}}]";
				//std::string raw = "[{\"$and\":[{\"$or\":[{\"name\":\"peersafe\"},{\"id\":1}]},{\"id\":10}]}, {\"id\":{\"$ge\":1}}]";
				ripple::Blob blob;
				blob.assign(raw.begin(), raw.end());
				obj.setFieldVL(sfRaw, blob);

			});

			tx.sign(keypair.first, keypair.second);

			TxStoreTransaction tr(txstore_dbconn_.get());
			BEAST_EXPECT(txstore_->Dispose(tx).first == false);
			tr.commit();
		}

		{
			const auto keypair = randomKeyPair(KeyType::ed25519);
			ripple::uint128 hex_table = ripple::from_hex_text<ripple::uint128>(table_name_);
			STTx tx(ttSQLSTATEMENT, [this, &hex_table, &keypair](STObject &obj) {
				set_OwnerID(obj);
				obj.setFieldU16(sfOpType, 10); // assert one record 
				set_tables(obj);
				std::string raw = "[{\"id\":2}, {\"id\":{\"$eq\":1}}]";
				ripple::Blob blob;
				blob.assign(raw.begin(), raw.end());
				obj.setFieldVL(sfRaw, blob);

			});

			tx.sign(keypair.first, keypair.second);

			TxStoreTransaction tr(txstore_dbconn_.get());
			BEAST_EXPECT(txstore_->Dispose(tx).first == false);
			tr.commit();
		}

		{
			const auto keypair = randomKeyPair(KeyType::ed25519);
			ripple::uint128 hex_table = ripple::from_hex_text<ripple::uint128>(table_name_);
			STTx tx(ttSQLSTATEMENT, [this, &hex_table, &keypair](STObject &obj) {
				set_OwnerID(obj);
				obj.setFieldU16(sfOpType, 10); // assert one record 
				set_tables(obj);
				std::string raw = "[{\"$and\":[{\"id\":{\"$gt\":0}},{\"name\":\"test1\"}]}, {\"id\":{\"$eq\":1}}]";
				ripple::Blob blob;
				blob.assign(raw.begin(), raw.end());
				obj.setFieldVL(sfRaw, blob);

			});

			tx.sign(keypair.first, keypair.second);

			TxStoreTransaction tr(txstore_dbconn_.get());
			BEAST_EXPECT(txstore_->Dispose(tx).first == false);
			tr.commit();
		}

		{
			const auto keypair = randomKeyPair(KeyType::ed25519);
			ripple::uint128 hex_table = ripple::from_hex_text<ripple::uint128>(table_name_);
			STTx tx(ttSQLSTATEMENT, [this, &hex_table, &keypair](STObject &obj) {
				set_OwnerID(obj);
				obj.setFieldU16(sfOpType, 10); // assert one record 
				set_tables(obj);
				std::string raw = "[{\"$and\":[{\"id\":{\"$lt\":0}},{\"name\":\"test1\"}]}, {\"id\":{\"$eq\":1}}]";
				ripple::Blob blob;
				blob.assign(raw.begin(), raw.end());
				obj.setFieldVL(sfRaw, blob);

			});

			tx.sign(keypair.first, keypair.second);

			TxStoreTransaction tr(txstore_dbconn_.get());
			BEAST_EXPECT(txstore_->Dispose(tx).first == false);
			tr.commit();
		}

		{
			const auto keypair = randomKeyPair(KeyType::ed25519);
			ripple::uint128 hex_table = ripple::from_hex_text<ripple::uint128>(table_name_);
			STTx tx(ttSQLSTATEMENT, [this, &hex_table, &keypair](STObject &obj) {
				set_OwnerID(obj);
				obj.setFieldU16(sfOpType, 10); // assert one record 
				set_tables(obj);
				std::string raw = "[{\"$or\":[{\"id\":{\"$lt\":0}},{\"name\":\"test1\"}]}, {\"id\":{\"$eq\":1}}]";
				ripple::Blob blob;
				blob.assign(raw.begin(), raw.end());
				obj.setFieldVL(sfRaw, blob);

			});

			tx.sign(keypair.first, keypair.second);

			TxStoreTransaction tr(txstore_dbconn_.get());
			BEAST_EXPECT(txstore_->Dispose(tx).first == false);
			tr.commit();
		}

		{
			const auto keypair = randomKeyPair(KeyType::ed25519);
			ripple::uint128 hex_table = ripple::from_hex_text<ripple::uint128>(table_name_);
			STTx tx(ttSQLSTATEMENT, [this, &hex_table, &keypair](STObject &obj) {
				set_OwnerID(obj);
				obj.setFieldU16(sfOpType, 10); // assert one record 
				set_tables(obj);
				std::string raw = "[{\"id\":{\"$in\":[1,2,3]}}, {\"id\":{\"$eq\":1}}]";
				ripple::Blob blob;
				blob.assign(raw.begin(), raw.end());
				obj.setFieldVL(sfRaw, blob);

			});

			tx.sign(keypair.first, keypair.second);

			TxStoreTransaction tr(txstore_dbconn_.get());
			BEAST_EXPECT(txstore_->Dispose(tx).first == false);
			tr.commit();
		}

		{
			const auto keypair = randomKeyPair(KeyType::ed25519);
			ripple::uint128 hex_table = ripple::from_hex_text<ripple::uint128>(table_name_);
			STTx tx(ttSQLSTATEMENT, [this, &hex_table, &keypair](STObject &obj) {
				set_OwnerID(obj);
				obj.setFieldU16(sfOpType, 10); // assert one record 
				set_tables(obj);
				std::string raw = "[{\"id\":{\"$nin\":[1,2,3]}}, {\"id\":{\"$eq\":1}}]";
				ripple::Blob blob;
				blob.assign(raw.begin(), raw.end());
				obj.setFieldVL(sfRaw, blob);

			});

			tx.sign(keypair.first, keypair.second);

			TxStoreTransaction tr(txstore_dbconn_.get());
			BEAST_EXPECT(txstore_->Dispose(tx).first == false);
			tr.commit();
		}

		{
			const auto keypair = randomKeyPair(KeyType::ed25519);
			ripple::uint128 hex_table = ripple::from_hex_text<ripple::uint128>(table_name_);
			STTx tx(ttSQLSTATEMENT, [this, &hex_table, &keypair](STObject &obj) {
				set_OwnerID(obj);
				obj.setFieldU16(sfOpType, 10); // assert one record 
				set_tables(obj);
				std::string raw = "[{\"id\":{\"$nin\":[1,2,3]}}, {\"id\":{\"$eq\":1}}]";
				ripple::Blob blob;
				blob.assign(raw.begin(), raw.end());
				obj.setFieldVL(sfRaw, blob);

			});

			tx.sign(keypair.first, keypair.second);

			TxStoreTransaction tr(txstore_dbconn_.get());
			BEAST_EXPECT(txstore_->Dispose(tx).first == false);
			tr.commit();
		}

		{
			const auto keypair = randomKeyPair(KeyType::ed25519);
			ripple::uint128 hex_table = ripple::from_hex_text<ripple::uint128>(table_name_);
			STTx tx(ttSQLSTATEMENT, [this, &hex_table, &keypair](STObject &obj) {
				set_OwnerID(obj);
				obj.setFieldU16(sfOpType, 10); // assert one record 
				set_tables(obj);
				std::string raw = "[{\"id\":{\"$nin\":[2,3]}}, {\"id\":{\"$eq\":1}}]";
				ripple::Blob blob;
				blob.assign(raw.begin(), raw.end());
				obj.setFieldVL(sfRaw, blob);

			});

			tx.sign(keypair.first, keypair.second);

			TxStoreTransaction tr(txstore_dbconn_.get());
			BEAST_EXPECT(txstore_->Dispose(tx).first == false);
			tr.commit();
		}
	}

	std::pair<int, conditionTree> createConditionTree(const Json::Value& conditions) {
		return conditionTree::createRoot(conditions);
	}

	std::pair<int, std::string> parse_conditions(const Json::Value& raw_value, conditionTree& root) {
		return conditionParse::parse_conditions(raw_value, root);
	}

	void test_sample_mongodb_json_style() {
		std::string ops[] = { "$eq","$ne","$lt","$le","$gt","$ge" };
		std::string expect_ops[] = {"=", "!=", "<", "<=", ">", ">="};
		for (size_t i = 0; i < 6; i++) {
			std::string raw_string = (boost::format("[{\"age\":{\"%1%\":20}}]") %ops[i]).str();
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			//BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			//BEAST_EXPECT(result2.first == 0);

			std::string result_conditions = result.second.asString();
			std::string expect_conditions = (boost::format("age %1% 20") %expect_ops[i]).str();
			BEAST_EXPECT(result_conditions == expect_conditions);
		}

		{
			std::string raw_string = "[{\"age\":{\"$in\":[20,30]}}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			//BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			//BEAST_EXPECT(result2.first == 0);

			std::string result_conditions = result.second.asString();
			std::string expect_conditions = "age in (20,30)";
			BEAST_EXPECT(result_conditions == expect_conditions);
		}

		{
			std::string raw_string = "[{\"name\":{\"$nin\":[\"peersafe\",\"zongxiang\"]}}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			//BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			//BEAST_EXPECT(result2.first == 0);

			std::string result_conditions = result.second.asString();
			std::string expect_conditions = "name not in ('peersafe','zongxiang')";
			BEAST_EXPECT(result_conditions == expect_conditions);
		}

		{
			std::string raw_string = "[{\"age\":20}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			//BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			//BEAST_EXPECT(result2.first == 0);

			std::string result_conditions = result.second.asString();
			std::string expect_conditions = "age = 20";
			BEAST_EXPECT(result_conditions == expect_conditions);
		}

		{
			std::string raw_string = "[{\"name\":\"peersafe\"}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			//BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			//BEAST_EXPECT(result2.first == 0);

			std::string result_conditions = result.second.asString();
			std::string expect_conditions = "name = 'peersafe'";
			BEAST_EXPECT(result_conditions == expect_conditions);
		}

		{
			std::string raw_string = "[{\"name\":{\"$regex\":\"/^peersafe/\"}}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			//BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			//BEAST_EXPECT(result2.first == 0);

			std::string result_conditions = result.second.asString();
			std::string expect_conditions = "name like '%peersafe'";
			BEAST_EXPECT(result_conditions == expect_conditions);
		}

		{
			std::string raw_string = "[{\"name\":{\"$regex\":\"/peersafe/\"}}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			//BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			//BEAST_EXPECT(result2.first == 0);

			std::string result_conditions = result.second.asString();
			std::string expect_conditions = "name like '%peersafe%'";
			BEAST_EXPECT(result_conditions == expect_conditions);
		}

	}

	void test_keywork() {
		{
			std::string raw_string = "[{\"age\":{\"ge\":20}}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			BEAST_EXPECT(result2.first == -1);
		}

		{
			std::string raw_string = "[{\"$and\":[{\"age\":{\"ge\":20}},{\"name\":\"peersafe\"}]}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			BEAST_EXPECT(result2.first == -1);
		}

		{
			std::string raw_string = "[{\"and\":[{\"age\":{\"$ge\":20}},{\"name\":\"peersafe\"}]}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			BEAST_EXPECT(result.first == -1);
		}

		{
			std::string raw_string = "[{\"or\":[{\"age\":{\"$ge\":20}},{\"name\":\"peersafe\"}]}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			BEAST_EXPECT(result.first == -1);
		}

		{
			std::string raw_string = "[{\"$or\":[{\"and\":[{\"id\":1},{\"age\":20}]}, {\"and\":[{\"id\":10},{\"age\":30}]}]}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			BEAST_EXPECT(result.first == 0);
			
			auto result2 = parse_conditions(conditions, result.second);
			BEAST_EXPECT(result2.first == -1);
		}

		{
			std::string raw_string = "[{\"$or\":[{\"$and\":[{\"id\":1},{\"age\":20}]}, {\"and\":[{\"id\":10},{\"age\":30}]}]}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			BEAST_EXPECT(result.first == 0);
			
			auto result2 = parse_conditions(conditions, result.second);
			BEAST_EXPECT(result2.first == -1);
		}

		{
			std::string raw_string = "[{\"$and\":[{\"or\":[{\"id\":1},{\"age\":20}]}, {\"or\":[{\"id\":10},{\"age\":30}]}]}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			BEAST_EXPECT(result.first == 0);
			
			auto result2 = parse_conditions(conditions, result.second);
			BEAST_EXPECT(result2.first == -1);
		}


		{
			std::string raw_string = "[{\"$and\":[{\"$or\":[{\"id\":1},{\"age\":20}]}, {\"or\":[{\"id\":10},{\"age\":30}]}]}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			BEAST_EXPECT(result.first == 0);
			
			auto result2 = parse_conditions(conditions, result.second);
			BEAST_EXPECT(result2.first == -1);
		}
	}

	void test_logicalAnd() {
		std::string ops[] = { "$eq","$ne","$lt","$le","$gt","$ge" };
		std::string expect_ops[] = {"=", "!=", "<", "<=", ">", ">="};
		for (size_t i = 0; i < 6; i++) {
			std::string raw_string = (boost::format("[{\"age\":{\"%1%\":20}, \"name\":{\"%1%\":\"peersafe\"}}]") 
				%ops[i]).str();
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			//BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			//BEAST_EXPECT(result2.first == 0);

			std::string result_conditions = result.second.asString();
			std::string expect_conditions = (boost::format("age %1% 20 and name %1% 'peersafe'") %expect_ops[i]).str();
			BEAST_EXPECT(result_conditions == expect_conditions);
		}

		{
			std::string raw_string = "[{\"age\":{\"$gt\":20,\"$lt\":60}}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			//BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			//BEAST_EXPECT(result2.first == 0);

			std::string result_conditions = result.second.asString();
			std::string expect_conditions = "age > 20 and age < 60";
			BEAST_EXPECT(result_conditions == expect_conditions);
		}

		{
			std::string raw_string = "[{\"age\":{\"$gt\":20,\"$lt\":60},\"id\":{\"$ge\":10,\"$le\":100}}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			//BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			//BEAST_EXPECT(result2.first == 0);

			std::string result_conditions = result.second.asString();
			std::string expect_conditions = "age > 20 and age < 60 and id >= 10 and id <= 100";
			BEAST_EXPECT(result_conditions == expect_conditions);
		}

		{
			std::string raw_string = "[{\"$and\":[{\"age\":{\"$ge\":20}},{\"name\":\"peersafe\"}]}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			//BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			//BEAST_EXPECT(result2.first == 0);

			std::string result_conditions = result.second.asString();
			std::string expect_conditions = "age >= 20 and name = 'peersafe'";
			BEAST_EXPECT(result_conditions == expect_conditions);	
		}

		{
			std::string raw_string = "[{\"$and\":[{\"age\":{\"$ge\":20,\"$le\":60}},{\"id\":{\"$ge\":10,\"$le\":100}}]}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			//BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			//BEAST_EXPECT(result2.first == 0);

			std::string result_conditions = result.second.asString();
			std::string expect_conditions = "age >= 20 and age <= 60 and id >= 10 and id <= 100";
			BEAST_EXPECT(result_conditions == expect_conditions);	
		}

		{
			std::string raw_string = "[{\"$and\":[{\"$or\":[{\"name\":\"peersafe\"},{\"id\":{\"$ge\":20}}]},{\"age\":20}]}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			//BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			//BEAST_EXPECT(result2.first == 0);

			if(result2.first == 0) {
				std::string result_conditions = result.second.asString();
				std::string expect_conditions = "(name = 'peersafe' or id >= 20) and age = 20";
				BEAST_EXPECT(result_conditions == expect_conditions);
			}
			else {
				BEAST_EXPECT(result2.first == 0);
			}
		}
	}

	void test_logicalOr() {
		std::string ops[] = { "$eq","$ne","$lt","$le","$gt","$ge" };
		std::string expect_ops[] = { "=", "!=", "<", "<=", ">", ">=" };
		for (size_t i = 0; i < 6; i++) {
			std::string raw_string = (boost::format("[{\"$or\":[{\"age\":{\"%1%\":20}}, {\"name\":{\"%1%\":\"peersafe\"}}]}]")
				% ops[i]).str();
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			//BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			//BEAST_EXPECT(result2.first == 0);
			
			if (result2.first == 0) {
				std::string result_conditions = result.second.asString();
				std::string expect_conditions = (boost::format("age %1% 20 or name %1% 'peersafe'") % expect_ops[i]).str();
				BEAST_EXPECT(result_conditions == expect_conditions);
			}
			else {
				BEAST_EXPECT(result2.first == 0);
			}
		}

		{
			std::string raw_string = "[{\"$or\":[{\"$and\":[{\"age\":{\"$ge\":20,\"$le\":60}}]},{\"$and\":[{\"id\":{\"$ge\":10,\"$le\":100}}]}]}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			//BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			//BEAST_EXPECT(result2.first == 0);

			if (result2.first == 0) {
				std::string result_conditions = result.second.asString();
				std::string expect_conditions = "(age >= 20 and age <= 60) or (id >= 10 and id <= 100)";
				BEAST_EXPECT(result_conditions == expect_conditions);
			}
			else {
				BEAST_EXPECT(result2.first == 0);
			}
		}

		{
			std::string raw_string = "[{\"$or\":[{\"$and\":[{\"age\":{\"$ge\":20,\"$le\":60}}]},{\"id\":{\"$ge\":10}}]}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			//BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			//BEAST_EXPECT(result2.first == 0);

			if (result2.first == 0) {
				std::string result_conditions = result.second.asString();
				std::string expect_conditions = "(age >= 20 and age <= 60) or id >= 10";
				BEAST_EXPECT(result_conditions == expect_conditions);
			}
			else {
				BEAST_EXPECT(result2.first == 0);
			}
		}

		{
			std::string raw_string = "[{\"$or\":[{\"age\":{\"$ge\":20}},{\"$and\":[{\"id\":{\"$ge\":10,\"$le\":100}}]}]}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			//BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			//BEAST_EXPECT(result2.first == 0);

			if (result2.first == 0) {
				std::string result_conditions = result.second.asString();
				std::string expect_conditions = "age >= 20 or (id >= 10 and id <= 100)";
				BEAST_EXPECT(result_conditions == expect_conditions);
			}
			else {
				BEAST_EXPECT(result2.first == 0);
			}
		}

		{
			std::string raw_string = "[{\"$or\":[{\"age\":{\"$ge\":100}},{\"age\":{\"$le\":20}},{\"$and\":[{\"id\":{\"$ge\":10,\"$le\":100}}]}]}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			//BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			//BEAST_EXPECT(result2.first == 0);

			if (result2.first == 0) {
				std::string result_conditions = result.second.asString();
				std::string expect_conditions = "age >= 100 or age <= 20 or (id >= 10 and id <= 100)";
				BEAST_EXPECT(result_conditions == expect_conditions);
			}
			else {
				BEAST_EXPECT(result2.first == 0);
			}
		}

		{
			std::string raw_string = "[{\"age\":{\"$ge\":20}},{\"name\":\"peersafe\"}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			//BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			//BEAST_EXPECT(result2.first == 0);

			if (result2.first == 0) {
				std::string result_conditions = result.second.asString();
				std::string expect_conditions = "age >= 20 or name = 'peersafe'";
				BEAST_EXPECT(result_conditions == expect_conditions);
			}
			else {
				BEAST_EXPECT(result2.first == 0);
			}
		}

		{
			std::string raw_string = "[{\"age\":{\"$ge\":20}},{\"name\":{\"$eq\":\"peersafe\"}}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			//BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			//BEAST_EXPECT(result2.first == 0);

			if (result2.first == 0) {
				std::string result_conditions = result.second.asString();
				std::string expect_conditions = "age >= 20 or name = 'peersafe'";
				BEAST_EXPECT(result_conditions == expect_conditions);
			}
			else {
				BEAST_EXPECT(result2.first == 0);
			}
		}

		{
			std::string raw_string = "[{\"age\":{\"$ge\":20,\"$le\":100}},{\"name\":{\"$eq\":\"peersafe\"}}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			//BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			//BEAST_EXPECT(result2.first == 0);

			if (result2.first == 0) {
				std::string result_conditions = result.second.asString();
				std::string expect_conditions = "(age >= 20 and age <= 100) or name = 'peersafe'";
				BEAST_EXPECT(result_conditions == expect_conditions);
			}
			else {
				BEAST_EXPECT(result2.first == 0);
			}
		}

		{
			std::string raw_string = "[{\"age\":{\"$ge\":20},\"id\":{\"$ge\":10}},{\"name\":{\"$eq\":\"peersafe\"}}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			//BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			//BEAST_EXPECT(result2.first == 0);

			if (result2.first == 0) {
				std::string result_conditions = result.second.asString();
				std::string expect_conditions = "(age >= 20 and id >= 10) or name = 'peersafe'";
				BEAST_EXPECT(result_conditions == expect_conditions);
			}
			else {
				BEAST_EXPECT(result2.first == 0);
			}
		}

	}

	void test_logicRecursion() {
		{
			std::string raw_string = "[{\"$or\":[{\"$and\":[{\"$or\":[{\"age\":10},{\"name\": \"3kx\"}]},{\"name\": \"peersafe\"}]},{\"id\": 1000}]}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			//BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			//BEAST_EXPECT(result2.first == 0);

			if (result2.first == 0) {
				std::string result_conditions = result.second.asString();
				std::string expect_conditions = "((age = 10 or name = '3kx') and name = 'peersafe') or id = 1000";
				BEAST_EXPECT(result_conditions == expect_conditions);
			}
			else {
				BEAST_EXPECT(result2.first == 0);
			}
		}

		{
			// 嵌套 2 层
			std::string raw_string = "[{\"$or\": [{\"$or\": [{\"$or\": [{\"age\":10},{\"name\": \"3kx\"}]},{\"name\": \"peersafe\"}]},{\"$and\": [{\"$or\": [{\"age\":30},{\"name\": \"xxxkkk\"}]},{\"name\": \"zxxx\"}]}]}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			//BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			//BEAST_EXPECT(result2.first == 0);

			if (result2.first == 0) {
				std::string result_conditions = result.second.asString();
				std::string expect_conditions = "((age = 10 or name = '3kx') or name = 'peersafe') or ((age = 30 or name = 'xxxkkk') and name = 'zxxx')";
				BEAST_EXPECT(result_conditions == expect_conditions);
			}
			else {
				BEAST_EXPECT(result2.first == 0);
			}
		}

		{
			// 嵌套 2 层
			std::string raw_string = "[{\"$or\": [{\"$and\": [{\"$or\": [{\"age\":10},{\"name\": \"3kx\"}]},{\"name\": \"peersafe\"}]},{\"$and\": [{\"$or\": [{\"age\":30},{\"name\": \"xxxkkk\"}]},{\"name\": \"zxxx\"}]}]}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			//BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			//BEAST_EXPECT(result2.first == 0);

			if (result2.first == 0) {
				std::string result_conditions = result.second.asString();
				std::string expect_conditions = "((age = 10 or name = '3kx') and name = 'peersafe') or ((age = 30 or name = 'xxxkkk') and name = 'zxxx')";
				BEAST_EXPECT(result_conditions == expect_conditions);
			}
			else {
				BEAST_EXPECT(result2.first == 0);
			}
		}

		{
			// 嵌套 3 层
			std::string raw_string = "[{\"$or\": [{\"$and\": [{\"$or\": [{\"$or\":[{\"id\":100},{\"age\":20},{\"name\":\"ll\"}]},{\"name\": \"3kx\"}]},{\"name\": \"peersafe\"}]},{\"id\":10}]}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			//BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			//BEAST_EXPECT(result2.first == 0);

			if (result2.first == 0) {
				std::string result_conditions = result.second.asString();
				std::string expect_conditions = "(((id = 100 or age = 20 or name = 'll') or name = '3kx') and name = 'peersafe') or id = 10";
				BEAST_EXPECT(result_conditions == expect_conditions);
			}
			else {
				BEAST_EXPECT(result2.first == 0);
			}
		}

		{
			// 嵌套 3 层
			std::string raw_string = "[{\"$or\": [{\"$and\": [{\"$or\": [{\"$and\":[{\"id\":100},{\"age\":20},{\"name\":\"ll\"}]},{\"name\": \"3kx\"}]},{\"name\": \"peersafe\"}]},{\"id\":10}]}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			//BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			//BEAST_EXPECT(result2.first == 0);

			if (result2.first == 0) {
				std::string result_conditions = result.second.asString();
				std::string expect_conditions = "(((id = 100 and age = 20 and name = 'll') or name = '3kx') and name = 'peersafe') or id = 10";
				BEAST_EXPECT(result_conditions == expect_conditions);
			}
			else {
				BEAST_EXPECT(result2.first == 0);
			}
		}

		{
			std::string raw_string = "[{\"$or\": [{\"$and\": [{\"$and\": [{\"$or\":[{\"id\":100},{\"age\":20},{\"name\":\"ll\"}]},{\"name\": \"3kx\"}]},{\"name\": \"peersafe\"}]},{\"id\":10}]}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			//BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			//BEAST_EXPECT(result2.first == 0);

			if (result2.first == 0) {
				std::string result_conditions = result.second.asString();
				std::string expect_conditions = "(((id = 100 or age = 20 or name = 'll') and name = '3kx') and name = 'peersafe') or id = 10";
				BEAST_EXPECT(result_conditions == expect_conditions);
			}
			else {
				BEAST_EXPECT(result2.first == 0);
			}
		}


		{
			// 嵌套 5 层
			std::string raw_string = "[{\"$or\": [{\"$and\": [{\"$or\": [{\"$or\":[{\"$and\":[{\"age\":36},{\"name\":\"zx\"}]},{\"id\":100},{\"age\":20},{\"name\":\"ll\"}]},{\"name\": \"3kx\"}]},{\"name\": \"peersafe\"}]},{\"id\":10}]}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			//BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			//BEAST_EXPECT(result2.first == 0);

			if (result2.first == 0) {
				std::string result_conditions = result.second.asString();
				std::string expect_conditions = "((((age = 36 and name = 'zx') or id = 100 or age = 20 or name = 'll') or name = '3kx') and name = 'peersafe') or id = 10";
				BEAST_EXPECT(result_conditions == expect_conditions);
			}
			else {
				BEAST_EXPECT(result2.first == 0);
			}
		}

		{
			// 嵌套 5 层
			std::string raw_string = "[{\"$or\": [{\"$and\": [{\"$or\": [{\"$or\":[{\"$and\":[{\"age\":36},{\"name\":\"zx\"}]},{\"id\":100},{\"age\":20},{\"name\":\"ll\"}]},{\"name\": \"3kx\"}]},{\"name\": \"peersafe\"}]},{\"$and\": [{\"$or\": [{\"$or\":[{\"$and\":[{\"age\":36},{\"name\":\"zx\"}]},{\"id\":100},{\"age\":20},{\"name\":\"ll\"}]},{\"name\": \"3kx\"}]},{\"name\": \"peersafe\"}]}]}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			//BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			//BEAST_EXPECT(result2.first == 0);

			if (result2.first == 0) {
				std::string result_conditions = result.second.asString();
				std::string expect_conditions = "((((age = 36 and name = 'zx') or id = 100 or age = 20 or name = 'll') or name = '3kx') and name = 'peersafe') or ((((age = 36 and name = 'zx') or id = 100 or age = 20 or name = 'll') or name = '3kx') and name = 'peersafe')";
				BEAST_EXPECT(result_conditions == expect_conditions);
			}
			else {
				BEAST_EXPECT(result2.first == 0);
			}
		}
	}

	void test_buildcondition() {

		std::string ops[] = { "$eq","$ne","$lt","$le","$gt","$ge" };
		std::string expect_ops[] = { "=", "!=", "<", "<=", ">", ">=" };
		for (size_t i = 0; i < 6; i++) {
			std::string raw_string = (boost::format("[{\"age\":{\"%1%\":20}}]") % ops[i]).str();
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}

			auto result = createConditionTree(conditions);
			if(result.first != 0)
				BEAST_EXPECT(result.first == 0);
			else {
				auto result2 = parse_conditions(conditions, result.second);
				if(result2.first != 0)
					BEAST_EXPECT(result2.first == 0);
				else {
					//LockedSociSession sql = txstore_dbconn_->GetDBConn()->checkoutDb();
					//auto t = *sql << "select * from user";
					auto t = result.second.asConditionString();
					std::string build_condition = t.second;
					//result.second.build(build_condition, t);
					std::string expect_conditions = (boost::format("age %1% :age") % expect_ops[i]).str();
					BEAST_EXPECT(build_condition == expect_conditions);
				}
			}
		}

		for (size_t i = 0; i < 6; i++) {
			std::string raw_string = (boost::format("[{\"age\":{\"%1%\":20}, \"name\":{\"%1%\":\"peersafe\"}}]")
				% ops[i]).str();
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}

			auto result = createConditionTree(conditions);
			if(result.first != 0)
				BEAST_EXPECT(result.first == 0);
			else {
				auto result2 = parse_conditions(conditions, result.second);
				if(result2.first != 0)
					BEAST_EXPECT(result2.first == 0);
				else {
					//LockedSociSession sql = txstore_dbconn_->GetDBConn()->checkoutDb();
					//auto t = *sql << "select * from user";
					auto t = result.second.asConditionString();
					std::string build_condition = t.second;
					//result.second.build(build_condition, t);
					std::string expect_conditions = (boost::format("age %1% :age and name %1% :name") % expect_ops[i]).str();
					BEAST_EXPECT(build_condition == expect_conditions);
				}
			}
		}


		for (size_t i = 0; i < 6; i++) {
			std::string raw_string = (boost::format("[{\"$or\":[{\"age\":{\"%1%\":20}}, {\"name\":{\"%1%\":\"peersafe\"}}]}]")
				% ops[i]).str();
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}

			auto result = createConditionTree(conditions);
			if (result.first != 0)
				BEAST_EXPECT(result.first == 0);
			else {
				auto result2 = parse_conditions(conditions, result.second);
				if (result2.first != 0)
					BEAST_EXPECT(result2.first == 0);
				else {
					//LockedSociSession sql = txstore_dbconn_->GetDBConn()->checkoutDb();
					//auto t = *sql << "select * from user";
					auto t = result.second.asConditionString();
					std::string build_condition = t.second;
					//result.second.build(build_condition, t);
					std::string expect_conditions = (boost::format("age %1% :age or name %1% :name") % expect_ops[i]).str();
					BEAST_EXPECT(build_condition == expect_conditions);
				}
			}
		}

		{
			std::string raw_string = "[{\"$or\":[{\"$and\":[{\"age\":{\"$ge\":20,\"$le\":60}}]},{\"$and\":[{\"id\":{\"$ge\":10,\"$le\":100}}]}]}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}

			auto result = createConditionTree(conditions);
			if (result.first != 0)
				BEAST_EXPECT(result.first == 0);
			else {
				auto result2 = parse_conditions(conditions, result.second);
				if (result2.first != 0)
					BEAST_EXPECT(result2.first == 0);
				else {
					//LockedSociSession sql = txstore_dbconn_->GetDBConn()->checkoutDb();
					//auto t = *sql << "select * from user";
					auto t = result.second.asConditionString();
					std::string build_condition = t.second;
					//result.second.build(build_condition, t);
					std::string expect_conditions = "(age >= :age and age <= :age) or (id >= :id and id <= :id)";
					BEAST_EXPECT(build_condition == expect_conditions);
					//BEAST_EXPECT(ret.second == expect_conditions);
				}
			}
		}

		{
			std::string raw_string = "[{\"age\":20}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			if(result.first != 0)
				BEAST_EXPECT(result.first == 0);
			else {
				auto result2 = parse_conditions(conditions, result.second);
				if(result2.first != 0)
					BEAST_EXPECT(result2.first == 0);
				else {
					//LockedSociSession sql = txstore_dbconn_->GetDBConn()->checkoutDb();
					//auto t = *sql << "select * from user";
					auto t = result.second.asConditionString();
					std::string build_condition = t.second;
					//result.second.build(build_condition, t);
					std::string expect_conditions = "age = :age";
					BEAST_EXPECT(build_condition == expect_conditions);
				}
			}
		}
	}


	void test_bugcrash() {
		// bug: RR-189
		{
			std::string raw_string = "[{\"$and\":[{\"$or\":[]}]}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			BEAST_EXPECT(result2.first != 0);

			if (result2.first == 0) {
				std::string result_conditions = result.second.asString();
				std::string expect_conditions = "((age = 10 or name = '3kx') and name = 'peersafe') or id = 1000";
				BEAST_EXPECT(result_conditions == expect_conditions);
			}
		}

		{
			std::string raw_string = "[{\"$and\":[{\"$or\":[]}, {\"name\":\"peersafe\"}]}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			BEAST_EXPECT(result2.first != 0);

			if (result2.first == 0) {
				std::string result_conditions = result.second.asString();
				std::string expect_conditions = "((age = 10 or name = '3kx') and name = 'peersafe') or id = 1000";
				BEAST_EXPECT(result_conditions == expect_conditions);
			}
		}

		{
			std::string raw_string = "[{\"$and\":[{\"$order\":[{\"id\" : -1}]},{\"id\":{\"$ge\":3}}]}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			BEAST_EXPECT(result2.first != 0);
		}

		{
			std::string raw_string = "[{\"$and\":[{},{}]}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			BEAST_EXPECT(result2.first != 0);
		}

		{
			std::string raw_string = "[{\"$and\":[{}]}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			BEAST_EXPECT(result2.first != 0);
		}

		{
			std::string raw_string = "[{\"$and\":\"fixhotbug\"}]";
			Json::Reader reader = Json::Reader();
			Json::Value conditions;
			if (reader.parse(raw_string, conditions) == false) {
				std::cout << "parse error. " << reader.getFormatedErrorMessages() << std::endl;
				return;
			}
			auto result = createConditionTree(conditions);
			BEAST_EXPECT(result.first == 0);

			auto result2 = parse_conditions(conditions, result.second);
			BEAST_EXPECT(result2.first != 0);
		}
	}

	void test_mongodb_json_style() {
		test_sample_mongodb_json_style();
		test_logicalAnd();
		test_logicalOr();
		test_keywork();
		test_logicRecursion();
		test_bugcrash();

		test_buildcondition();
	}

	void run() {
		// init env
		init_env();

		test_fixbug_RR207();
		test_crashfix_on_select();

		//test_CreateTableForeginTransaction();
		test_CreateTableTransaction();
		test_InsertRecordTransaction();
		test_UpdateRecordTransaction();
		test_SelectRecord();

		test_assert_transaction();

		test_DeleteRecordTransaction();

		test_DropTableTransaction();

		test_mongodb_json_style();
		test_join_select();
		pass();
	}

private:
	void set_AccountID(STObject &obj) {
		std::string account_str = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";
		ripple::AccountID account;
		RPC::accountFromString(account, account_str, false);
		obj.setAccountID(sfAccount, account);
	}

	void set_OwnerID(STObject &obj) {
		std::string owner_str = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";
		ripple::AccountID owner;
		RPC::accountFromString(owner, owner_str, false);
		obj.setAccountID(sfOwner, owner);
	}

	void set_tables(STObject &obj) {
		ripple::uint160 hex_table = ripple::from_hex_text<ripple::uint160>(table_name_);
		std::string str("user");
		ripple::Blob blob;
		blob.assign(str.begin(), str.end());

		STObject table(sfTable);
		table.setFieldVL(sfTableName, blob);
		table.setFieldH160(sfNameInDB, hex_table);
		STArray tables;
		tables.push_back(table);
		obj.setFieldArray(sfTables, tables);
	}

	std::shared_ptr<TxStoreDBConn> txstore_dbconn_;
	std::shared_ptr<TxStore> txstore_;
	ripple::Config config_;
	std::string table_name_;
}; // class Transaction_test

BEAST_DEFINE_TESTSUITE_MANUAL(Transaction2Sql, app, ripple);

}	// namespace ripple
