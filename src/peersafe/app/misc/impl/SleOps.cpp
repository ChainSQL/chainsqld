#include <peersafe/app/misc/SleOps.h>

namespace ripple {
    //just raw function for zxc, all paras should be tranformed in extvmFace modules.

    SLE::pointer SleOps::getSle(ApplyContext& ctx, evmc_address const & addr) const
    {        
        ApplyView& view = ctx.view();
        
        AccountID accountID = fromEvmC(addr);
        auto const k = keylet::account(accountID);
        return view.peek(k);
    }



}