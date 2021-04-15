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
 * [consensus]
 * type = pop/POP
 * max_txs_in_pool = 100000
 * min_block_time = 1000
 * max_block_time = 1000
 * max_txs_per_ledger = 10000
 * time_out = 3000
 * omit_empty_block = false
 * init_time = 90
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

    std::chrono::seconds initTIME = std::chrono::seconds{90};

    bool omitEMPTY = true;

    bool proposeTxSetDetail = false;

    // The minimum tx limit for leader to propose a tx-set after
    // half-MinBlockTime
    const unsigned minTXS_IN_LEDGER_ADVANCE = 5000;

    const unsigned timeoutCOUNT_ROLLBACK = 5;

    const std::chrono::seconds initANNOUNCE_INTERVAL = std::chrono::seconds{1};

    inline Json::Value
    getJson() const
    {
        using Int = Json::Value::Int;

        Json::Value ret(Json::objectValue);

        ret["min_block_time"] = minBLOCK_TIME;
        ret["max_block_time"] = maxBLOCK_TIME;
        ret["max_txs_per_ledger"] = maxTXS_IN_LEDGER;
        ret["time_out"] = static_cast<Int>(consensusTIMEOUT.count());
        ret["omit_empty_block"] = omitEMPTY;
        ret["init_time"] = static_cast<Int>(initTIME.count());
        ret["propose_txset_detail"] = proposeTxSetDetail;

        return ret;
    }
};

}  // namespace ripple

#endif  // PEERSAFE_CONSENSUS_POP_PARAMS_H_INCLUDE
