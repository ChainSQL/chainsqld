
#ifndef PEERSAFE_CONSENSUS_HOTSTUFF_PARAMS_H_INCLUDE
#define PEERSAFE_CONSENSUS_HOTSTUFF_PARAMS_H_INCLUDE

namespace ripple {

/**
 * simple config section e.x.
 * [hconsensus]
 * timeout=60
 */

struct HotstuffConsensusParms
{
    explicit HotstuffConsensusParms() = default;

    unsigned minBLOCK_TIME = 1000;
    unsigned maxBLOCK_TIME = 1000;

    unsigned maxTXS_IN_LEDGER = 10000;

    std::chrono::seconds consensusTIMEOUT = std::chrono::seconds{ 6 };

    std::chrono::seconds initTIME = std::chrono::seconds{ 90 };

    unsigned minTXS_IN_LEDGER_ADVANCE = 5000;
};


}


#endif // PEERSAFE_CONSENSUS_HOTSTUFF_PARAMS_H_INCLUDE
