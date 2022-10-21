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

#ifndef PEERSAFE_APP_TX_PATHS_TUNING_H_INCLUDED
#define PEERSAFE_APP_TX_PATHS_TUNING_H_INCLUDED

namespace ripple {
	//an address can create at most 100 tables
	int const ACCOUNT_OWN_TABLE_COUNT = 100;

	//a table owner can grant at most 256 users
	int const TABLE_GRANT_COUNT = 256;

	int const MIN_NODE_COUNT_SCHEMA = 3;

	int const TX_TIMEOUT = 30;

	int const RAW_SHOW_SIZE = 2048;

	int const MAX_VALIDATOR_SCHEMA_COUNT = 5;

	int const MAX_BROAD_CAST_BATCH = 10000;

    int const DELAY_START_COUNT = 5;

	int const MAX_ACCOUNT_HELD_COUNT = 1500;
    int const MAX_HELD_COUNT = 15000;

	int const LAST_LEDGER_SEQ_OFFSET = 10;
} // ripple

#endif