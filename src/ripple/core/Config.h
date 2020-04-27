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

#ifndef RIPPLE_CORE_CONFIG_H_INCLUDED
#define RIPPLE_CORE_CONFIG_H_INCLUDED

#include <ripple/basics/BasicConfig.h>
#include <ripple/basics/base_uint.h>
#include <ripple/protocol/SystemParameters.h> // VFALCO Breaks levelization
#include <ripple/beast/net/IPEndpoint.h>
#include <beast/core/string.hpp>
#include <ripple/beast/utility/Journal.h>
#include <boost/asio/ip/tcp.hpp> // VFALCO FIX: This include should not be here
#include <boost/filesystem.hpp> // VFALCO FIX: This include should not be here
#include <boost/lexical_cast.hpp>
#include <boost/optional.hpp>
#include <cstdint>
#include <map>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace ripple {

using namespace std::chrono_literals;

class Rules;

//------------------------------------------------------------------------------

enum SizedItemName
{
    siSweepInterval,
    siNodeCacheSize,
    siNodeCacheAge,
    siTreeCacheSize,
    siTreeCacheAge,
    siSLECacheSize,
    siSLECacheAge,
    siLedgerSize,
    siLedgerAge,
    siLedgerFetch,
    siHashNodeDBCache,
    siTxnDBCache,
    siLgrDBCache,
};

struct SizedItem
{
    SizedItemName   item;
    int             sizes[5];
};

//  This entire derived class is deprecated.
//  For new config information use the style implied
//  in the base class. For existing config information
//  try to refactor code to use the new style.
//
class Config : public BasicConfig
{
public:
    // Settings related to the configuration file location and directories
    static char const* const configFileName;
    static char const* const databaseDirName;
    static char const* const validatorsFileName;

    /** Returns the full path and filename of the debug log file. */
    boost::filesystem::path getDebugLogFile () const;

    /** Returns the full path and filename of the entropy seed file. */
    boost::filesystem::path getEntropyFile () const;

private:
    boost::filesystem::path CONFIG_FILE;
    boost::filesystem::path CONFIG_DIR;
    boost::filesystem::path DEBUG_LOGFILE;

    void load ();
    beast::Journal j_;

    bool QUIET = false;          // Minimize logging verbosity.
    bool SILENT = false;         // No output to console after startup.
    /** Operate in stand-alone mode.

        In stand alone mode:

        - Peer connections are not attempted or accepted
        - The ledger is not advanced automatically.
        - If no ledger is loaded, the default ledger with the root
          account is created.
    */
    bool                        RUN_STANDALONE = false;

public:
    bool doImport = false;
    bool ELB_SUPPORT = false;

    std::vector<std::string>    IPS;                    // Peer IPs from rippled.cfg.
    std::vector<std::string>    IPS_FIXED;              // Fixed Peer IPs from rippled.cfg.
    std::vector<std::string>    SNTP_SERVERS;           // SNTP servers from rippled.cfg.


	std::vector<std::string>    ROOT_CERTIFICATES;          // root certificates from rippled.cfg.

    enum StartUpType
    {
        FRESH,
        NORMAL,
        LOAD,
        LOAD_FILE,
        REPLAY,
        NETWORK
    };
    StartUpType                 START_UP = NORMAL;

    bool                        START_VALID = false;

    std::string                 START_LEDGER;

    // Network parameters
    int const                   TRANSACTION_FEE_BASE = 10;   // The number of fee units a reference transaction costs

    // Note: The following parameters do not relate to the UNL or trust at all
    std::size_t                 NETWORK_QUORUM = 0;         // Minimum number of nodes to consider the network present

    // Peer networking parameters
    bool                        PEER_PRIVATE = false;           // True to ask peers not to relay current IP.
    int                         PEERS_MAX = 0;

    std::chrono::seconds        WEBSOCKET_PING_FREQ = 5min;

    // Path searching
    int                         PATH_SEARCH_OLD = 7;
    int                         PATH_SEARCH = 7;
    int                         PATH_SEARCH_FAST = 2;
    int                         PATH_SEARCH_MAX = 10;

    // Validation
    boost::optional<std::size_t> VALIDATION_QUORUM;     // Minimum validations to consider ledger authoritative

    std::uint64_t                      FEE_DEFAULT = 10;
    std::uint64_t                      FEE_ACCOUNT_RESERVE = 5*SYSTEM_CURRENCY_PARTS;
    std::uint64_t                      FEE_OWNER_RESERVE = 1*SYSTEM_CURRENCY_PARTS;


	std::uint64_t                     DROPS_PER_BYTE = (1000000 / 1024);

    std::uint64_t                      FEE_OFFER = 10;

    // Node storage configuration
    std::uint32_t                      LEDGER_HISTORY = 256;
    std::uint32_t                      FETCH_DEPTH = 1000000000;
    int                         NODE_SIZE = 0;

    bool                        SSL_VERIFY = true;
    std::string                 SSL_VERIFY_FILE;
    std::string                 SSL_VERIFY_DIR;


    // Thread pool configuration
    std::size_t                 WORKERS = 0;

    // These override the command line client settings
    boost::optional<boost::asio::ip::address_v4> rpc_ip;
    boost::optional<std::uint16_t> rpc_port;

    std::unordered_set<uint256, beast::uhash<>> features;


	// shard related configuration items

	const std::uint32_t SHARD_ROLE_UNDEFINED = 0x00000000;
	const std::uint32_t SHARD_ROLE_LOOKUP    = 0x00000001;
	const std::uint32_t SHARD_ROLE_SHARD     = 0x00000002;
	const std::uint32_t SHARD_ROLE_COMMITTEE = 0x00000004;
	const std::uint32_t SHARD_ROLE_SYNC	     = 0x00000008;



	std::uint32_t             SHARD_ROLE  = SHARD_ROLE_UNDEFINED;
	std::size_t               SHARD_COUNT = 1;
	std::size_t               SHARD_INDEX = 1;



	std::string               SHARD_FILE;
	std::string               LOOKUP_FILE;
	std::string               SYNC_FILE;


	std::vector<std::string>    LOOKUP_IPS;                 
	std::vector<std::string>    COMMITTEE_IPS;
	std::vector<std::string>    SYNC_IPS;      

	std::vector< std::vector<std::string> >   SHARD_IPS;
	std::vector< std::vector<std::string> >   SHARD_VALIDATORS;

	std::vector<std::string>    LOOKUP_PUBLIC_KEYS;
	std::vector<std::string>    COMMITTEE_VALIDATORS;


public:
    Config() = default;

    int getSize (SizedItemName) const;
    /* Be very careful to make sure these bool params
        are in the right order. */
    void setup (std::string const& strConf, bool bQuiet,
        bool bSilent, bool bStandalone);
    void setupControl (bool bQuiet,
        bool bSilent, bool bStandalone);

    /**
     *  Load the config from the contents of the string.
     *
     *  @param fileContents String representing the config contents.
     */
    void loadFromString (std::string const& fileContents);

    bool quiet() const { return QUIET; }
    bool silent() const { return SILENT; }
    bool standalone() const { return RUN_STANDALONE; }



	// shard related

	bool loadLookupConfig(IniFileSections& secConfig);
	bool loadShardConfig(IniFileSections& secConfig);
	bool loadCommitteeConfig(IniFileSections& secConfig);
	bool loadSyncConfigConfig(IniFileSections& secConfig);

	// get all shard related ips
	void getShardRelatedIps(std::vector<std::string>& ips);

	// get shard role string. example: "lookup,sync" "shard"
	std::string shardRoleToString(std::uint32_t& shardRole);

	int          getShardRole()  const;
	std::size_t  getShardIndex() const;

	bool         isShardOrCommittee();
};

} // ripple

#endif
