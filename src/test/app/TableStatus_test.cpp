// ------------------------------------------------------------------------------
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
#include <ripple/protocol/AccountID.h>
#include <peersafe/app/sql/SQLConditionTree.h>
#include <boost/algorithm/string.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/format.hpp>
#include <test/jtx.h>
#include <iostream>
#include <peersafe/app/table/TableStatusDB.h>
#include <peersafe/app/table/TableStatusDBSQLite.h>
#include <peersafe/app/table/TableStatusDBMySQL.h>
#include <peersafe/app/sql/STTx2SQL.h>
#include <peersafe/app/sql/TxStore.h>

using namespace std;

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

	extern std::unique_ptr<ripple::Logs> logs_;

	class TableStatus_test : public beast::unit_test::suite
	{
	public:
		void run()
		{
			init_env();
			initDB();
			testInsertSyncDB();
			testReadSyncDB();
			testUpdateSyncDB();
			testGetMaxTxnInfo();
			testIs();
			testRecord();
			testGetAutoSync();
		}
		TableStatus_test() :
			txstore_dbconn_(nullptr)
			, txstore_(nullptr)
			, config_()
			, table_name_("1c2f5e6095324e2e08838f221a72ab4f") {
			logs_ = std::make_unique<SuiteLogs>(*this);
		}
		~TableStatus_test() {
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

				boost::optional<AccountID> account = parseBase58<AccountID>("rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh");
				if (account) {
					account_ = *account;
				}
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

		void initDB() {
			DatabaseCon::Setup setup = ripple::setup_SyncDatabaseCon(config_);
			std::pair<std::string, bool> result = setup.sync_db.find("type");

			beast::Journal j;
			if (result.first.compare("sqlite") == 0)
                m_pTableStatusDB = std::make_unique<TableStatusDBSQLite>(txstore_dbconn_->GetDBConn(), (ripple::Application*)this, j);				
			else
                m_pTableStatusDB = std::make_unique<TableStatusDBMySQL>(txstore_dbconn_->GetDBConn(), (ripple::Application*)this, j);
				
			if (m_pTableStatusDB)
			{
				m_pTableStatusDB->InitDB(setup);
			}
		}

		void testInsertSyncDB()
		{
			m_pTableStatusDB->InsertSnycDB("hello", "t_abcde", to_string(account_), 100, beast::zero, 1, "500000");
			m_pTableStatusDB->InsertSnycDB("hello2", "t_abcde2", to_string(account_), 100, beast::zero, 0, "500000");
			m_pTableStatusDB->InsertSnycDB("hello3", "t_abcde3", to_string(account_), 10000, beast::zero, 1, "500000");
		}

		void testReadSyncDB() {
			LedgerIndex TxnLedgerSeq = 0, LedgerSeq = 1;
			uint256 TxnLedgerHash, LedgerHash, TxnUpdateHash;
			bool deleted = 0;
			m_pTableStatusDB->ReadSyncDB("t_abcde", TxnLedgerSeq, TxnLedgerHash, LedgerSeq, LedgerHash, TxnUpdateHash, deleted);
			JLOG(logs_->journal("Transaction2Sql").info()) << "TxnLedgerSeq:" << TxnLedgerSeq << " TxnLedgerHash:" << TxnLedgerHash;
		}

		void testUpdateSyncDB() {
			m_pTableStatusDB->UpdateSyncDB(to_string(account_), "t_abcde",
				"A4E0570F7E106B3D97C764F66C1D330F0CAA083CC499F015BFABAC80D91F34A8", "1000", "025727FDA2BCB928F8D0E6EF87188C6D62AFD4B50CBBBDAF172DF8230F2B7955",
				"1005", "00000", "6000000", "1111");

			//bool UpdateSyncDB(AccountID accountID, std::string TableName, std::string TableNameInDB);
			m_pTableStatusDB->UpdateSyncDB(account_, "hello", "t_abcdef");

			//bool UpdateSyncDB(const std::string &Owner, const std::string &TableNameInDB,bool bDel, const std::string &PreviousCommit);
			m_pTableStatusDB->UpdateSyncDB(to_string(account_), "t_abcdef", true, "22222");

			//    bool UpdateSyncDB(const std::string &Owner, const std::string &TableNameInDB,
			//		const std::string &LedgerHash, const std::string &LedgerSeq, const std::string &PreviousCommit);
			m_pTableStatusDB->UpdateSyncDB(to_string(account_), "t_abcdef", "6B4873B86A2B971B18713D32A507DED67020A735A034ACA6A1C6D46030A4F3E5", "1228", "3333");

			//    bool UpdateSyncDB(const std::string &Owner, const std::string &TableNameInDB,
			//		const std::string &TxnUpdateHash, const std::string &PreviousCommit);
			m_pTableStatusDB->UpdateSyncDB(to_string(account_), "t_abcdef", std::string("29F28C9BF852DBC9134C2D3B4C4B8D2E11930A0A1BB2D405D024469493303A68"), "44444");

		}

		void testGetMaxTxnInfo()
		{
			//bool GetMaxTxnInfo(std::string TableName, std::string Owner, LedgerIndex &TxnLedgerSeq, uint256 &TxnLedgerHash);
			LedgerIndex txnLedgerSeq;
			uint256 txnLedgerHash;

			bool bRet = m_pTableStatusDB->GetMaxTxnInfo("hello", to_string(account_), txnLedgerSeq, txnLedgerHash);
			JLOG(logs_->journal("TableStatus").info()) << "TxnLedgerSeq:" << txnLedgerSeq << " TxnLedgerHash:" << txnLedgerHash;
		}

		void testIs()
		{
			//bool isNameInDBExist(std::string TableName, std::string Owner, bool delCheck, std::string &TableNameInDB);
			std::string nameInDB;
			bool bRet = m_pTableStatusDB->isNameInDBExist("hijack", to_string(account_),true, nameInDB);
			if (bRet)
				JLOG(logs_->journal("TableStatus").info())<<"NameInDBExist";
			bRet = m_pTableStatusDB->IsExist(account_, "t_abcdef");
		}

		void testRecord()
		{
			bool bRet = m_pTableStatusDB->RenameRecord(account_, "t_abcdef", "hallo");
			bRet = m_pTableStatusDB->DeleteRecord(account_, "hello3");
		}


		void testGetAutoSync() {
			//bool GetAutoListFromDB(bool bAutoSunc, std::list<std::tuple<std::string, std::string, std::string, bool> > &tuple);
			std::list<std::tuple<std::string, std::string, std::string, bool> > list;
			bool bRet = m_pTableStatusDB->GetAutoListFromDB(true, list);
			if (bRet)
			{
				std::string owner, tablename, time;
				bool isAutoSync;
				for (auto item : list)
				{
					std::tie(owner, tablename, time, isAutoSync) = item;
					JLOG(logs_->journal("TableStatus").info()) << "owner:"<<owner<<" tablename:"<<tablename;
				}
			}
		}

	private:
		std::unique_ptr <TableStatusDB> m_pTableStatusDB;
		std::shared_ptr<TxStoreDBConn> txstore_dbconn_;
		std::shared_ptr<TxStore> txstore_;
		ripple::Config config_;
		std::string table_name_;
		AccountID account_;
	};


	BEAST_DEFINE_TESTSUITE(TableStatus, app, ripple);

} // ripple
