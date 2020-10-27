
#ifndef PEERSAFE_CONSENSUS_HOTSTUFF_PARAMS_H_INCLUDE
#define PEERSAFE_CONSENSUS_HOTSTUFF_PARAMS_H_INCLUDE

namespace ripple {

/**
 * simple config section e.x.
 * [hconsensus]
 * xxx=xxx
 */

struct HotstuffConsensusParms
{
    explicit HotstuffConsensusParms() = default;

    unsigned maxTXS_IN_LEDGER = 10000;
};


}


#endif // PEERSAFE_CONSENSUS_HOTSTUFF_PARAMS_H_INCLUDE
