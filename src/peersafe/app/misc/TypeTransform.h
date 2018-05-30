#ifndef CHAINSQL_APP_MISC_TYPETRANSFORM_H_INCLUDED
#define CHAINSQL_APP_MISC_TYPETRANSFORM_H_INCLUDED

#include <ripple/protocol/AccountID.h>
#include <peersafe/vm/ExtVMFace.h>

namespace ripple {


    inline evmc_address toEvmC(AccountID const& addr)
    {
        return reinterpret_cast<evmc_address const&>(addr);
    }

    inline evmc_uint256be toEvmC(uint256 const& _h)
    {
        return reinterpret_cast<evmc_uint256be const&>(_h);
    }

    inline uint256 fromEvmC(evmc_uint256be const& _n)
    {
        return beast::zero;
    }

    inline AccountID fromEvmC(evmc_address const& _addr)
    {
        return AccountID();
    }

}

#endif
