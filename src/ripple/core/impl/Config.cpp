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
#include <ripple/core/Config.h>
#include <ripple/core/ConfigSections.h>
#include <ripple/basics/contract.h>
#include <ripple/basics/Log.h>
#include <ripple/json/json_reader.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/SystemParameters.h>
#include <ripple/net/HTTPClient.h>
#include <ripple/beast/core/LexicalCast.h>
#include <beast/core/string.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/regex.hpp>
#include <fstream>
#include <iostream>
#include <iterator>

namespace ripple {

//
// TODO: Check permissions on config file before using it.
//

#define SECTION_DEFAULT_NAME    ""

IniFileSections
parseIniFile (std::string const& strInput, const bool bTrim)
{
    std::string strData (strInput);
    std::vector<std::string> vLines;
    IniFileSections secResult;

    // Convert DOS format to unix.
    boost::algorithm::replace_all (strData, "\r\n", "\n");

    // Convert MacOS format to unix.
    boost::algorithm::replace_all (strData, "\r", "\n");

    boost::algorithm::split (vLines, strData,
        boost::algorithm::is_any_of ("\n"));

    // Set the default Section name.
    std::string strSection  = SECTION_DEFAULT_NAME;

    // Initialize the default Section.
    secResult[strSection]   = IniFileSections::mapped_type ();

    // Parse each line.
    for (auto& strValue : vLines)
    {
        if (strValue.empty () || strValue[0] == '#')
        {
            // Blank line or comment, do nothing.
        }
        else if (strValue[0] == '[' && strValue[strValue.length () - 1] == ']')
        {
            // New Section.
            strSection              = strValue.substr (1, strValue.length () - 2);
            secResult.emplace(strSection, IniFileSections::mapped_type{});
        }
        else
        {
            // Another line for Section.
            if (bTrim)
                boost::algorithm::trim (strValue);

            if (!strValue.empty ())
                secResult[strSection].push_back (strValue);
        }
    }

    return secResult;
}

IniFileSections::mapped_type*
getIniFileSection (IniFileSections& secSource, std::string const& strSection)
{
    IniFileSections::iterator it;
    IniFileSections::mapped_type* smtResult;
    it  = secSource.find (strSection);
    if (it == secSource.end ())
        smtResult   = 0;
    else
        smtResult   = & (it->second);
    return smtResult;
}

bool getSingleSection (IniFileSections& secSource,
    std::string const& strSection, std::string& strValue, beast::Journal j)
{
    IniFileSections::mapped_type* pmtEntries =
        getIniFileSection (secSource, strSection);
    bool bSingle = pmtEntries && 1 == pmtEntries->size ();

    if (bSingle)
    {
        strValue    = (*pmtEntries)[0];
    }
    else if (pmtEntries)
    {
        JLOG (j.warn()) << boost::str (
            boost::format ("Section [%s]: requires 1 line not %d lines.") %
            strSection % pmtEntries->size ());
    }

    return bSingle;
}

//------------------------------------------------------------------------------
//
// Config (DEPRECATED)
//
//------------------------------------------------------------------------------

char const* const Config::configFileName = "chainsqld.cfg";
char const* const Config::databaseDirName = "db";
char const* const Config::validatorsFileName = "validators.txt";

static
std::string
getEnvVar (char const* name)
{
    std::string value;

    auto const v = getenv (name);

    if (v != nullptr)
        value = v;

    return value;
}

void Config::setupControl(bool bQuiet,
    bool bSilent, bool bStandalone)
{
    QUIET = bQuiet || bSilent;
    SILENT = bSilent;
    RUN_STANDALONE = bStandalone;
}

void Config::setup (std::string const& strConf, bool bQuiet,
    bool bSilent, bool bStandalone)
{
    boost::filesystem::path dataDir;
    std::string strDbPath, strConfFile;

    // Determine the config and data directories.
    // If the config file is found in the current working
    // directory, use the current working directory as the
    // config directory and that with "db" as the data
    // directory.

    setupControl(bQuiet, bSilent, bStandalone);

    strDbPath = databaseDirName;

    if (!strConf.empty())
        strConfFile = strConf;
    else
        strConfFile = configFileName;

    if (!strConf.empty ())
    {
        // --conf=<path> : everything is relative that file.
        CONFIG_FILE             = strConfFile;
        CONFIG_DIR              = boost::filesystem::absolute (CONFIG_FILE);
        CONFIG_DIR.remove_filename ();
        dataDir                 = CONFIG_DIR / strDbPath;
    }
    else
    {
        CONFIG_DIR              = boost::filesystem::current_path ();
        CONFIG_FILE             = CONFIG_DIR / strConfFile;
        dataDir                 = CONFIG_DIR / strDbPath;

        // Construct XDG config and data home.
        // http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html
        std::string strHome          = getEnvVar ("HOME");
        std::string strXdgConfigHome = getEnvVar ("XDG_CONFIG_HOME");
        std::string strXdgDataHome   = getEnvVar ("XDG_DATA_HOME");

        if (boost::filesystem::exists (CONFIG_FILE)
                // Can we figure out XDG dirs?
                || (strHome.empty () && (strXdgConfigHome.empty () || strXdgDataHome.empty ())))
        {
            // Current working directory is fine, put dbs in a subdir.
        }
        else
        {
            if (strXdgConfigHome.empty ())
            {
                // $XDG_CONFIG_HOME was not set, use default based on $HOME.
                strXdgConfigHome    = strHome + "/.config";
            }

            if (strXdgDataHome.empty ())
            {
                // $XDG_DATA_HOME was not set, use default based on $HOME.
                strXdgDataHome  = strHome + "/.local/share";
            }

            CONFIG_DIR  = strXdgConfigHome + "/" + systemName ();
            CONFIG_FILE = CONFIG_DIR / strConfFile;
            dataDir    = strXdgDataHome + "/" + systemName ();

            if (!boost::filesystem::exists (CONFIG_FILE))
            {
                CONFIG_DIR  = "/etc/opt/" + systemName ();
                CONFIG_FILE = CONFIG_DIR / strConfFile;
                dataDir = "/var/opt/" + systemName();
            }
        }
    }

    // Update default values
    load ();
    {
        // load() may have set a new value for the dataDir
        std::string const dbPath (legacy ("database_path"));
        if (!dbPath.empty ())
            dataDir = boost::filesystem::path (dbPath);
        else if (RUN_STANDALONE)
            dataDir.clear();
    }

    if (!dataDir.empty())
    {
        boost::system::error_code ec;
        boost::filesystem::create_directories(dataDir, ec);

        if (ec)
            Throw<std::runtime_error>(
                boost::str(boost::format("Can not create %s") % dataDir));

        legacy("database_path", boost::filesystem::absolute(dataDir).string());
    }

    HTTPClient::initializeSSLContext(*this, j_);

    if (RUN_STANDALONE)
        LEDGER_HISTORY = 0;
}

void Config::load ()
{
    // NOTE: this writes to cerr because we want cout to be reserved
    // for the writing of the json response (so that stdout can be part of a
    // pipeline, for instance)
    if (!QUIET)
        std::cerr << "Loading: " << CONFIG_FILE << "\n";

    std::ifstream ifsConfig (CONFIG_FILE.c_str (), std::ios::in);

    if (!ifsConfig)
    {
        std::cerr << "Failed to open '" << CONFIG_FILE << "'." << std::endl;
        return;
    }

    std::string fileContents;
    fileContents.assign ((std::istreambuf_iterator<char>(ifsConfig)),
                          std::istreambuf_iterator<char>());

    if (ifsConfig.bad ())
    {
        std::cerr << "Failed to read '" << CONFIG_FILE << "'." << std::endl;
        return;
    }

    loadFromString (fileContents);
}

void Config::loadFromString (std::string const& fileContents)
{
    IniFileSections secConfig = parseIniFile (fileContents, true);

    build (secConfig);

    if (auto s = getIniFileSection (secConfig, SECTION_IPS))
        IPS = *s;

    if (auto s = getIniFileSection (secConfig, SECTION_IPS_FIXED))
        IPS_FIXED = *s;

    if (auto s = getIniFileSection (secConfig, SECTION_SNTP))
        SNTP_SERVERS = *s;


	if (auto s = getIniFileSection(secConfig, SECTION_PATH_X509)) {

		auto const vecCrtPath = *s;
		std::set<std::string> setRootCert;
		for (auto path : vecCrtPath) {

			std::string rootCert;
			std::ifstream ifsPath(path.c_str());
			rootCert.assign(
				std::istreambuf_iterator<char>(ifsPath),
				std::istreambuf_iterator<char>());

			if (rootCert.empty() || setRootCert.count(rootCert) !=0 )
				continue;

			setRootCert.insert(rootCert);
			ROOT_CERTIFICATES.push_back(rootCert);
		}

	}
		

    {
        std::string dbPath;
        if (getSingleSection (secConfig, "database_path", dbPath, j_))
        {
            boost::filesystem::path p(dbPath);
            legacy("database_path",
                   boost::filesystem::absolute (p).string ());
        }
    }

    std::string strTemp;

    if (getSingleSection (secConfig, SECTION_PEER_PRIVATE, strTemp, j_))
        PEER_PRIVATE = beast::lexicalCastThrow <bool> (strTemp);

    if (getSingleSection (secConfig, SECTION_PEERS_MAX, strTemp, j_))
        PEERS_MAX = std::max (0, beast::lexicalCastThrow <int> (strTemp));

    if (getSingleSection (secConfig, SECTION_NODE_SIZE, strTemp, j_))
    {
        if (beast::detail::iequals(strTemp, "tiny"))
            NODE_SIZE = 0;
        else if (beast::detail::iequals(strTemp, "small"))
            NODE_SIZE = 1;
        else if (beast::detail::iequals(strTemp, "medium"))
            NODE_SIZE = 2;
        else if (beast::detail::iequals(strTemp, "large"))
            NODE_SIZE = 3;
        else if (beast::detail::iequals(strTemp, "huge"))
            NODE_SIZE = 4;
        else
        {
            NODE_SIZE = beast::lexicalCastThrow <int> (strTemp);

            if (NODE_SIZE < 0)
                NODE_SIZE = 0;
            else if (NODE_SIZE > 4)
                NODE_SIZE = 4;
        }
    }

    if (getSingleSection (secConfig, SECTION_ELB_SUPPORT, strTemp, j_))
        ELB_SUPPORT         = beast::lexicalCastThrow <bool> (strTemp);

    if (getSingleSection (secConfig, SECTION_WEBSOCKET_PING_FREQ, strTemp, j_))
        WEBSOCKET_PING_FREQ = std::chrono::seconds{beast::lexicalCastThrow <int>(strTemp)};

    getSingleSection (secConfig, SECTION_SSL_VERIFY_FILE, SSL_VERIFY_FILE, j_);
    getSingleSection (secConfig, SECTION_SSL_VERIFY_DIR, SSL_VERIFY_DIR, j_);

    if (getSingleSection (secConfig, SECTION_SSL_VERIFY, strTemp, j_))
        SSL_VERIFY          = beast::lexicalCastThrow <bool> (strTemp);

    if (exists(SECTION_VALIDATION_SEED) && exists(SECTION_VALIDATOR_TOKEN))
        Throw<std::runtime_error> (
            "Cannot have both [" SECTION_VALIDATION_SEED "] "
            "and [" SECTION_VALIDATOR_TOKEN "] config sections");

    if (getSingleSection (secConfig, SECTION_NETWORK_QUORUM, strTemp, j_))
        NETWORK_QUORUM      = beast::lexicalCastThrow <std::size_t> (strTemp);

    if (getSingleSection (secConfig, SECTION_FEE_ACCOUNT_RESERVE, strTemp, j_))
        FEE_ACCOUNT_RESERVE = beast::lexicalCastThrow <std::uint64_t> (strTemp);

    if (getSingleSection (secConfig, SECTION_FEE_OWNER_RESERVE, strTemp, j_))
        FEE_OWNER_RESERVE   = beast::lexicalCastThrow <std::uint64_t> (strTemp);

    if (getSingleSection (secConfig, SECTION_FEE_OFFER, strTemp, j_))
        FEE_OFFER           = beast::lexicalCastThrow <int> (strTemp);

    if (getSingleSection (secConfig, SECTION_FEE_DEFAULT, strTemp, j_))
        FEE_DEFAULT         = beast::lexicalCastThrow <int> (strTemp);

    if (getSingleSection (secConfig, SECTION_LEDGER_HISTORY, strTemp, j_))
    {
        if (beast::detail::iequals(strTemp, "full"))
            LEDGER_HISTORY = 1000000000u;
        else if (beast::detail::iequals(strTemp, "none"))
            LEDGER_HISTORY = 0;
        else
            LEDGER_HISTORY = beast::lexicalCastThrow <std::uint32_t> (strTemp);
    }

    if (getSingleSection (secConfig, SECTION_FETCH_DEPTH, strTemp, j_))
    {
        if (beast::detail::iequals(strTemp, "none"))
            FETCH_DEPTH = 0;
        else if (beast::detail::iequals(strTemp, "full"))
            FETCH_DEPTH = 1000000000u;
        else
            FETCH_DEPTH = beast::lexicalCastThrow <std::uint32_t> (strTemp);

        if (FETCH_DEPTH < 10)
            FETCH_DEPTH = 10;
    }

    if (getSingleSection (secConfig, SECTION_PATH_SEARCH_OLD, strTemp, j_))
        PATH_SEARCH_OLD     = beast::lexicalCastThrow <int> (strTemp);
    if (getSingleSection (secConfig, SECTION_PATH_SEARCH, strTemp, j_))
        PATH_SEARCH         = beast::lexicalCastThrow <int> (strTemp);
    if (getSingleSection (secConfig, SECTION_PATH_SEARCH_FAST, strTemp, j_))
        PATH_SEARCH_FAST    = beast::lexicalCastThrow <int> (strTemp);
    if (getSingleSection (secConfig, SECTION_PATH_SEARCH_MAX, strTemp, j_))
        PATH_SEARCH_MAX     = beast::lexicalCastThrow <int> (strTemp);

    if (getSingleSection (secConfig, SECTION_DEBUG_LOGFILE, strTemp, j_))
        DEBUG_LOGFILE       = strTemp;

    if (getSingleSection (secConfig, SECTION_WORKERS, strTemp, j_))
        WORKERS      = beast::lexicalCastThrow <std::size_t> (strTemp);

    // shard related configuration items
    bool strongCheck = true;
    if (!section(SECTION_VALIDATOR_LIST_SITES).lines().empty() &&
        !section(SECTION_VALIDATOR_LIST_KEYS).lines().empty())
    {
        strongCheck = false;
    }

	const ripple::Section& shard = section("shard");

	std::pair<std::string, bool> role        = shard.find("role");
	std::pair<std::string, bool> shard_count = shard.find("shard_count");
	std::pair<std::string, bool> shard_index = shard.find("shard_index");

    // [shard].role
	if (role.second)
    {
		std::vector<std::string> vecRoles;
		boost::split(vecRoles, role.first, boost::is_any_of(","), boost::token_compress_on);

		for (auto item : vecRoles)
        {
			if      (item == std::string("lookup"))    SHARD_ROLE |= SHARD_ROLE_LOOKUP;
			else if (item == std::string("shard"))     SHARD_ROLE |= SHARD_ROLE_SHARD;
			else if (item == std::string("committee")) SHARD_ROLE |= SHARD_ROLE_COMMITTEE;
			else if (item == std::string("sync"))      SHARD_ROLE |= SHARD_ROLE_SYNC;
		}

        if (SHARD_ROLE & SHARD_ROLE_SHARD && SHARD_ROLE ^ SHARD_ROLE_SHARD)
        {
            Throw<std::runtime_error>("shard role cannot combine other roles!");
        }
        if (SHARD_ROLE & SHARD_ROLE_COMMITTEE && SHARD_ROLE ^ SHARD_ROLE_COMMITTEE)
        {
            Throw<std::runtime_error>("committee role cannot combine other roles!");
        }
	}
	if (SHARD_ROLE == SHARD_ROLE_UNDEFINED)
    {
		Throw<std::runtime_error>("the role must be set!");
	}

    // [shard].shard_count
	if (shard_count.second)
    {
		SHARD_COUNT = beast::lexicalCastThrow <std::size_t>(shard_count.first);
	}
    if (!SHARD_COUNT && strongCheck)
    {
        Throw<std::runtime_error>("shard_count at least 1!");
    }

    // [shard].shard_index
    if (SHARD_ROLE == SHARD_ROLE_COMMITTEE)
    {
        SHARD_INDEX = SHARD_INDEX_COMMITTEE;
    }
    else if (SHARD_ROLE == SHARD_ROLE_SHARD)
    {
        if (!shard_index.second)
        {
            Throw<std::runtime_error>("must specify the shard_index if I'm a shard node!");
        }
        SHARD_INDEX = beast::lexicalCastThrow <std::uint32_t>(shard_index.first);
        if (!SHARD_INDEX || (SHARD_INDEX > SHARD_COUNT && strongCheck))
        {
            Throw<std::runtime_error>("shard_index must be less or equal to shard_count and cannot be zero!");
        }
    }

    // [lookup_ips]
    if (auto s = getIniFileSection(secConfig, SECTION_LOOKUP_IPS))
    {
        LOOKUP_IPS = *s;
    }

    // [lookup_public_keys]
    if (auto s = getIniFileSection(secConfig, SECTION_LOOKUP_PUBLIC_KEYS))
    {
        LOOKUP_PUBLIC_KEYS = *s;
    }

    // [lookup_relay_interval]
    if (getSingleSection(secConfig, SECTION_LOOKUP_RELAY_INTERVAL, strTemp, j_))
    {
        LOOKUP_RELAY_INTERVAL = beast::lexicalCastThrow<std::uint32_t>(strTemp);
    }

    // [sync_ips]
    if (auto s = getIniFileSection(secConfig, SECTION_SYNC_IPS))
    {
        SYNC_IPS = *s;
    }

    // [shard_ips]
    if (auto s = getIniFileSection(secConfig, SECTION_SHARD_IPS))
    {
        std::vector<std::string> shardIPs = *s;
        for (auto IPs : shardIPs)
        {
            std::vector<std::string> vecIP;
            boost::split(vecIP, IPs, boost::is_any_of(","));
            SHARD_IPS.push_back(vecIP);
        }
    }

    // [shard_validators]
    if (auto s = getIniFileSection(secConfig, SECTION_SHARD_VALIDATORS))
    {
        std::vector<std::string> shardValidators = *s;
        for (auto validators : shardValidators)
        {
            std::vector<std::string> vecValidator;
            boost::split(vecValidator, validators, boost::is_any_of(","));
            SHARD_VALIDATORS.push_back(vecValidator);
        }
    }

    // [committee_ips]
    if (auto s = getIniFileSection(secConfig, SECTION_COMMITTEE_IPS))
    {
        COMMITTEE_IPS = *s;
    }

    // [committee_validators]
    if (auto s = getIniFileSection(secConfig, SECTION_COMMITTEE_VALIDATORS))
    {
        COMMITTEE_VALIDATORS = *s;
    }

	loadLookupConfig(secConfig);
	loadShardConfig(secConfig);
	loadCommitteeConfig(secConfig);
	loadSyncConfigConfig(secConfig);

	if (strongCheck && (SHARD_IPS.size() != SHARD_COUNT || SHARD_VALIDATORS.size() != SHARD_COUNT))
    {
		Throw<std::runtime_error>("shard_count must be equal to the number of shard_ips and shard_validators configuration items!");
	}

    if (strongCheck && (!SHARD_VALIDATORS.size() || !COMMITTEE_VALIDATORS.size()))
    {
        Throw<std::runtime_error>("shard_validators and committee_validators must be configured!");
    }

    if (isShardOrCommittee() && !LOOKUP_PUBLIC_KEYS.size())
    {
        Throw<std::runtime_error>("lookup_public_keys must be configured!");
    }

    // Do not load trusted validator configuration for standalone mode
    if (! RUN_STANDALONE)
    {
        // If a file was explicitly specified, then throw if the
        // path is malformed or if the file does not exist or is
        // not a file.
        // If the specified file is not an absolute path, then look
        // for it in the same directory as the config file.
        // If no path was specified, then look for validators.txt
        // in the same directory as the config file, but don't complain
        // if we can't find it.
        boost::filesystem::path validatorsFile;

        if (getSingleSection (secConfig, SECTION_VALIDATORS_FILE, strTemp, j_))
        {
            validatorsFile = strTemp;

            if (validatorsFile.empty ())
                Throw<std::runtime_error> (
                    "Invalid path specified in [" SECTION_VALIDATORS_FILE "]");

            if (!validatorsFile.is_absolute() && !CONFIG_DIR.empty())
                validatorsFile = CONFIG_DIR / validatorsFile;

            if (!boost::filesystem::exists (validatorsFile))
                Throw<std::runtime_error> (
                    "The file specified in [" SECTION_VALIDATORS_FILE "] "
                    "does not exist: " + validatorsFile.string());

            else if (!boost::filesystem::is_regular_file (validatorsFile) &&
                    !boost::filesystem::is_symlink (validatorsFile))
                Throw<std::runtime_error> (
                    "Invalid file specified in [" SECTION_VALIDATORS_FILE "]: " +
                    validatorsFile.string());
        }
        else if (!CONFIG_DIR.empty())
        {
            validatorsFile = CONFIG_DIR / validatorsFileName;

            if (!validatorsFile.empty ())
            {
                if(!boost::filesystem::exists (validatorsFile))
                    validatorsFile.clear();
                else if (!boost::filesystem::is_regular_file (validatorsFile) &&
                        !boost::filesystem::is_symlink (validatorsFile))
                    validatorsFile.clear();
            }
        }

        if (!validatorsFile.empty () &&
                boost::filesystem::exists (validatorsFile) &&
                (boost::filesystem::is_regular_file (validatorsFile) ||
                boost::filesystem::is_symlink (validatorsFile)))
        {
            std::ifstream ifsDefault (validatorsFile.native().c_str());

            std::string data;

            data.assign (
                std::istreambuf_iterator<char>(ifsDefault),
                std::istreambuf_iterator<char>());

            auto iniFile = parseIniFile (data, true);

            auto entries = getIniFileSection (
                iniFile,
                SECTION_VALIDATORS);

            if (entries)
                section (SECTION_VALIDATORS).append (*entries);

            auto valKeyEntries = getIniFileSection(
                iniFile,
                SECTION_VALIDATOR_KEYS);

            if (valKeyEntries)
                section (SECTION_VALIDATOR_KEYS).append (*valKeyEntries);

            auto valSiteEntries = getIniFileSection(
                iniFile,
                SECTION_VALIDATOR_LIST_SITES);

            if (valSiteEntries)
                section (SECTION_VALIDATOR_LIST_SITES).append (*valSiteEntries);

            auto valListKeys = getIniFileSection(
                iniFile,
                SECTION_VALIDATOR_LIST_KEYS);

            if (valListKeys)
                section (SECTION_VALIDATOR_LIST_KEYS).append (*valListKeys);

            if (!entries && !valKeyEntries && !valListKeys)
                Throw<std::runtime_error> (
                    "The file specified in [" SECTION_VALIDATORS_FILE "] "
                    "does not contain a [" SECTION_VALIDATORS "], "
                    "[" SECTION_VALIDATOR_KEYS "] or "
                    "[" SECTION_VALIDATOR_LIST_KEYS "]"
                    " section: " +
                    validatorsFile.string());
        }

        // Consolidate [validator_keys] and [validators]
        section (SECTION_VALIDATORS).append (
            section (SECTION_VALIDATOR_KEYS).lines ());

        if (! section (SECTION_VALIDATOR_LIST_SITES).lines().empty() &&
            section (SECTION_VALIDATOR_LIST_KEYS).lines().empty())
        {
            Throw<std::runtime_error> (
                "[" + std::string(SECTION_VALIDATOR_LIST_KEYS) +
                "] config section is missing");
        }
    }

    {
        auto const part = section("features");
        for(auto const& s : part.values())
        {
            if (auto const f = getRegisteredFeature(s))
                features.insert(*f);
            else
                Throw<std::runtime_error>(
                    "Unknown feature: " + s + "  in config file.");
        }
    }
}

bool Config::loadLookupConfig(IniFileSections& secConfig)
{
	bool bLoad = false;
	boost::filesystem::path lookupFile;
	std::string strTemp;
	if (getSingleSection(secConfig, SECTION_LOOKUP_FILE, strTemp, j_)){
		lookupFile = strTemp;
	}

	if (boost::filesystem::exists(lookupFile)) {

		std::ifstream ifsDefault(lookupFile.native().c_str());
		std::string data;

		data.assign(
			std::istreambuf_iterator<char>(ifsDefault),
			std::istreambuf_iterator<char>());

		auto iniFile = parseIniFile(data, true);

        if (auto s = getIniFileSection(iniFile, SECTION_LOOKUP_IPS))
        {
            if (LOOKUP_IPS.size())
            {
                Throw<std::runtime_error>("[lookup_ips] config ambiguous!");
            }
            LOOKUP_IPS = *s;
        }

        if (auto s = getIniFileSection(iniFile, SECTION_LOOKUP_PUBLIC_KEYS))
        {
            if (LOOKUP_PUBLIC_KEYS.size())
            {
                Throw<std::runtime_error>("[lookup_public_keys] config ambiguous!");
            }
            LOOKUP_PUBLIC_KEYS = *s;
        }

        if (getSingleSection(iniFile, SECTION_LOOKUP_RELAY_INTERVAL, strTemp, j_))
        {
            if (LOOKUP_RELAY_INTERVAL)
            {
                Throw<std::runtime_error>("[lookup_relay_interval] config ambiguous!");
            }
            LOOKUP_RELAY_INTERVAL = beast::lexicalCastThrow<std::uint32_t>(strTemp);
        }

		bLoad = true;
	}

	return bLoad;
}

bool Config::loadShardConfig(IniFileSections& secConfig)
{
	bool bLoad = false;
	boost::filesystem::path shardFile;
	std::string strTemp;
	if (getSingleSection(secConfig, SECTION_SHARD_FILE, strTemp, j_)) {
		shardFile = strTemp;
	}

	if (boost::filesystem::exists(shardFile)) {

		std::ifstream ifsDefault(shardFile.native().c_str());
		std::string data;

		data.assign(
			std::istreambuf_iterator<char>(ifsDefault),
			std::istreambuf_iterator<char>());

		auto iniFile = parseIniFile(data, true);

		std::vector<std::string>    shardIPs;
		if (auto s = getIniFileSection(iniFile, SECTION_SHARD_IPS))
        {
			shardIPs = *s;
            if (SHARD_IPS.size() && shardIPs.size())
            {
                Throw<std::runtime_error>("[shard_ips] config ambiguous!");
            }
			for (auto ip : shardIPs)
            {
				std::vector<std::string> vecIP;
				boost::split(vecIP, ip, boost::is_any_of(","));
				SHARD_IPS.push_back(vecIP);
			}
		}

		std::vector<std::string>    validators;
		if (auto s = getIniFileSection(iniFile, SECTION_SHARD_VALIDATORS))
        {
			validators = *s;
            if (SHARD_VALIDATORS.size() && validators.size())
            {
                Throw<std::runtime_error>("[shard_validators] config ambiguous!");
            }
			for (auto validator : validators)
            {
				std::vector<std::string> vecValidator;
				boost::split(vecValidator, validator, boost::is_any_of(","));
				SHARD_VALIDATORS.push_back(vecValidator);
			}
		}

		bLoad = true;
	}

	return bLoad;
}

bool Config::loadCommitteeConfig(IniFileSections& secConfig)
{

	bool bLoad = false;

	boost::filesystem::path committeeFile;
	std::string strTemp;
	if (getSingleSection(secConfig, SECTION_COMMITTEE_FILE, strTemp, j_)) {
		committeeFile = strTemp;
	}

	if (boost::filesystem::exists(committeeFile)) {

		std::ifstream ifsDefault(committeeFile.native().c_str());
		std::string data;

		data.assign(
			std::istreambuf_iterator<char>(ifsDefault),
			std::istreambuf_iterator<char>());

		auto iniFile = parseIniFile(data, true);

        if (auto s = getIniFileSection(iniFile, SECTION_COMMITTEE_IPS))
        {
            if (COMMITTEE_IPS.size())
            {
                Throw<std::runtime_error>("[committee_ips] config ambiguous!");
            }
            COMMITTEE_IPS = *s;
        }

        if (auto s = getIniFileSection(iniFile, SECTION_COMMITTEE_VALIDATORS))
        {
            if (COMMITTEE_VALIDATORS.size())
            {
                Throw<std::runtime_error>("[committee_validators] config ambiguous!");
            }
            COMMITTEE_VALIDATORS = *s;
        }

		bLoad = true;
	}

	return bLoad;
}

bool Config::loadSyncConfigConfig(IniFileSections& secConfig)
{

	bool bLoad = false;

	boost::filesystem::path syncFile;
	std::string strTemp;
	if (getSingleSection(secConfig, SECTION_SYNC_FILE, strTemp, j_)) {
		syncFile = strTemp;
	}

	if (boost::filesystem::exists(syncFile)) {

		std::ifstream ifsDefault(syncFile.native().c_str());
		std::string data;

		data.assign(
			std::istreambuf_iterator<char>(ifsDefault),
			std::istreambuf_iterator<char>());

		auto iniFile = parseIniFile(data, true);

        if (auto s = getIniFileSection(iniFile, SECTION_SYNC_IPS))
        {
            if (SYNC_IPS.size())
            {
                Throw<std::runtime_error>("[sync_ips] config ambiguous!");
            }
            SYNC_IPS = *s;
        }

		bLoad = true;
	}

	return bLoad;
}

void Config::getShardRelatedIps(std::vector<std::string>& ips)
{
	ips.insert(ips.end(), LOOKUP_IPS.begin(), LOOKUP_IPS.end());
	ips.insert(ips.end(), COMMITTEE_IPS.begin(), COMMITTEE_IPS.end());
	ips.insert(ips.end(), SYNC_IPS.begin(), SYNC_IPS.end());

    if (SHARD_ROLE == SHARD_ROLE_SHARD)
    {
        assert(SHARD_IPS.size() >= SHARD_INDEX);
        for (auto const& item : SHARD_IPS[SHARD_INDEX - 1])
        {
            ips.push_back(item);
        }
    }
    else
    {
        for (auto const& item : SHARD_IPS) {
            ips.insert(ips.end(), item.begin(), item.end());
        }
    }

	std::sort(ips.begin(), ips.end());
	ips.erase(std::unique(ips.begin(), ips.end()), ips.end());
}

std::string Config::shardRoleToString(std::uint32_t& shardRole)
{
	std::string roleString;
	if (shardRole & SHARD_ROLE_LOOKUP)       roleString  += "lookup,";
	if (shardRole & SHARD_ROLE_SHARD)        roleString  += "shard,";
	if (shardRole & SHARD_ROLE_SYNC)         roleString  += "sync,";
	if (shardRole & SHARD_ROLE_COMMITTEE)    roleString  += "committee,";

	// erase the last ','
	roleString.pop_back();

	return roleString;

}

int Config::getShardRole() const
{
	return SHARD_ROLE;
}

std::size_t Config::getShardIndex() const
{
	return SHARD_INDEX;
}

bool Config::isShardOrCommittee()
{
	return SHARD_ROLE == SHARD_ROLE_SHARD || SHARD_ROLE == SHARD_ROLE_COMMITTEE;
}

int Config::getSize (SizedItemName item) const
{
    SizedItem sizeTable[] =   //    tiny    small   medium  large       huge
    {

        { siSweepInterval,      {   10,     30,     60,     90,         120     } },

        { siLedgerFetch,        {   2,      3,      5,      5,          8       } },

        { siNodeCacheSize,      {   16384,  32768,  131072, 262144,     524288  } },
        { siNodeCacheAge,       {   60,     90,     120,    900,        1800    } },

        { siTreeCacheSize,      {   128000, 256000, 512000, 768000,     2048000 } },
        { siTreeCacheAge,       {   30,     60,     90,     120,        900     } },

        { siSLECacheSize,       {   4096,   8192,   16384,  65536,      131072  } },
        { siSLECacheAge,        {   30,     60,     90,     120,        300     } },

        { siLedgerSize,         {   32,     128,    256,    384,        768     } },
        { siLedgerAge,          {   30,     90,     180,    240,        900     } },

        { siTransactionSize,    {   65536,  131072, 196608, 262144,     327680  } },
        { siTransactionAge,     {   60,     90,     120,    900,        1800    } },

        { siHashNodeDBCache,    {   4,      12,     24,     64,         128     } },
        { siTxnDBCache,         {   4,      12,     24,     64,         128     } },
        { siLgrDBCache,         {   4,      8,      16,     32,         128     } },
    };

    for (int i = 0; i < (sizeof (sizeTable) / sizeof (SizedItem)); ++i)
    {
        if (sizeTable[i].item == item)
            return sizeTable[i].sizes[NODE_SIZE];
    }

    assert (false);
    return -1;
}

boost::filesystem::path Config::getDebugLogFile () const
{
    auto log_file = DEBUG_LOGFILE;

    if (!log_file.empty () && !log_file.is_absolute ())
    {
        // Unless an absolute path for the log file is specified, the
        // path is relative to the config file directory.
        log_file = boost::filesystem::absolute (
            log_file, CONFIG_DIR);
    }

    if (!log_file.empty ())
    {
        auto log_dir = log_file.parent_path ();

        if (!boost::filesystem::is_directory (log_dir))
        {
            boost::system::error_code ec;
            boost::filesystem::create_directories (log_dir, ec);

            // If we fail, we warn but continue so that the calling code can
            // decide how to handle this situation.
            if (ec)
            {
                std::cerr <<
                    "Unable to create log file path " << log_dir <<
                    ": " << ec.message() << '\n';
            }
        }
    }

    return log_file;
}

} // ripple
