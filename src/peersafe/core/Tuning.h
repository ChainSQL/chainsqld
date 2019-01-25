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

#ifndef PEERSAFE_CORE_PATHS_TUNING_H_INCLUDED
#define PEERSAFE_CORE_PATHS_TUNING_H_INCLUDED

namespace ripple {	
	// 
    int64_t const MAX_CODE_SIZE         = 0x6000;	
	//
    int64_t const CREATE_DATA_GAS       = 200;
    //
    uint64_t const STORE_REFUND_GAS     = 15000;
    //
    uint64_t const SUICIDE_REFUND_GAS   = 24000;

    uint64_t const TX_GAS               = 21000;
    uint64_t const TX_CREATE_GAS        = 53000;
    uint64_t const TX_DATA_ZERO_GAS     = 4;
    uint64_t const TX_DATA_NON_ZERO_GAS = 68;

	constexpr auto maxUInt64 = std::numeric_limits<std::uint64_t>::max();
	constexpr auto maxInt64  = std::numeric_limits<std::int64_t>::max();

	uint64_t const GAS_PRICE			= 10;

	uint32 const INITIAL_DEPTH			= 1;

	uint32 const LAST_LEDGERSEQ_PASS	= 8;

} // ripple

#endif