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

#include <ripple/basics/Log.h>
#include <ripple/basics/contract.h>
#include <ripple/core/DatabaseCon.h>
#include <ripple/core/SociDB.h>

#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>

#include <memory>
#include <unordered_map>

#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

namespace ripple {

DatabaseCon::DatabaseCon(
	Setup const& setup,
	std::string const& strName,
	const char* initStrings[],
	int initCount, std::string sDBType):
	session_(std::make_shared<soci::session>())
{
	if (sDBType.compare("sqlite") == 0) {
		auto const useTempFiles  // Use temporary files or regular DB files?
			= setup.standAlone &&
			setup.startUp != Config::LOAD &&
			setup.startUp != Config::LOAD_FILE &&
			setup.startUp != Config::REPLAY;
		boost::filesystem::path pPath = useTempFiles
			? "" : (setup.dataDir / strName);

		open(*session_, "sqlite", pPath.string());
	}
	else {
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
		open(*session_, back_end, connectionstring);
		if (boost::iequals(back_end, "mycat")) {
			session_->autocommit_after_transaction(true);
		}
		if (strName.empty() == false) {
			std::string use_database = "use " + strName;
			soci::statement st = session_->prepare << use_database;
			st.execute(true);
		}
	}
	for (int i = 0; i < initCount; ++i)
	{
		try
		{
			soci::statement st = session_->prepare <<
				initStrings[i];
			st.execute(true);
		}
		catch (soci::soci_error&)
		{
			// ignore errors
		}
	}
}
class CheckpointersCollection
{
    std::uintptr_t nextId_{0};
    // Mutex protects the CheckpointersCollection
    std::mutex mutex_;
    // Each checkpointer is given a unique id. All the checkpointers that are
    // part of a DatabaseCon are part of this collection. When the DatabaseCon
    // is destroyed, its checkpointer is removed from the collection
    std::unordered_map<std::uintptr_t, std::shared_ptr<Checkpointer>>
        checkpointers_;

public:
    std::shared_ptr<Checkpointer>
    fromId(std::uintptr_t id)
    {
        std::lock_guard l{mutex_};
        auto it = checkpointers_.find(id);
        if (it != checkpointers_.end())
            return it->second;
        return {};
    }

    void
    erase(std::uintptr_t id)
    {
        std::lock_guard lock{mutex_};
        checkpointers_.erase(id);
    }

    std::shared_ptr<Checkpointer>
    create(
        std::shared_ptr<soci::session> const& session,
        JobQueue& jobQueue,
        JobCounter& counter,
        Logs& logs)
    {
        std::lock_guard lock{mutex_};
        auto const id = nextId_++;
        auto const r = makeCheckpointer(id, session, jobQueue, counter, logs);
        checkpointers_[id] = r;
        return r;
    }
};

CheckpointersCollection checkpointers;

std::shared_ptr<Checkpointer>
checkpointerFromId(std::uintptr_t id)
{
    return checkpointers.fromId(id);
}

DatabaseCon::~DatabaseCon()
{
    if (checkpointer_)
    {
        checkpointers.erase(checkpointer_->id());
    }
}

DatabaseCon::Setup
setup_DatabaseCon(Config const& c, boost::optional<beast::Journal> j)
{
    DatabaseCon::Setup setup;

    setup.startUp = c.START_UP;
    setup.standAlone = c.standalone();
    setup.dataDir = c.legacy("database_path");
    if (!setup.standAlone && setup.dataDir.empty())
    {
        Throw<std::runtime_error>("database_path must be set.");
    }

    if (!setup.globalPragma)
    {
        setup.globalPragma = [&c, &j]() {
            auto const& sqlite = c.section("sqlite");
            auto result = std::make_unique<std::vector<std::string>>();
            result->reserve(3);

            // defaults
            std::string safety_level;
            std::string journal_mode = "wal";
            std::string synchronous = "normal";
            std::string temp_store = "file";
            bool showRiskWarning = false;

            if (set(safety_level, "safety_level", sqlite))
            {
                if (boost::iequals(safety_level, "low"))
                {
                    // low safety defaults
                    journal_mode = "memory";
                    synchronous = "off";
                    temp_store = "memory";
                    showRiskWarning = true;
                }
                else if (!boost::iequals(safety_level, "high"))
                {
                    Throw<std::runtime_error>(
                        "Invalid safety_level value: " + safety_level);
                }
            }

            {
                // #journal_mode Valid values : delete, truncate, persist,
                // memory, wal, off
                if (set(journal_mode, "journal_mode", sqlite) &&
                    !safety_level.empty())
                {
                    Throw<std::runtime_error>(
                        "Configuration file may not define both "
                        "\"safety_level\" and \"journal_mode\"");
                }
                bool higherRisk = boost::iequals(journal_mode, "memory") ||
                    boost::iequals(journal_mode, "off");
                showRiskWarning = showRiskWarning || higherRisk;
                if (higherRisk || boost::iequals(journal_mode, "delete") ||
                    boost::iequals(journal_mode, "truncate") ||
                    boost::iequals(journal_mode, "persist") ||
                    boost::iequals(journal_mode, "wal"))
                {
                    result->emplace_back(boost::str(
                        boost::format(CommonDBPragmaJournal) % journal_mode));
                }
                else
                {
                    Throw<std::runtime_error>(
                        "Invalid journal_mode value: " + journal_mode);
                }
            }

            {
                //#synchronous Valid values : off, normal, full, extra
                if (set(synchronous, "synchronous", sqlite) &&
                    !safety_level.empty())
                {
                    Throw<std::runtime_error>(
                        "Configuration file may not define both "
                        "\"safety_level\" and \"synchronous\"");
                }
                bool higherRisk = boost::iequals(synchronous, "off");
                showRiskWarning = showRiskWarning || higherRisk;
                if (higherRisk || boost::iequals(synchronous, "normal") ||
                    boost::iequals(synchronous, "full") ||
                    boost::iequals(synchronous, "extra"))
                {
                    result->emplace_back(boost::str(
                        boost::format(CommonDBPragmaSync) % synchronous));
                }
                else
                {
                    Throw<std::runtime_error>(
                        "Invalid synchronous value: " + synchronous);
                }
            }

            {
                // #temp_store Valid values : default, file, memory
                if (set(temp_store, "temp_store", sqlite) &&
                    !safety_level.empty())
                {
                    Throw<std::runtime_error>(
                        "Configuration file may not define both "
                        "\"safety_level\" and \"temp_store\"");
                }
                bool higherRisk = boost::iequals(temp_store, "memory");
                showRiskWarning = showRiskWarning || higherRisk;
                if (higherRisk || boost::iequals(temp_store, "default") ||
                    boost::iequals(temp_store, "file"))
                {
                    result->emplace_back(boost::str(
                        boost::format(CommonDBPragmaTemp) % temp_store));
                }
                else
                {
                    Throw<std::runtime_error>(
                        "Invalid temp_store value: " + temp_store);
                }
            }

            if (showRiskWarning && j && c.LEDGER_HISTORY > SQLITE_TUNING_CUTOFF)
            {
                JLOG(j->warn())
                    << "reducing the data integrity guarantees from the "
                       "default [sqlite] behavior is not recommended for "
                       "nodes storing large amounts of history, because of the "
                       "difficulty inherent in rebuilding corrupted data.";
            }
            assert(result->size() == 3);
            return result;
        }();
    }
    setup.useGlobalPragma = true;

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
    if (result.second == false || result.first.empty() ||
        result.first.compare("sqlite") == 0)
    {
        setup.dataDir = c.legacy("database_path");
        if (!setup.standAlone && setup.dataDir.empty())
        {
            Throw<std::runtime_error>("database_path must be set.");
        }
    }

    return setup;
}

std::unique_ptr<std::vector<std::string> const>
    DatabaseCon::Setup::globalPragma;

void
DatabaseCon::setupCheckpointing(JobQueue* q, JobCounter* c, Logs& l)
{
    if (!q)
        Throw<std::logic_error>("No JobQueue");
    checkpointer_ = checkpointers.create(session_, *q, *c, l);
}

}  // namespace ripple
