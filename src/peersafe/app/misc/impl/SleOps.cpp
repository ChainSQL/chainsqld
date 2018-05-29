#include <peersafe/app/misc/SleOps.h>

namespace ripple {
    //just raw function for zxc, all paras should be tranformed in extvmFace modules.

    SLE::pointer SleOps::getSle(evmc_address const & addr) const
    {        
        ApplyView& view = ctx_.view();
        
        AccountID accountID = fromEvmC(addr);
        auto const k = keylet::account(accountID);
        return view.peek(k);
    }

	void SleOps::incNonce(evmc_address const& addr)
	{
		SLE::pointer pSle = getSle(addr);
		uint32 nonce = pSle->getFieldU32(sfNonce);
		pSle->setFieldU32(sfNonce,++nonce);
	}

}