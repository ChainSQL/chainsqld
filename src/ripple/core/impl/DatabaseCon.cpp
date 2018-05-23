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

#include <BeastConfig.h>
#include <ripple/core/DatabaseCon.h>
#include <ripple/core/SociDB.h>
#include <ripple/basics/contract.h>
#include <ripple/basics/Log.h>
#include <memory>

#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

namespace ripple {

DatabaseCon::DatabaseCon (
    Setup const& setup,
    std::string const& strName,
    const char* initStrings[],
    int initCount, std::string sDBType)
{	
	if (sDBType.compare("sqlite") == 0) {
        auto const useTempFiles  // Use temporary files or regular DB files?
            = setup.standAlone &&
            setup.startUp != Config::LOAD &&
            setup.startUp != Config::LOAD_FILE &&
            setup.startUp != Config::REPLAY;
        boost::filesystem::path pPath = useTempFiles
            ? "" : (setup.dataDir / strName);

        open(session_, "sqlite", pPath.string());
	} else {  
        //connect to mycat server 
        std::pair<std::string, bool> type = setup.sync_db.find("type");
		std::string back_end;
		if (type.second) {
			back_end = type.first;
		}
		if (back_end.empty()) {
			Throw<std::runtime_error>("configuration error: type must be specified in sync_db.");
			return;
		}

        std::pair<std::string, bool> host = setup.sync_db.find("host");
        std::pair<std::string, bool> port = setup.sync_db.find("port");
        std::pair<std::string, bool> user = setup.sync_db.find("user");
        std::pair<std::string, bool> pwd = setup.sync_db.find("pass");
        std::pair<std::string, bool> db = setup.sync_db.find("db");
        std::pair<std::string, bool> unix_socket = setup.sync_db.find("unix_socket");
        std::pair<std::string, bool> ssl_ca = setup.sync_db.find("ssl_ca");
        std::pair<std::string, bool> ssl_cert = setup.sync_db.find("ssl_cert");
        std::pair<std::string, bool> ssl_key = setup.sync_db.find("ssl_key");
        std::pair<std::string, bool> local_infile = setup.sync_db.find("local_infile");
        std::pair<std::string, bool> charset = setup.sync_db.find("charset");

        std::string connectionstring;

        if (host.second)
            connectionstring += " host = " + host.first;
        if (port.second)
            connectionstring += " port = " + port.first;
        if (user.second)
            connectionstring += " user = " + user.first;
        if (pwd.second)
            connectionstring += " pass = " + pwd.first;
        if (db.second)
            connectionstring += " db = " + db.first;
        if (unix_socket.second)
            connectionstring += " unix_socket = " + unix_socket.first;
        if (ssl_ca.second)
            connectionstring += " sslca = " + ssl_ca.first;
        if (ssl_cert.second)
            connectionstring += " sslcert = " + ssl_cert.first;
        if (ssl_key.second)
            connectionstring += " sslkey = " + ssl_key.first;
        if (local_infile.second)
            connectionstring += " local_infile = " + local_infile.first;
        if (charset.second)
            connectionstring += " charset = " + charset.first;
		
		if (connectionstring.empty()) {
			Throw<std::runtime_error>("configuration error: connection string is empty.");
			return;
		}

        open(session_, back_end, connectionstring);
		if (boost::iequals(back_end, "mycat")) {
			session_.autocommit_after_transaction(true);
		}
        if (strName.empty() == false) {
            std::string use_database = "use " + strName;
            soci::statement st = session_.prepare << use_database;
            st.execute(true);
        }
	}
	for (int i = 0; i < initCount; ++i)
	{
		try
		{
			soci::statement st = session_.prepare <<
				initStrings[i];
			st.execute(true);
		}
		catch (soci::soci_error&)
		{
			// ignore errors
		}
	}
}

DatabaseCon::Setup setup_DatabaseCon (Config const& c)
{
	DatabaseCon::Setup setup;

	setup.startUp = c.START_UP;
	setup.standAlone = c.standalone();
	setup.dataDir = c.legacy("database_path");
	if (!setup.standAlone && setup.dataDir.empty())
	{
		Throw<std::runtime_error>(
			"database_path must be set.");
	}

	return setup;
}

DatabaseCon::Setup
setup_SyncDatabaseCon(Config const& c) 
{
    DatabaseCon::Setup setup;
	setup.startUp = c.START_UP;
    setup.standAlone = c.standalone();
	setup.sync_db = c["sync_db"];
	std::pair<std::string, bool> result = setup.sync_db.find("type");
	if (result.second == false
		|| result.first.empty() || result.first.compare("sqlite") == 0) {
		setup.dataDir = c.legacy("database_path");
		if (!setup.standAlone && setup.dataDir.empty())
		{
			Throw<std::runtime_error>(
				"database_path must be set.");
		}
	}

    return setup;
}

void DatabaseCon::setupCheckpointing (JobQueue* q, Logs& l)
{
    if (! q)
        Throw<std::logic_error> ("No JobQueue");
    checkpointer_ = makeCheckpointer (session_, *q, l);
}

} // ripple
