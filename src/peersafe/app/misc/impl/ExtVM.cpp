#include <peersafe/app/misc/ExtVM.h>
#include <peersafe/app/misc/Executive.h>
#include <peersafe/protocol/STMap256.h>
#include <ripple/protocol/ZXCAmount.h>
#include <ripple/app/ledger/LedgerMaster.h>

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

    evmc_uint256be ExtVM::balance(evmc_address const& addr)
    {        
        SLE::pointer pSle = oSle_.getSle(addr);

        auto& stBalance = pSle->getFieldAmount(sfBalance);        
        std::int64_t i64Drops = stBalance.zxc().drops();

        return toEvmC(uint256(i64Drops));
    }

    evmc_uint256be ExtVM::store(evmc_uint256be const& key)
    {           
        SLE::pointer pSle = oSle_.getSle(myAddress);
        STMap256& mapStore = pSle->peekFieldM256(sfStorageOverlay);

        uint256 uKey = fromEvmC(key);        
        uint256& uV = mapStore.at(uKey);
        return toEvmC(uV);
    }

    void ExtVM::setStore(evmc_uint256be const& key, evmc_uint256be const& value)
    {
        SLE::pointer pSle = oSle_.getSle(myAddress);
        STMap256& mapStore = pSle->peekFieldM256(sfStorageOverlay);
                
        uint256 uKey = fromEvmC(key);
        uint256 uValue = fromEvmC(value);
        mapStore[uKey] = uValue;
    }

    bytes const& ExtVM::codeAt(evmc_address const& addr)
    { 
        SLE::pointer pSle = oSle_.getSle(addr);
        Blob blobCode = pSle->getFieldVL(sfContractCode);

        return blobCode; 
    }

    size_t ExtVM::codeSizeAt(evmc_address const& addr) 
    { 
        SLE::pointer pSle = oSle_.getSle(addr);
        Blob blobCode = pSle->getFieldVL(sfContractCode);

        return blobCode.size();
    }

    bool ExtVM::exists(evmc_address const& addr) { 
        SLE::pointer pSle = oSle_.getSle(addr);
        return pSle != nullptr;
    }

    void ExtVM::suicide(evmc_address const& addr) 
    {
        SLE::pointer sleContract = oSle_.getSle(addr);
        SLE::pointer sleMy = oSle_.getSle(myAddress);

        auto& stBalanceContract = sleContract->getFieldAmount(sfBalance);
        auto& stBalanceMy = sleMy->getFieldAmount(sfBalance);
        sleMy->setFieldAmount(sfBalance, stBalanceContract + stBalanceMy);

        ExtVMFace::suicide(addr);
    }

    CreateResult ExtVM::create(evmc_uint256be const&, int64_t const&, bytesConstRef const&, Instruction, evmc_uint256be const&)
    {
        CreateResult ret(EVMC_SUCCESS, owning_bytes_ref(),evmc_address());
        return ret;
    }

    CallResult ExtVM::call(CallParameters&)
    {
        CallResult ret(EVMC_SUCCESS, owning_bytes_ref());
        return ret;
    }

    void ExtVM::log(evmc_uint256be const* /*topics*/, size_t /*numTopics*/, bytesConstRef const& data) 
    {
        ApplyContext& ctx = oSle_.getContex();
        auto j = ctx.app.journal("ExtVM");

        JLOG(j.trace()) << data.toString();
    }
    
    evmc_uint256be ExtVM::blockHash(int64_t  const& iSeq)
    {
        uint256 uHash = beast::zero;

        ApplyContext& ctx = oSle_.getContex();                
        auto ledger = ctx.app.getLedgerMaster().getLedgerBySeq(iSeq);
        
        if (ledger != nullptr)
        {
            uHash = ledger->info().hash;
        }

        return toEvmC(uHash);
    }

/*
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

void goOnOffloadedStack(Executive& _e, OnOpFunc const& _onOp)
{
	// Set new stack size enouth to handle the rest of the calls up to the limit.
	boost::thread::attributes attrs;
	attrs.set_stack_size((c_depthLimit - c_offloadPoint) * c_singleExecutionStackSize);

	// Create new thread with big stack and join immediately.
	// TODO: It is possible to switch the implementation to Boost.Context or similar when the API is stable.
	boost::exception_ptr exception;
	boost::thread{attrs, [&]{
		try
		{
			_e.go(_onOp);
		}
		catch (...)
		{
			exception = boost::current_exception(); // Catch all exceptions to be rethrown in parent thread.
		}
	}}.join();
	if (exception)
		boost::rethrow_exception(exception);
}

void go(unsigned _depth, Executive& _e, OnOpFunc const& _onOp)
{
	// If in the offloading point we need to switch to additional separated stack space.
	// Current stack is too small to handle more CALL/CREATE executions.
	// It needs to be done only once as newly allocated stack space it enough to handle
	// the rest of the calls up to the depth limit (c_depthLimit).

	if (_depth == c_offloadPoint)
	{
		cnote << "Stack offloading (depth: " << c_offloadPoint << ")";
		goOnOffloadedStack(_e, _onOp);
	}
	else
		_e.go(_onOp);
}

evmc_status_code transactionExceptionToEvmcStatusCode(TransactionException ex) noexcept
{
    
    switch (ex)
    {
    case TransactionException::None:
        return EVMC_SUCCESS;

    case TransactionException::RevertInstruction:
        return EVMC_REVERT;

    case TransactionException::OutOfGas:
        return EVMC_OUT_OF_GAS;

    case TransactionException::BadInstruction:
        return EVMC_UNDEFINED_INSTRUCTION;

    case TransactionException::OutOfStack:
        return EVMC_STACK_OVERFLOW;

    case TransactionException::StackUnderflow:
        return EVMC_STACK_UNDERFLOW;

    case TransactionException ::BadJumpDestination:
        return EVMC_BAD_JUMP_DESTINATION;

    default:
        return EVMC_FAILURE;
    }
    
    return EVMC_FAILURE;
}

} // anonymous namespace


CallResult ExtVM::call(CallParameters& _p)
{
    Executive e{m_s, envInfo(), m_sealEngine, depth + 1};
    if (!e.call(_p, gasPrice, origin))
    {
        go(depth, e, _p.onOp);
        e.accrueSubState(sub);
    }
    _p.gas = e.gas();

    return {transactionExceptionToEvmcStatusCode(e.getException()), e.takeOutput()};
}



CreateResult ExtVM::create(u256 _endowment, u256& io_gas, bytesConstRef _code, Instruction _op, u256 _salt, OnOpFunc const& _onOp)
{
	Executive e{m_s, envInfo(), m_sealEngine, depth + 1};
	bool result = false;
	if (_op == Instruction::CREATE)
		result = e.createOpcode(myAddress, _endowment, gasPrice, io_gas, _code, origin);
	else
		result = e.create2Opcode(myAddress, _endowment, gasPrice, io_gas, _code, origin, _salt);

	if (!result)
	{
		go(depth, e, _onOp);
		e.accrueSubState(sub);
	}
	io_gas = e.gas();
	return {transactionExceptionToEvmcStatusCode(e.getException()), e.takeOutput(), e.newAddress()};
}

*/
}