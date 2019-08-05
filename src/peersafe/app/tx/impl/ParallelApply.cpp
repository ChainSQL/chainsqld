
#if USE_TBB

#include <peersafe/app/tx/ParallelApply.h>
#include <ripple/app/tx/apply.h>


namespace ripple {

void ParallelApply::operator() (Txs::range_type &r) const
{
    for (auto it = r.begin(); it != r.end(); ++it)
    {
        try
        {
            switch (applyTransaction(
                app_, view_, *it->get(), certainRetry_, tapNO_CHECK_SIGN | tapForConsensus, j))
            {
            case ApplyResult::Success:
                ++changes_;
                break;

            case ApplyResult::Fail:
                break;

            case ApplyResult::Retry:
                retryTxs_.push_back(*it);
                ++it;
                break;
            }
        }
        catch (std::exception const&)
        {
            JLOG(j.warn()) << "Transaction throws";
            it = retryTxs_.push_back(*it);
        }
    }
}

void ParallelApply::preRetry()
{
    // Clear already applied txs.
    shouldApplyTxs_.clear();

    // Then swap should retry txs to shouldApply, prepare for next apply round.
    // Also clear should retry txs.
    shouldApplyTxs_.swap(retryTxs_);

    changes_ = 0;
}

}

#endif