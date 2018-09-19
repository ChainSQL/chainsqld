#include <peersafe/app/misc/ExtVM.h>
#include <peersafe/app/misc/Executive.h>
#include <peersafe/protocol/STMap256.h>
#include <ripple/protocol/ZXCAmount.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <peersafe/protocol/TableDefines.h>

#include <boost/thread.hpp>
#include <exception>


namespace ripple
{
    static unsigned const c_depthLimit = 1024;

    /// Upper bound of stack space needed by single CALL/CREATE execution. Set experimentally.
    static size_t const c_singleExecutionStackSize = 100 * 1024;
    /// Standard thread stack size.
    static size_t const c_defaultStackSize =
#if defined(__linux)
        8 * 1024 * 1024;
#elif defined(_WIN32)
        16 * 1024 * 1024;
#else
        512 * 1024; // OSX and other OSs
#endif
    /// Stack overhead prior to allocation.
    static size_t const c_entryOverhead = 128 * 1024;
    /// On what depth execution should be offloaded to additional separated stack space.
    static unsigned const c_offloadPoint = (c_defaultStackSize - c_entryOverhead) / c_singleExecutionStackSize;

    void goOnOffloadedStack(Executive& _e)
    {
        // Set new stack size enouth to handle the rest of the calls up to the limit.
        boost::thread::attributes attrs;
        attrs.set_stack_size((c_depthLimit - c_offloadPoint) * c_singleExecutionStackSize);

        // Create new thread with big stack and join immediately.
        // TODO: It is possible to switch the implementation to Boost.Context or similar when the API is stable.
        boost::exception_ptr exception;
        boost::thread{ attrs, [&] {
            try
            {
                _e.go();
            }
            catch (...)
            {
                exception = boost::current_exception(); // Catch all exceptions to be rethrown in parent thread.
            }
        } }.join();
        if (exception)
            boost::rethrow_exception(exception);
    }

    void go(unsigned _depth, Executive& _e, beast::Journal &j)
    {
        // If in the offloading point we need to switch to additional separated stack space.
        // Current stack is too small to handle more CALL/CREATE executions.
        // It needs to be done only once as newly allocated stack space it enough to handle
        // the rest of the calls up to the depth limit (c_depthLimit).

        if (_depth == c_offloadPoint)
        {
            JLOG(j.trace()) << "Stack offloading (depth: " << c_offloadPoint << ")";
            goOnOffloadedStack(_e);
        }
        else
            _e.go();
    }

    evmc_status_code terToEvmcStatusCode(TER eState) noexcept
    {
        switch (eState)
        {
        case tesSUCCESS:
            return EVMC_SUCCESS;
        case tefGAS_INSUFFICIENT:
            return EVMC_OUT_OF_GAS;
        case tefCONTRACT_REVERT_INSTRUCTION:
            return EVMC_REVERT;
        default:
            return EVMC_FAILURE;
        }
        return EVMC_SUCCESS;
    }

    evmc_uint256be ExtVM::balance(evmc_address const& addr)
    {        
		int64_t drops = oSle_.balance(fromEvmC(addr));
        return toEvmC(uint256(drops));
    }

    evmc_uint256be ExtVM::store(evmc_uint256be const& key)
    {           
        SLE::pointer pSle = oSle_.getSle(fromEvmC(myAddress));
        STMap256& mapStore = pSle->peekFieldM256(sfStorageOverlay);

        uint256 uKey = fromEvmC(key);  
		try {
			auto uV = mapStore.at(uKey);
			return toEvmC(uV);
		}
		catch (std::exception e) {
			return toEvmC(uint256(0));
		}
    }

    void ExtVM::setStore(evmc_uint256be const& key, evmc_uint256be const& value)
    {
        SLE::pointer pSle = oSle_.getSle(fromEvmC(myAddress));
        STMap256& mapStore = pSle->peekFieldM256(sfStorageOverlay);
                
        uint256 uKey = fromEvmC(key);
        uint256 uValue = fromEvmC(value);
		if (uValue == uint256(0))
			mapStore.erase(uKey);
		else
			mapStore[uKey] = uValue;

		oSle_.ctx().view().update(pSle);
    }

    bytes const& ExtVM::codeAt(evmc_address const& addr)
    { 
        return oSle_.code(fromEvmC(addr));
    }

    size_t ExtVM::codeSizeAt(evmc_address const& addr) 
    { 
        bytes const& code = oSle_.code(fromEvmC(addr));
        return code.size();
    }

    bool ExtVM::exists(evmc_address const& addr) { 
        SLE::pointer pSle = oSle_.getSle(fromEvmC(addr));
        return pSle != nullptr;
    }

    void ExtVM::suicide(evmc_address const& addr) 
    {
        SLE::pointer sleContract = oSle_.getSle(fromEvmC(myAddress));
        SLE::pointer sleTo = oSle_.getSle(fromEvmC(addr));

        auto& stBalanceContract = sleContract->getFieldAmount(sfBalance);
        auto& stBalanceMy = sleTo->getFieldAmount(sfBalance);
		sleTo->setFieldAmount(sfBalance, stBalanceContract + stBalanceMy);

        ExtVMFace::suicide(addr);
    }

    CreateResult ExtVM::create(evmc_uint256be const& endowment, int64_t& ioGas,
        bytesConstRef const& code, Instruction op, evmc_uint256be const& /*salt*/)
    {
        //CreateResult ret(EVMC_SUCCESS, owning_bytes_ref(),evmc_address());
        
        Executive e(oSle_, envInfo(), depth + 1);
        assert(op == Instruction::CREATE);
        bool result = e.createOpcode(fromEvmC(myAddress), fromEvmC(endowment), fromEvmC(gasPrice),
			ioGas, code, fromEvmC(origin));

        if (!result)
        {
            ApplyContext const& ctx = oSle_.ctx();
            auto j = ctx.app.journal("ExtVM");
            go(depth, e, j);  
            e.accrueSubState(sub);
        }
        ioGas = e.gas();
        return{ terToEvmcStatusCode(e.getException()), e.takeOutput(), toEvmC(e.newAddress()) };
    }

    CallResult ExtVM::call(CallParameters& oPara)
    {
        //CallResult ret(EVMC_SUCCESS, owning_bytes_ref());

        Executive e(oSle_, envInfo(), depth + 1);
		CallParametersR p(oPara);
        if (!e.call(p, fromEvmC(gasPrice), fromEvmC(origin)))
        {
            ApplyContext const& ctx = oSle_.ctx();
            auto j = ctx.app.journal("ExtVM");
            go(depth, e, j);
            e.accrueSubState(sub);
        }
        oPara.gas = e.gas();

        return{ terToEvmcStatusCode(e.getException()), e.takeOutput() };
    }

    void ExtVM::log(evmc_uint256be const* topics, size_t numTopics, bytesConstRef const& data) 
    {
        ApplyContext const& ctx = oSle_.ctx();
        auto j = ctx.app.journal("ExtVM");
		if (ctx.view().flags() & tapForConsensus)
		{
			oSle_.PubContractEvents(fromEvmC(myAddress), fromEvmC(topics), numTopics, data.toBytes());
		}

        JLOG(j.trace()) << data.toString();
    }
    
    int64_t ExtVM::executeSQL(evmc_address const* _addr, uint8_t _type, bytesConstRef const& _name, bytesConstRef const& _raw)
    {
        ApplyContext const& ctx = oSle_.ctx();
        auto j = ctx.app.journal("ExtVM");
        TableOpType eType = T_COMMON;
        switch (_type)
        {
        case 0:            return eType = T_CREATE;
        case 1:            return eType = R_INSERT;
        case 2:            return eType = R_UPDATE;
        case 3:            return eType = R_DELETE;
        case 4:            return eType = R_GET;
        default:           return eType = T_COMMON;
        }
        int64_t iRet = oSle_.executeSQL(fromEvmC(myAddress), fromEvmC(*_addr), eType, _name.toString(), _raw.toString());

        JLOG(j.trace()) <<"tableName is "<< _name.toString() << ", raw is " << _raw.toString();
        return iRet;
    }

    evmc_uint256be ExtVM::blockHash(int64_t  const& iSeq)
    {
        uint256 uHash = beast::zero;

        ApplyContext const& ctx = oSle_.ctx();
        auto ledger = ctx.app.getLedgerMaster().getLedgerBySeq(iSeq);
        
        if (ledger != nullptr)
        {
            uHash = ledger->info().hash;
        }

        return toEvmC(uHash);
    }
}