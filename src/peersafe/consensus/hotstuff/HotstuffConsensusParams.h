
#ifndef PEERSAFE_CONSENSUS_HOTSTUFF_PARAMS_H_INCLUDE
#define PEERSAFE_CONSENSUS_HOTSTUFF_PARAMS_H_INCLUDE


namespace ripple {

/**
 * simple config section e.x.
 * [consensus]
 * type = hotstuff/HOTSTUFF
 * max_txs_in_pool = 100000
 * min_block_time = 1000
 * max_block_time = 1000
 * max_txs_per_ledger = 10000
 * time_out = 5000
 * omit_empty_block = false
 * init_time = 90
 */

struct HotstuffConsensusParms
{
    explicit HotstuffConsensusParms() = default;

    unsigned minBLOCK_TIME = 1000;
    unsigned maxBLOCK_TIME = 1000;

    unsigned maxTXS_IN_LEDGER = 10000;

    bool omitEMPTY = true;

    std::chrono::milliseconds consensusTIMEOUT =
        std::chrono::milliseconds{5000};

    std::chrono::seconds initTIME = std::chrono::seconds{90};

    std::chrono::milliseconds extractINTERVAL =
        std::chrono::milliseconds{200};

        // The minimum tx limit for leader to propose a tx-set after
    // half-MinBlockTime
    const unsigned minTXS_IN_LEDGER_ADVANCE = 5000;

    const unsigned timeoutCOUNT_ROLLBACK = 5;

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

        return ret;
    }
};


}


#endif // PEERSAFE_CONSENSUS_HOTSTUFF_PARAMS_H_INCLUDE
