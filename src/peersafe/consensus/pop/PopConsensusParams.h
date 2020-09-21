
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

struct RopConsensusParms
{
    explicit RopConsensusParms() = default;

    // The minimum and maximum block generation time(ms)
    unsigned minBLOCK_TIME = 1000;
    unsigned maxBLOCK_TIME = 1000;

    unsigned maxTXS_IN_LEDGER = 10000;

    std::chrono::milliseconds consensusTIMEOUT = std::chrono::milliseconds {3000};

    bool omitEMPTY = true;

    std::chrono::seconds initTIME = std::chrono::seconds{ 90 };


    // The minimum tx limit for leader to propose a tx-set after half-MinBlockTime
    const unsigned minTXS_IN_LEDGER_ADVANCE = 5000;

    const unsigned timeoutCOUNT_ROLLBACK = 5;
};


}


#endif // PEERSAFE_APP_CONSENSUS_PCONSENSUSPARAMS_H
