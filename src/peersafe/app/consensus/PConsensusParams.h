
#ifndef PEERSAFE_APP_CONSENSUS_PCONSENSUSPARAMS_H
#define PEERSAFE_APP_CONSENSUS_PCONSENSUSPARAMS_H

namespace ripple {

    // Default pconsensus parameters

    // The minimum block generation time(ms)
    const unsigned MinBlockTime = 1000;

    // The maximum block generation time(ms) even without transactions.
    const unsigned MaxBlockTime = 1000;

    const unsigned MaxTxsInLedger = 10000;

	// The minimum tx limit for leader to propose a tx-set after half-MinBlockTime
	const unsigned MinTxsInLedgerAdvance = 5000;

    const std::size_t TxPoolCapacity = 100000;


	const std::chrono::milliseconds CONSENSUS_TIMEOUT = 3s;

	const unsigned TimeOutCountRollback = 5;

    // simple config section e.x.
    // [pconsensus]
    // min_block_time=500
    // max_block_time=1000
    // max_txs_per_ledger=10000
    // txpool_cap=100000
    // empty_block=0
}


#endif // PEERSAFE_APP_CONSENSUS_PCONSENSUSPARAMS_H
