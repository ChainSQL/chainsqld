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

namespace ripple {
class DataBaseConn_test : public beast::unit_test::suite {
public:
	DataBaseConn_test() {
		std::string strconf("E:\\work\\rippled\\build\\msvc.debug.nounity\\rippled.cfg");
		ripple::Config config;
		config.setup(strconf, true, false, false);
		setup_ = ripple::setup_SyncDatabaseCon(config);
	}

	void test_CreateTable() {
		ripple::DatabaseCon database(setup_, "ripple", NULL, 0);
		soci::session& session = database.getSession();
		session << "drop table if exists user";
		session << "create table if not exists user(id int primary key auto_increment, name varchar(32))";
	}

	void test_InsertRecord() {
		ripple::DatabaseCon database(setup_, "ripple", NULL, 0);
		soci::session& session = database.getSession();
		session << "insert into user (name) values ('peersafe')";
		session << "insert into user (name) values ('ripple')";

		int count = 0;
		session << "select count(*) from user", soci::into(count);
		BEAST_EXPECT(count == 2);
	}

	void test_UpdateRecord() {
		ripple::DatabaseCon database(setup_, "ripple", NULL, 0);
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
		ripple::DatabaseCon database(setup_, "ripple", NULL, 0);
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

	void run() {
		test_CreateTable();
		test_InsertRecord();
		test_UpdateRecord();
		test_SelectRecord();
		pass();
	}
private:
	ripple::DatabaseCon::Setup setup_;
}; // class DataBaseConn_test

BEAST_DEFINE_TESTSUITE_MANUAL(DataBaseConn, app, ripple);

}	// namespace ripple
