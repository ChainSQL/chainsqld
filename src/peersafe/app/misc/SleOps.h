#ifndef CHAINSQL_APP_MISC_SLEOPS_H_INCLUDED
#define CHAINSQL_APP_MISC_SLEOPS_H_INCLUDED

#include <ripple/protocol/AccountID.h>
#include <ripple/protocol/UintTypes.h>
#include <ripple/protocol/STAmount.h>
#include <ripple/app/tx/impl/ApplyContext.h>
#include <peersafe/app/misc/SleOps.h>
#include <peersafe/app/misc/TypeTransform.h>


namespace ripple {

class SleOps
{
public:
    SleOps() {}

    SLE::pointer getSle(ApplyContext& ctx, evmc_address const & addr) const;
};

}

#endif
