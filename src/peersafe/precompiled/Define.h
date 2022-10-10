#ifndef PRECOMPILED_DEFINE_H_INCLUDE
#define PRECOMPILED_DEFINE_H_INCLUDE

#include <ripple/protocol/AccountID.h>

namespace ripple {
	const AccountID TABLE_OPERATION_ADDR = AccountID(0x1001); //zzzzzzzzzzzzzzzzzzz3yHctrGe
	const AccountID TOOLS_PRE_ADDR		 = AccountID(0x1002);

	// preCompiledContract address for function
	const AccountID SM3_ADDR = AccountID(0x4000);
	const AccountID EN_BASE58_ADDR = AccountID(0x4001);
	const AccountID DE_BASE58_ADDR = AccountID(0x4002);

}

#endif