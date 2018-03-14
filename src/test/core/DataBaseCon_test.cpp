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

#include <iostream>
#include <memory>

#include <ripple/beast/unit_test.h>
#include <ripple/core/DatabaseCon.h>
#include <ripple/core/Config.h>
#include <soci/mysql/soci-mysql.h>

namespace ripple {
class DataBaseConn_test : public beast::unit_test::suite {
public:

	void init_env() {
		std::string args = arg();
		size_t npos = args.find("conf=");
		if (npos == std::string::npos) {
			std::cout << "usage: chainsqld.exe --unittest=\"DataBaseConn\""
				<< "--unittest-arg=\"conf = chainsqld.cfg\"" << std::endl;
			exit(-1);
		}

		try {
			chainsql_conf_ = args.substr(npos + 4 + 1);
			std::cout << "conf = " << chainsql_conf_ << std::endl;

			ripple::Config config;
			config.setup(chainsql_conf_, true, false, false);
			setup_ = ripple::setup_SyncDatabaseCon(config);

			db_type_ = setup_.sync_db.find("type").first;
		}
		catch (const soci::soci_error& e) {
			exit(-1);
		}
		catch (...) {
			exit(-1);
		}
	}

	DataBaseConn_test() {

	}

	void test_CreateTable() {
		ripple::DatabaseCon database(setup_, "ripple", NULL, 0, db_type_);
		soci::session& session = database.getSession();
		session << "drop table user";
		session << "create table user(id int, name varchar(32))";
	}

	void test_InsertRecord() {
		ripple::DatabaseCon database(setup_, "ripple", NULL, 0, db_type_);
		soci::session& session = database.getSession();
		session << "insert into user (id,name) values (1,'peersafe')";
		session << "insert into user (id,name) values (2,'ripple')";

		int count = 0;
		session << "select count(*) from user", soci::into(count);
		BEAST_EXPECT(count == 2);
	}

	void test_UpdateRecord() {
		ripple::DatabaseCon database(setup_, "ripple", NULL, 0, db_type_);
		soci::session& session = database.getSession();
		std::string new_name = "new peersafe";
		int update_id = 1;
		//session << "update user set name = :name where id = :id",
		//		soci::use(new_name), soci::use(update_id);
		{
			auto t = session << "update user set name = :name where id = :id";
			//t = t, soci::use(new_name), soci::use(update_id);
			t = t, soci::use(new_name);
			t = t, soci::use(update_id);
		}
		
		std::string expected_name;
		session << "select name from user where id = :id",
				soci::into(expected_name), soci::use(update_id);
		BEAST_EXPECT(expected_name == new_name);
	}

	void test_SelectRecord() {
		ripple::DatabaseCon database(setup_, "ripple", NULL, 0, db_type_);
		soci::session& session = database.getSession();
		
		std::string expected_name[] = {"new peersafe", "ripple"};
		soci::rowset<soci::row> rs = (session.prepare << "select * from user");
		int id = 1;
		for (auto it = rs.begin(); it != rs.end(); it++) {
			soci::row& r = (*it);
			BEAST_EXPECT(r.get<int>(0) == id);
			BEAST_EXPECT(r.get<std::string>(1) == expected_name[id - 1]);
			id++;
		}
	}

	void test_transaction() {
		test_CreateTable();
		//ripple::DatabaseCon database(setup_, "ripple", NULL, 0, db_type_);
		soci::session session;
		session.open(soci::mysql, "host=chainSQL port=8066 user=root pass=3.16 db=ripple charset=utf8");
		{
			soci::transaction tr(session);
			try {
				session << "insert into user (id,name) values (1,'peersafe')";
				tr.rollback();
				//tr.commit();
			}
			catch (const soci::soci_error &e) {
			}
			catch (...) {
			}
		}


		session << "insert into user (id,name) values (2,'ripple')";
		session.commit();

		session << "insert into user (id,name) values (2,'ripple')";

		//soci::rowset<soci::row> rs = (session.prepare << "select * from user");
		//for (soci::rowset<soci::row>::iterator it = rs.begin(); it != rs.end(); ++it)
		//{
		//	const soci::row& row = *it;
		//	std::cout << "  id:" << row.get<int>(0) << std::endl;
		//}
	}

	void run() {
		init_env();
		//test_CreateTable();
		//test_InsertRecord();
		//test_UpdateRecord();
		//test_SelectRecord();
		test_transaction();

		pass();
	}
private:
	ripple::DatabaseCon::Setup setup_;
	std::string chainsql_conf_;
	std::string db_type_;
}; // class DataBaseConn_test

BEAST_DEFINE_TESTSUITE_MANUAL(DataBaseConn, app, ripple);

}	// namespace ripple
