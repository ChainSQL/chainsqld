//------------------------------------------------------------------------------
/*
        This file is part of rippled: https://github.com/ripple/rippled
        Copyright (c) 2012, 2013 Ripple Labs Inc.

        Permission to use, copy, modify, and/or distribute this software for any
        purpose  with  or without fee is hereby granted, provided that the above
        copyright notice and this permission notice appear in all copies.

        THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
   WARRANTIES WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED
   WARRANTIES  OF MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE
   LIABLE FOR ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY
   DAMAGES WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER
   IN AN ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
   OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================


#ifndef PEERSAFE_CONSENSUS_POP_PARAMS_H_INCLUDE
#define PEERSAFE_CONSENSUS_POP_PARAMS_H_INCLUDE

namespace ripple {

/**
 * simple config section e.x.
 * [pconsensus]
 * min_block_time=500
 * max_block_time=1000
 * max_txs_per_ledger=10000
 * txpool_cap=100000
 * empty_block=0
 * init_time=90
 */

struct PopConsensusParms
{
    explicit PopConsensusParms() = default;

    // The minimum and maximum block generation time(ms)
    unsigned minBLOCK_TIME = 1000;
    unsigned maxBLOCK_TIME = 1000;

    unsigned maxTXS_IN_LEDGER = 10000;

    std::chrono::milliseconds consensusTIMEOUT =
        std::chrono::milliseconds{3000};

    bool omitEMPTY = true;

    std::chrono::seconds initTIME = std::chrono::seconds{90};

    // The minimum tx limit for leader to propose a tx-set after
    // half-MinBlockTime
    const unsigned minTXS_IN_LEDGER_ADVANCE = 5000;

    const unsigned timeoutCOUNT_ROLLBACK = 5;
};

}


#endif // PEERSAFE_CONSENSUS_POP_PARAMS_H_INCLUDE
