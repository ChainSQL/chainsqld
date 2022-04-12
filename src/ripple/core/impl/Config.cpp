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

#include <ripple/basics/FileUtilities.h>
#include <ripple/basics/Log.h>
#include <ripple/basics/contract.h>
#include <ripple/beast/core/LexicalCast.h>
#include <ripple/core/Config.h>
#include <ripple/core/ConfigSections.h>
#include <ripple/json/json_reader.h>
#include <ripple/net/HTTPClient.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/SystemParameters.h>
#include <ripple/protocol/CommonKey.h>
#include <peersafe/gmencrypt/GmEncryptObj.h>
#include <boost/algorithm/string.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/format.hpp>
#include <boost/regex.hpp>
#include <boost/system/error_code.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <iostream>
#include <iterator>

namespace ripple {

inline constexpr std::array<std::pair<SizedItem, std::array<int, 5>>, 13>
    sizedItems{{
        // FIXME: We should document each of these items, explaining exactly
        // what
        //        they control and whether there exists an explicit config
        //        option that can be used to override the default.
        {SizedItem::sweepInterval, {{10, 30, 60, 90, 120}}},
        {SizedItem::treeCacheSize, {{128000, 256000, 512000, 768000, 2048000}}},
        {SizedItem::treeCacheAge, {{30, 60, 90, 120, 900}}},
        {SizedItem::ledgerSize, {{32, 128, 256, 384, 768}}},
        {SizedItem::ledgerAge, {{30, 90, 180, 240, 900}}},
        {SizedItem::ledgerFetch, {{2, 3, 4, 5, 8}}},
        {SizedItem::nodeCacheSize, {{16384, 32768, 131072, 262144, 524288}}},
        {SizedItem::nodeCacheAge, {{60, 90, 120, 900, 1800}}},
        {SizedItem::hashNodeDBCache, {{4, 12, 24, 64, 128}}},
        {SizedItem::txnDBCache, {{4, 12, 24, 64, 128}}},
        {SizedItem::lgrDBCache, {{4, 8, 16, 32, 128}}},
        {SizedItem::transactionSize,    {{65536,  131072, 196608, 262144,     327680  }} },
        {SizedItem::transactionAge,     {{60,     90,     120,    900,        1800    }} }
    }};

// Ensure that the order of entries in the table corresponds to the
// order of entries in the enum:
static_assert(
    []() constexpr->bool {
        std::underlying_type_t<SizedItem> idx = 0;

        for (auto const& i : sizedItems)
        {
            if (static_cast<std::underlying_type_t<SizedItem>>(i.first) != idx)
                return false;

            ++idx;
        }

        return true;
    }(),
    "Mismatch between sized item enum & array indices");

//
// TODO: Check permissions on config file before using it.
//

#define SECTION_DEFAULT_NAME ""



IniFileSections
parseIniFile(std::string const& strInput, const bool bTrim)
{
    std::string strData(strInput);
    std::vector<std::string> vLines;
    IniFileSections secResult;

    // Convert DOS format to unix.
    boost::algorithm::replace_all(strData, "\r\n", "\n");

    // Convert MacOS format to unix.
    boost::algorithm::replace_all(strData, "\r", "\n");

    boost::algorithm::split(vLines, strData, boost::algorithm::is_any_of("\n"));

    // Set the default Section name.
    std::string strSection = SECTION_DEFAULT_NAME;

    // Initialize the default Section.
    secResult[strSection] = IniFileSections::mapped_type();

    // Parse each line.
    for (auto& strValue : vLines)
    {
        if (bTrim)
            boost::algorithm::trim(strValue);

        if (strValue.empty() || strValue[0] == '#')
        {
            // Blank line or comment, do nothing.
        }
        else if (strValue[0] == '[' && strValue[strValue.length() - 1] == ']')
        {
            // New Section.
            strSection = strValue.substr(1, strValue.length() - 2);
            secResult.emplace(strSection, IniFileSections::mapped_type{});
        }
        else
        {
            // Another line for Section.
            if (!strValue.empty())
                secResult[strSection].push_back(strValue);
        }
    }

    return secResult;
}

IniFileSections::mapped_type*
getIniFileSection(IniFileSections& secSource, std::string const& strSection)
{
    IniFileSections::iterator it;
    IniFileSections::mapped_type* smtResult;
    it = secSource.find(strSection);
    if (it == secSource.end())
        smtResult = nullptr;
    else
        smtResult = &(it->second);
    return smtResult;
}

bool
getSingleSection(
    IniFileSections& secSource,
    std::string const& strSection,
    std::string& strValue,
    beast::Journal j)
{
    IniFileSections::mapped_type* pmtEntries =
        getIniFileSection(secSource, strSection);
    bool bSingle = pmtEntries && 1 == pmtEntries->size();

    if (bSingle)
    {
        strValue = (*pmtEntries)[0];
    }
    else if (pmtEntries)
    {
        JLOG(j.warn()) << boost::str(
            boost::format("Section [%s]: requires 1 line not %d lines.") %
            strSection % pmtEntries->size());
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

static std::string
getEnvVar(char const* name)
{
    std::string value;

    auto const v = getenv(name);

    if (v != nullptr)
        value = v;

    return value;
}

constexpr FeeUnit32 Config::TRANSACTION_FEE_BASE;

void
Config::setupControl(bool bQuiet, bool bSilent, bool bStandalone)
{
    QUIET = bQuiet || bSilent;
    SILENT = bSilent;
    RUN_STANDALONE = bStandalone;
}

void
Config::setupStartUpType(Config::StartUpType type)
{
    START_UP = type;
}

void Config::onSchemaModify(Config& config,
    std::vector<std::string> validators,
    std::vector<std::string> peer_list)
{
    auto fileContents = config.getConfigFileContents();
    if (fileContents.empty()) {

        // error msg
        return;
    }

    // copy fileContents from config
    build(fileContents);

    deprecatedClearSection("validators");
    section("validators").append(validators);

    deprecatedClearSection("ips");
    section("ips").append(peer_list);

    CONFIG_FILE = config.CONFIG_DIR / "chainsqld.cfg";
    {
        std::ofstream outfile(CONFIG_FILE.generic_string());
        outfile << *this;
        outfile.close();
    }
}

void Config::initSchemaConfig(Config& config, SchemaParams const& schemaParams)
{
    {
        // test standalone
        START_VALID = true;
    }

    if (schemaParams.strategy == SchemaStragegy::with_state) {
        START_UP = NEWCHAIN_WITHSTATE;
    }

    CONFIG_DIR = boost::filesystem::path(config.SCHEMA_PATH) / to_string(schemaParams.schema_id);
    CONFIG_FILE = CONFIG_DIR / "chainsqld.cfg";
    if(boost::filesystem::exists(CONFIG_FILE) && schemaParams.fromLoad)
    {
        //In case the cfg has been modified.
        return;
    }

    if(!boost::filesystem::exists(CONFIG_DIR))
    {
        boost::system::error_code ec;
        boost::filesystem::create_directories(CONFIG_DIR, ec);

        if (ec)
            Throw<std::runtime_error>(
                boost::str(boost::format("Can not create %s") % CONFIG_DIR));
    }
    auto fileContents =  config.getConfigFileContents();
    if (fileContents.empty()) {
        // error msg
        return ;
    }

    // copy fileContents from config
    build(fileContents);

    std::string const node_db_type{ get<std::string>(section(ConfigSection::nodeDatabase()), "type") };

    // ./AB868A6CFEEC779C2FF845C0AF00A642259986AF40C01976A7F842B6918936C7/db/NuDB
    auto node_db_path = CONFIG_DIR / node_db_type;
    overwrite(ConfigSection::nodeDatabase(), "path", node_db_path.generic_string());

    auto database_path = CONFIG_DIR / "db";
    deprecatedClearSection("database_path");
    section("database_path").append(database_path.generic_string());
    //legacy("database_path", database_path.generic_string());

    deprecatedClearSection("ips");
    section("ips").append(schemaParams.peer_list);

    auto const& names = section("server").values();
    for (auto const& name : names)
    {
        if (exists(name))
            removeSection(name);
    }
    removeSection("server");
    removeSection("rpc_startup");
    removeSection("sntp_servers");
    removeSection("validation_seed");
    removeSection("debug_logfile");
    removeSection("schema");
    removeSection("ips_fixed");
    removeSection("sync_db");        //need separate configuration
    removeSection("auto_sync");      //need separate configuration
    removeSection("sync_tables");    //need separate configuration
    removeSection("prometheus");
    removeSection(SECTION_VALIDATOR_LIST_KEYS);
    removeSection(SECTION_VALIDATOR_LIST_SITES);

    // about certificate
    removeSection("trusted_ca_list");
    removeSection("peer_x509_root_path");
    removeSection("x509_crt_path");  //need separate configuration
    removeSection("ssl_verify");
    removeSection("cmd_ssl_cert");
    removeSection("ssl_verify_file");
    removeSection("ssl_verify_dir");

    deprecatedClearSection("validators");
    std::vector<std::string>   validatorList;
    for (auto item : schemaParams.validator_list) {
        std::string sPublicKey = toBase58(TokenType::NodePublic, item.first);
        validatorList.push_back(sPublicKey);
    }
    section("validators").append(validatorList);

    {
        std::ofstream outfile(CONFIG_FILE.generic_string());
        outfile << *this;
        outfile.close();
    }

    initSchemaInfo(CONFIG_DIR,schemaParams);
}

void Config::initSchemaInfo(boost::filesystem::path config_dir,SchemaParams const& schemaParams)
{
    // save schema_info
    boost::filesystem::path schema_info_file = config_dir / "schema_info";

    assert(schemaParams.validator_list.size() > 0);
    std::string sValidatorInfo;
    for (auto i = 0; i < schemaParams.validator_list.size(); i++) {

        if (i > 0) {
            sValidatorInfo += ";";
        }

        std::string sNodePublic = toBase58(TokenType::NodePublic, schemaParams.validator_list[i].first);
        std::string sValidate = schemaParams.validator_list[i].second ? "1" : "0";

        sValidatorInfo += (boost::format("%s %s") % sNodePublic % sValidate).str();
    }

    assert(schemaParams.peer_list.size() > 0);
    std::string sPeerListInfo;
    for (auto i = 0; i < schemaParams.peer_list.size(); i++) {

        if (i > 0) {
            sPeerListInfo += ";";
        }
        sPeerListInfo += schemaParams.peer_list[i];
    }

    std::ofstream infofile(schema_info_file.string());
    infofile << to_string(schemaParams.schema_id) << std::endl;
    infofile << schemaParams.schema_name << std::endl;
    infofile << to_string(schemaParams.account) << std::endl;
    infofile << (int)schemaParams.strategy << std::endl;
    infofile << to_string(schemaParams.anchor_ledger_hash) << std::endl;
    infofile << sValidatorInfo << std::endl;
    infofile << sPeerListInfo;

    infofile.close();
}

IniFileSections Config::getConfigFileContents() const
{
    IniFileSections fileContents;
    boost::system::error_code ec;
    auto const strFileContents = getFileContents(ec, CONFIG_FILE);
    if (ec)
    {
        std::cerr << "Failed to read '" << CONFIG_FILE << "'." <<
            ec.value() << ": " << ec.message() << std::endl;
        return fileContents;
    }

    fileContents = parseIniFile(strFileContents, true);
    return fileContents;
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

    if (!strConf.empty())
    {
        // --conf=<path> : everything is relative that file.
        CONFIG_FILE = strConfFile;
        CONFIG_DIR = boost::filesystem::absolute(CONFIG_FILE);
        CONFIG_DIR.remove_filename();
        dataDir = CONFIG_DIR / strDbPath;
    }
    else
    {
        CONFIG_DIR = boost::filesystem::current_path();
        CONFIG_FILE = CONFIG_DIR / strConfFile;
        dataDir = CONFIG_DIR / strDbPath;

        // Construct XDG config and data home.
        // http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html
        std::string strHome = getEnvVar("HOME");
        std::string strXdgConfigHome = getEnvVar("XDG_CONFIG_HOME");
        std::string strXdgDataHome = getEnvVar("XDG_DATA_HOME");

        if (boost::filesystem::exists(CONFIG_FILE)
            // Can we figure out XDG dirs?
            || (strHome.empty() &&
                (strXdgConfigHome.empty() || strXdgDataHome.empty())))
        {
            // Current working directory is fine, put dbs in a subdir.
        }
        else
        {
            if (strXdgConfigHome.empty())
            {
                // $XDG_CONFIG_HOME was not set, use default based on $HOME.
                strXdgConfigHome = strHome + "/.config";
            }

            if (strXdgDataHome.empty())
            {
                // $XDG_DATA_HOME was not set, use default based on $HOME.
                strXdgDataHome = strHome + "/.local/share";
            }

            CONFIG_DIR = strXdgConfigHome + "/" + systemName();
            CONFIG_FILE = CONFIG_DIR / strConfFile;
            dataDir = strXdgDataHome + "/" + systemName();

            if (!boost::filesystem::exists(CONFIG_FILE))
            {
                CONFIG_DIR = "/etc/opt/" + systemName();
                CONFIG_FILE = CONFIG_DIR / strConfFile;
                dataDir = "/var/opt/" + systemName();
            }
        }
    }

    // Update default values
    load();
    {
        // load() may have set a new value for the dataDir
        std::string const dbPath(legacy("database_path"));
        if (!dbPath.empty())
            dataDir = boost::filesystem::path(dbPath);
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

void
Config::load()
{
    // NOTE: this writes to cerr because we want cout to be reserved
    // for the writing of the json response (so that stdout can be part of a
    // pipeline, for instance)
    if (!QUIET)
        std::cerr << "Loading: " << CONFIG_FILE << "\n";

    boost::system::error_code ec;
    auto const fileContents = getFileContents(ec, CONFIG_FILE);

    if (ec)
    {
        std::cerr << "Failed to read '" << CONFIG_FILE << "'." << ec.value()
                  << ": " << ec.message() << std::endl;
        return;
    }

    loadFromString(fileContents);
}

void
Config::loadFromString(std::string const& fileContents)
{
    IniFileSections secConfig = parseIniFile(fileContents, true);

    build(secConfig);

    if (auto s = getIniFileSection(secConfig, SECTION_IPS))
        IPS = *s;

    if (auto s = getIniFileSection(secConfig, SECTION_IPS_FIXED))
        IPS_FIXED = *s;

    if (auto s = getIniFileSection(secConfig, SECTION_SNTP))
        SNTP_SERVERS = *s;

    if (auto s = getIniFileSection(secConfig, SECTOIN_TRUSTED_CA_LIST))
    {
        auto const vecCaList = *s;
        std::set<std::string> setCaList;
        for (auto caItem : vecCaList) {
            TRUSTED_CA_LIST.push_back(caItem);
        }
    }

    if (auto s = getIniFileSection(secConfig, SECTION_USER_X509_ROOT_PATH))
    {

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
            USER_ROOT_CERTIFICATES.push_back(rootCert);
        }

    }

    if (auto s = getIniFileSection(secConfig, SECTION_SCHEMAS)) 
        SCHEMA_IDS = *s;

    if (auto peerRootCertPath =
                getIniFileSection(secConfig, SECTION_PEER_X509_ROOT_PATH))
        {
            std::map<std::string, bool> duplicateCheck;
            for (auto path : *peerRootCertPath)
            {
                std::string peerRootCert;
                std::ifstream ifsPath(path.c_str());
                peerRootCert.assign(
                    std::istreambuf_iterator<char>(ifsPath),
                    std::istreambuf_iterator<char>());
                if (peerRootCert.empty() ||
                    duplicateCheck.count(peerRootCert) > 0)
                {
                    continue;
                }
                duplicateCheck.emplace(std::make_pair(peerRootCert, true));
                PEER_ROOT_CERTIFICATES.push_back(peerRootCert);
            }
        }

    {
        std::string strTemp;
        if (getSingleSection(
                secConfig, SECTION_PEER_X509_CRED_PATH, strTemp, j_))
        {
            std::ifstream ifsPath(strTemp.c_str());
            PEER_X509_CRED.assign(
                std::istreambuf_iterator<char>(ifsPath),
                std::istreambuf_iterator<char>());
        }
    }

    auto cryptoAlgSection = section( ConfigSection::cryptoAlg() );
    if (!cryptoAlgSection.empty ())
    {
        if(cryptoAlgSection.exists("node_alg_type"))
        {
            auto nodeAlgType = cryptoAlgSection.get<std::string>("node_alg_type");
            if (!CommonKey::setAlgType(*nodeAlgType))
            {
                Throw<std::runtime_error> ("node_alg_type is invalid");
            }
        }

        // if(cryptoAlgSection.exists("hash_type"))
        // {
        //     auto hashType = cryptoAlgSection.get<std::string>("hash_type");
        //     if (!CommonKey::setHashType(*hashType))
        //     {
        //         Throw<std::runtime_error> ("hash_type is invalid");
        //     }
        // }

        if(cryptoAlgSection.exists("gm_self_check"))
        {
            auto gmSelfCheck = cryptoAlgSection.get<bool>("gm_self_check");
            GM_SELF_CHECK = *gmSelfCheck;
        }
    }

    {
        std::string dbPath;
        if (getSingleSection(secConfig, "database_path", dbPath, j_))
        {
            boost::filesystem::path p(dbPath);
            legacy("database_path", boost::filesystem::absolute(p).string());
        }
    }

    std::string strTemp;

    if (getSingleSection(secConfig, SECTION_PEER_PRIVATE, strTemp, j_))
        PEER_PRIVATE = beast::lexicalCastThrow<bool>(strTemp);

    if (getSingleSection(secConfig, SECTION_PEERS_MAX, strTemp, j_))
        PEERS_MAX = beast::lexicalCastThrow<std::size_t>(strTemp);

    if (getSingleSection(secConfig, SECTION_NODE_SIZE, strTemp, j_))
    {
        if (boost::iequals(strTemp, "tiny"))
            NODE_SIZE = 0;
        else if (boost::iequals(strTemp, "small"))
            NODE_SIZE = 1;
        else if (boost::iequals(strTemp, "medium"))
            NODE_SIZE = 2;
        else if (boost::iequals(strTemp, "large"))
            NODE_SIZE = 3;
        else if (boost::iequals(strTemp, "huge"))
            NODE_SIZE = 4;
        else
            NODE_SIZE = std::min<std::size_t>(
                4, beast::lexicalCastThrow<std::size_t>(strTemp));
    }

    if (getSingleSection(secConfig, SECTION_SIGNING_SUPPORT, strTemp, j_))
        signingEnabled_ = beast::lexicalCastThrow<bool>(strTemp);

    if (getSingleSection(secConfig, SECTION_ELB_SUPPORT, strTemp, j_))
        ELB_SUPPORT = beast::lexicalCastThrow<bool>(strTemp);

    if (getSingleSection(secConfig, SECTION_WEBSOCKET_PING_FREQ, strTemp, j_))
        WEBSOCKET_PING_FREQ =
            std::chrono::seconds{beast::lexicalCastThrow<int>(strTemp)};

    getSingleSection(secConfig, SECTION_SSL_VERIFY_FILE, SSL_VERIFY_FILE, j_);
    getSingleSection(secConfig, SECTION_SSL_VERIFY_DIR, SSL_VERIFY_DIR, j_);

    if (getSingleSection(secConfig, SECTION_SSL_VERIFY, strTemp, j_))
        SSL_VERIFY = beast::lexicalCastThrow<bool>(strTemp);

    auto cmdSSLCertSection = section( SECTION_CMD_SSL_CERT );
    if (!cmdSSLCertSection.empty ())
    {
        if(cmdSSLCertSection.exists("ssl_key") && cmdSSLCertSection.exists("ssl_cert"))
        {
            CMD_SSL_KEY = *(cmdSSLCertSection.get<std::string>("ssl_key"));
            CMD_SSL_CERT = *(cmdSSLCertSection.get<std::string>("ssl_cert"));
        }
        else if(!cmdSSLCertSection.exists("ssl_key") && !cmdSSLCertSection.exists("ssl_cert"))
        {
            CMD_SSL_KEY = "";
            CMD_SSL_CERT = "";
        }
        else
            Throw<std::runtime_error>(
                "ssl_key and ssl_cert  in [" SECTION_CMD_SSL_CERT
                "] section must both config or both not config");
    }

    if (getSingleSection(secConfig, SECTION_RELAY_VALIDATIONS, strTemp, j_))
    {
        if (boost::iequals(strTemp, "all"))
            RELAY_UNTRUSTED_VALIDATIONS = true;
        else if (boost::iequals(strTemp, "trusted"))
            RELAY_UNTRUSTED_VALIDATIONS = false;
        else
            Throw<std::runtime_error>(
                "Invalid value specified in [" SECTION_RELAY_VALIDATIONS
                "] section");
    }

    if (getSingleSection(secConfig, SECTION_RELAY_PROPOSALS, strTemp, j_))
    {
        if (boost::iequals(strTemp, "all"))
            RELAY_UNTRUSTED_PROPOSALS = true;
        else if (boost::iequals(strTemp, "trusted"))
            RELAY_UNTRUSTED_PROPOSALS = false;
        else
            Throw<std::runtime_error>(
                "Invalid value specified in [" SECTION_RELAY_PROPOSALS
                "] section");
    }

    if (exists(SECTION_VALIDATION_SEED) && exists(SECTION_VALIDATOR_TOKEN))
        Throw<std::runtime_error>("Cannot have both [" SECTION_VALIDATION_SEED
                                  "] "
                                  "and [" SECTION_VALIDATOR_TOKEN
                                  "] config sections");

    if (getSingleSection(secConfig, SECTION_NETWORK_QUORUM, strTemp, j_))
        NETWORK_QUORUM = beast::lexicalCastThrow<std::size_t>(strTemp);

    if (getSingleSection(secConfig, SECTION_FEE_ACCOUNT_RESERVE, strTemp, j_))
        FEE_ACCOUNT_RESERVE = beast::lexicalCastThrow<std::uint64_t>(strTemp);

    if (getSingleSection(secConfig, SECTION_FEE_OWNER_RESERVE, strTemp, j_))
        FEE_OWNER_RESERVE = beast::lexicalCastThrow<std::uint64_t>(strTemp);

    if (getSingleSection(secConfig, SECTION_FEE_DEFAULT, strTemp, j_))
        FEE_DEFAULT = beast::lexicalCastThrow<std::uint64_t>(strTemp);

    if (getSingleSection(secConfig, SECTION_LEDGER_HISTORY, strTemp, j_))
    {
        if (boost::iequals(strTemp, "full"))
            LEDGER_HISTORY =
                std::numeric_limits<decltype(LEDGER_HISTORY)>::max();
        else if (boost::iequals(strTemp, "none"))
            LEDGER_HISTORY = 0;
        else
            LEDGER_HISTORY = beast::lexicalCastThrow<std::uint32_t>(strTemp);
    }

    if (getSingleSection(secConfig, SECTION_FETCH_DEPTH, strTemp, j_))
    {
        if (boost::iequals(strTemp, "none"))
            FETCH_DEPTH = 0;
        else if (boost::iequals(strTemp, "full"))
            FETCH_DEPTH = std::numeric_limits<decltype(FETCH_DEPTH)>::max();
        else
            FETCH_DEPTH = beast::lexicalCastThrow<std::uint32_t>(strTemp);

        if (FETCH_DEPTH < 10)
            FETCH_DEPTH = 10;
    }

    if (getSingleSection(secConfig, SECTION_PATH_SEARCH_OLD, strTemp, j_))
        PATH_SEARCH_OLD = beast::lexicalCastThrow<int>(strTemp);
    if (getSingleSection(secConfig, SECTION_PATH_SEARCH, strTemp, j_))
        PATH_SEARCH = beast::lexicalCastThrow<int>(strTemp);
    if (getSingleSection(secConfig, SECTION_PATH_SEARCH_FAST, strTemp, j_))
        PATH_SEARCH_FAST = beast::lexicalCastThrow<int>(strTemp);
    if (getSingleSection(secConfig, SECTION_PATH_SEARCH_MAX, strTemp, j_))
        PATH_SEARCH_MAX = beast::lexicalCastThrow<int>(strTemp);

    if (getSingleSection(secConfig, SECTION_DEBUG_LOGFILE, strTemp, j_))
        DEBUG_LOGFILE = strTemp;

    if (getSingleSection(secConfig, SECTION_WORKERS, strTemp, j_))
        WORKERS = beast::lexicalCastThrow<std::size_t>(strTemp);

    if (getSingleSection(secConfig, SECTION_COMPRESSION, strTemp, j_))
        COMPRESSION = beast::lexicalCastThrow<bool>(strTemp);

    if (getSingleSection(
            secConfig, SECTION_AMENDMENT_MAJORITY_TIME, strTemp, j_))
    {
        using namespace std::chrono;
        boost::regex const re(
            "^\\s*(\\d+)\\s*(minutes|hours|days|weeks)\\s*(\\s+.*)?$");
        boost::smatch match;
        if (!boost::regex_match(strTemp, match, re))
            Throw<std::runtime_error>(
                "Invalid " SECTION_AMENDMENT_MAJORITY_TIME
                ", must be: [0-9]+ [minutes|hours|days|weeks]");

        std::uint32_t duration =
            beast::lexicalCastThrow<std::uint32_t>(match[1].str());

        if (boost::iequals(match[2], "minutes"))
            AMENDMENT_MAJORITY_TIME = minutes(duration);
        else if (boost::iequals(match[2], "hours"))
            AMENDMENT_MAJORITY_TIME = hours(duration);
        else if (boost::iequals(match[2], "days"))
            AMENDMENT_MAJORITY_TIME = days(duration);
        else if (boost::iequals(match[2], "weeks"))
            AMENDMENT_MAJORITY_TIME = weeks(duration);

        if (AMENDMENT_MAJORITY_TIME < minutes(15))
            Throw<std::runtime_error>(
                "Invalid " SECTION_AMENDMENT_MAJORITY_TIME
                ", the minimum amount of time an amendment must hold a "
                "majority is 15 minutes");
    }


    auto const result = section(SECTION_SCHEMA).find("schema_path");
    if (result.second)
    {
        try
        {
            SCHEMA_PATH = result.first;

        }
        catch (std::exception const&)
        {
            JLOG(j_.error()) <<
                "Invalid value '" << result.second << "' for key " <<
                "'schema_path' in [" << SECTION_SCHEMA << "]\n";
            Rethrow();
        }
    }

    auto const resSchema = section(SECTION_SCHEMA).find("auto_accept_new_schema");
    if (resSchema.second)
    {
        try
        {
            AUTO_ACCEPT_NEW_SCHEMA = (beast::lexicalCastThrow <int>(resSchema.first) == 1);
        }
        catch (std::exception const&)
        {
            JLOG(j_.error()) <<
                "Invalid value '" << resSchema.second << "' for key " <<
                "'auto_accept_new_schema' in [" << SECTION_SCHEMA << "]\n";
            Rethrow();
        }
    }

    auto const resSchemaValidate = section(SECTION_SCHEMA).find("only_validate_for_schema");
    if (resSchemaValidate.second)
    {
        try
        {
            ONLY_VALIDATE_FOR_SCHEMA = (beast::lexicalCastThrow <int>(resSchemaValidate.first) == 1);
            if (ONLY_VALIDATE_FOR_SCHEMA)
            {
                //non-validator,don't load last full ledger
                START_UP = FRESH;            
            }
        }
        catch (std::exception const&)
        {
            JLOG(j_.error()) <<
                "Invalid value '" << resSchemaValidate.second << "' for key " <<
                "'only_validate_for_schema' in [" << SECTION_SCHEMA << "]\n";
            Rethrow();
        }
    }



    // Do not load trusted validator configuration for standalone mode
    if (!RUN_STANDALONE)
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

        if (getSingleSection(secConfig, SECTION_VALIDATORS_FILE, strTemp, j_))
        {
            validatorsFile = strTemp;

            if (validatorsFile.empty())
                Throw<std::runtime_error>(
                    "Invalid path specified in [" SECTION_VALIDATORS_FILE "]");

            if (!validatorsFile.is_absolute() && !CONFIG_DIR.empty())
                validatorsFile = CONFIG_DIR / validatorsFile;

            if (!boost::filesystem::exists(validatorsFile))
                Throw<std::runtime_error>(
                    "The file specified in [" SECTION_VALIDATORS_FILE
                    "] "
                    "does not exist: " +
                    validatorsFile.string());

            else if (
                !boost::filesystem::is_regular_file(validatorsFile) &&
                !boost::filesystem::is_symlink(validatorsFile))
                Throw<std::runtime_error>(
                    "Invalid file specified in [" SECTION_VALIDATORS_FILE
                    "]: " +
                    validatorsFile.string());
        }
        else if (!CONFIG_DIR.empty())
        {
            validatorsFile = CONFIG_DIR / validatorsFileName;

            if (!validatorsFile.empty())
            {
                if (!boost::filesystem::exists(validatorsFile))
                    validatorsFile.clear();
                else if (
                    !boost::filesystem::is_regular_file(validatorsFile) &&
                    !boost::filesystem::is_symlink(validatorsFile))
                    validatorsFile.clear();
            }
        }

        if (!validatorsFile.empty() &&
            boost::filesystem::exists(validatorsFile) &&
            (boost::filesystem::is_regular_file(validatorsFile) ||
             boost::filesystem::is_symlink(validatorsFile)))
        {
            boost::system::error_code ec;
            auto const data = getFileContents(ec, validatorsFile);
            if (ec)
            {
                Throw<std::runtime_error>(
                    "Failed to read '" + validatorsFile.string() + "'." +
                    std::to_string(ec.value()) + ": " + ec.message());
            }

            auto iniFile = parseIniFile(data, true);

            auto entries = getIniFileSection(iniFile, SECTION_VALIDATORS);

            if (entries)
                section(SECTION_VALIDATORS).append(*entries);

            auto valKeyEntries =
                getIniFileSection(iniFile, SECTION_VALIDATOR_KEYS);

            if (valKeyEntries)
                section(SECTION_VALIDATOR_KEYS).append(*valKeyEntries);

            auto valSiteEntries =
                getIniFileSection(iniFile, SECTION_VALIDATOR_LIST_SITES);

            if (valSiteEntries)
                section(SECTION_VALIDATOR_LIST_SITES).append(*valSiteEntries);

            auto valListKeys =
                getIniFileSection(iniFile, SECTION_VALIDATOR_LIST_KEYS);

            if (valListKeys)
                section(SECTION_VALIDATOR_LIST_KEYS).append(*valListKeys);

            if (!entries && !valKeyEntries && !valListKeys)
                Throw<std::runtime_error>(
                    "The file specified in [" SECTION_VALIDATORS_FILE
                    "] "
                    "does not contain a [" SECTION_VALIDATORS
                    "], "
                    "[" SECTION_VALIDATOR_KEYS
                    "] or "
                    "[" SECTION_VALIDATOR_LIST_KEYS
                    "]"
                    " section: " +
                    validatorsFile.string());
        }

        // Consolidate [validator_keys] and [validators]
        section(SECTION_VALIDATORS)
            .append(section(SECTION_VALIDATOR_KEYS).lines());

        if (!section(SECTION_VALIDATOR_LIST_SITES).lines().empty() &&
            section(SECTION_VALIDATOR_LIST_KEYS).lines().empty())
        {
            Throw<std::runtime_error>(
                "[" + std::string(SECTION_VALIDATOR_LIST_KEYS) +
                "] config section is missing");
        }
    }

    {
        auto const part = section("features");
        for (auto const& s : part.values())
        {
            if (auto const f = getRegisteredFeature(s))
                features.insert(*f);
            else
                Throw<std::runtime_error>(
                    "Unknown feature: " + s + "  in config file.");
        }
    }

    Section ledgerTxTablesSection = section(LEDGER_TXS_TABLES);
    get_if_exists(ledgerTxTablesSection, "use_tx_tables", USE_TX_TABLES);
    get_if_exists(ledgerTxTablesSection, "save_tx_binary", SAVE_TX_RAW);
    get_if_exists(ledgerTxTablesSection, "use_trace_table", USE_TRACE_TABLE);

    if (!USE_TX_TABLES && SAVE_TX_RAW)
    {
        Throw<std::runtime_error>(
            "Cannot configure 'use_tx_tables=0' with 'save_tx_binary=1' in "
            "config file.");
    }
    get_if_exists(
        section(SECTION_CONSENSUS), "batch_broadcast", BATCH_BROADCAST);

    get_if_exists(
        section(SECTION_GOVERNANCE), "open_account_delay", OPEN_ACCOUNT_DELAY);

    get_if_exists(
        section(SECTION_GOVERNANCE),
        "default_authority_enabled",
        DEFAULT_AUTHORITY_ENABLED);

    if (exists(SECTION_GOVERNANCE))
    {
        auto result = section(SECTION_GOVERNANCE).find("admin");
        if (result.second)
        {
            ADMIN = ripple::parseBase58<AccountID>(result.first);
            if (!ADMIN)
                Throw<std::runtime_error>("admin address is invalid");
        }
    }
}

boost::filesystem::path
Config::getDebugLogFile() const
{
    auto log_file = DEBUG_LOGFILE;

    if (!log_file.empty() && !log_file.is_absolute())
    {
        // Unless an absolute path for the log file is specified, the
        // path is relative to the config file directory.
        log_file = boost::filesystem::absolute(log_file, CONFIG_DIR);
    }

    if (!log_file.empty())
    {
        auto log_dir = log_file.parent_path();

        if (!boost::filesystem::is_directory(log_dir))
        {
            boost::system::error_code ec;
            boost::filesystem::create_directories(log_dir, ec);

            // If we fail, we warn but continue so that the calling code can
            // decide how to handle this situation.
            if (ec)
            {
                std::cerr << "Unable to create log file path " << log_dir
                          << ": " << ec.message() << '\n';
            }
        }
    }

    return log_file;
}

int
Config::getValueFor(SizedItem item, boost::optional<std::size_t> node) const
{
    auto const index = static_cast<std::underlying_type_t<SizedItem>>(item);
    assert(index < sizedItems.size());
    assert(!node || *node <= 4);
    return sizedItems.at(index).second.at(node.value_or(NODE_SIZE));
}

bool
Config::checkCertificates() const
{
    bool flag = false;

    if (!section(SECTION_USER_X509_ROOT_PATH).values().empty())
    {
        if (USER_ROOT_CERTIFICATES.empty())
        {
            Throw<std::runtime_error>("The user root certificate config is incorrectly");
        }
        flag = true;
    }

    if (!section(SECTION_PEER_X509_ROOT_PATH).values().empty())
    {
        if (PEER_ROOT_CERTIFICATES.empty())
        {
            Throw<std::runtime_error>("The peer credential config is incorrectly");
        }
        flag = true;
    }

    if (!section(SECTION_PEER_X509_CRED_PATH).values().empty())
    {
        if (PEER_X509_CRED.empty())
        {
            Throw<std::runtime_error>("The peer root certificate config is incorrectly");
        }
        flag = true;
    }

    if (PEER_ROOT_CERTIFICATES.empty() !=
        PEER_X509_CRED.empty())
    {
        Throw<std::runtime_error>("The peer root certificate config and peer "
                                   "credential config is inconformity");
    }

    return flag;
}

}  // namespace ripple
