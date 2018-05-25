#include <peersafe/app/misc/ExtVM.h>
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
    /*
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
    */
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

size_t ExtVM::codeSizeAt(dev::Address _a)
{
	return m_s.codeSize(_a);
}

u256 ExtVM::store(u256 _n)
{
    m_s.storage(myAddress, _n);
}

void ExtVM::setStore(u256 _n, u256 _v)
{
	m_s.setStorage(myAddress, _n, _v);
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

void ExtVM::suicide(Address _a)
{
    //how to suicide in zxc net???
    ExtVMFace::suicide(_a);
}

h256 ExtVM::blockHash(u256 _number)
{
	u256 const currentNumber = envInfo().number();

	if (_number >= currentNumber || _number < (std::max<u256>(256, currentNumber) - 256))
		return h256();

	if (currentNumber < m_sealEngine.chainParams().constantinopleForkBlock + 256)
	{
		h256 const parentHash = envInfo().header().parentHash();
		h256s const lastHashes = envInfo().lastHashes().precedingHashes(parentHash);

		assert(lastHashes.size() > (unsigned)(currentNumber - 1 - _number));
		return lastHashes[(unsigned)(currentNumber - 1 - _number)];
	}

	u256 const nonce = m_s.getNonce(caller);
	u256 const gas = 1000000;
	Transaction tx(0, 0, gas, c_blockhashContractAddress, toBigEndian(_number), nonce);
	tx.forceSender(caller);

	ExecutionResult res;
	std::tie(res, std::ignore) = m_s.execute(envInfo(), m_sealEngine, tx, Permanence::Reverted);
	return h256(res.output);
}
