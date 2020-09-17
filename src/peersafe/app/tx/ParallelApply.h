#ifndef CHAINSQL_APP_TX_PARALLELAPPLY_H_INCLUDED
#define CHAINSQL_APP_TX_PARALLELAPPLY_H_INCLUDED

#if USE_TBB
#include <peersafe/schema/Schema.h>
#include <ripple/beast/utility/Journal.h>
#include <ripple/protocol/STTx.h>
#include <ripple/ledger/ApplyView.h>
#include <tbb/blocked_range.h>
#include <tbb/concurrent_vector.h>

namespace ripple {

class ParallelApply
{
public:
    using Txs = tbb::concurrent_vector<std::shared_ptr<STTx const>>;

    ParallelApply(
        Txs &a, Txs &r, bool &certainRetry, int &changes, 
        Schema& app, OpenView& view, beast::Journal j
    )
        : certainRetry_(certainRetry)
        , changes_(changes)
	, shouldApplyTxs_(a)
	, retryTxs_(r)
        , app_(app)
        , view_(view)
        , j(j)
    {}

    ~ParallelApply() {}

    void operator() (Txs::range_type &r) const;

    void preRetry();

private:
    bool &certainRetry_;
    int &changes_;

    Txs &shouldApplyTxs_;
    Txs &retryTxs_;

    Schema& app_;
    OpenView& view_;
    beast::Journal j;

};

}

#endif

#endif
