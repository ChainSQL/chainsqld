#ifndef RIPPLE_RPC_LEDGERADJUST_LEDGER_H_INCLUDED
#define RIPPLE_RPC_LEDGERADJUST_LEDGER_H_INCLUDED

#include <ripple/core/Config.h>
#include <ripple/core/JobQueue.h>
#include <ripple/beast/utility/PropertyStream.h>
#include <ripple/protocol/Protocol.h>
#include <ripple/basics/Log.h>
#include <ripple/basics/TaggedCache.h>
#include <peersafe/schema/Schema.h>
#include <ripple/app/ledger/OpenLedger.h>
#include <ripple/ledger/ApplyView.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/app/main/Application.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/ledger/LedgerToJson.h>
#include <ripple/app/misc/Transaction.h>
#include <ripple/app/misc/impl/AccountTxPaging.h>
#include <array>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>

namespace ripple {
	
	enum ContractState : int { CONTRACT_CREATE = 0, CONTRACT_CALL = 1, CONTRACT_DESTORY = 2 };
	class LedgerAdjust {

	public:
		//LedgerAdjust();
		
		//virtual ~LedgerAdjust();
        static int getTxSucessCount(LockedSociSession db);
        static int getTxFailCount(LockedSociSession db);
        static int getContractCreateCount(LockedSociSession db);
        static int getContractCallCount(LockedSociSession db);
        static int getAccountCount(LockedSociSession db);
        static void updateContractCount(Schema& app, ApplyView& view, ContractState state);
		static void updateTxCount(Schema& app, OpenView& view, int successCount, int failCount);
		static void updateAccountCount(Schema& app, OpenView& view,int accountCount);
        static void createSle(Schema& app);
        static bool isCompleteReadData(Schema& app);
	};
}

#endif
