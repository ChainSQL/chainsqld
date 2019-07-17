
#ifndef PEERSAFE_APP_CONSENSUS_PCONSENSUSPARAMS_H
#define PEERSAFE_APP_CONSENSUS_PCONSENSUSPARAMS_H

namespace ripple {
        // The minimum block generation time(ms)
        const unsigned MinBlockTime = 500;

        // The maximum block generation time(ms) even without transactions.
        const unsigned MaxBlockTime = 1000;

        const unsigned MaxTxsInLedger = 5000;


        const std::size_t TxPoolCapacity = 100000;
}


#endif // PEERSAFE_APP_CONSENSUS_PCONSENSUSPARAMS_H
