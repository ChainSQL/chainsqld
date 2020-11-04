
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

    unsigned maxTXS_IN_LEDGER = 10000;

    std::chrono::seconds consensusTIMEOUT = std::chrono::seconds{ 3 };
};


}


#endif // PEERSAFE_CONSENSUS_HOTSTUFF_PARAMS_H_INCLUDE
