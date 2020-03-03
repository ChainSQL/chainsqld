#include "VMC.h"

namespace ripple {

VM::VM(evmc_instance* instance) noexcept 
: m_instance(instance) {
	assert(m_instance != nullptr);
	assert(m_instance->abi_version == EVMC_ABI_VERSION);

	// Set the options.
	//for (auto& pair : evmcOptions())
	//	m_instance->set_option(m_instance, pair.first.c_str(), pair.second.c_str());
}

owning_bytes_ref VMC::exec(int64_t& gas, ExtVMFace& ext) {
	constexpr int64_t int64max = std::numeric_limits<int64_t>::max();
	(void)int64max;
	assert(gas <= int64max);
	assert(ext.depth <= static_cast<size_t>(std::numeric_limits<int32_t>::max()));

	VM::Result r = execute(ext, gas);
	switch (r.status())
	{
	case EVMC_SUCCESS:
		gas = r.gasLeft();
		// FIXME: Copy the output for now, but copyless version possible.
		return{ r.output().toVector(), 0, r.output().size() };
	case EVMC_REVERT:
		gas = r.gasLeft();
		throw RevertInstruction{ { r.output().toVector(), 0, r.output().size() } };
	case EVMC_REVERTDIY:
		gas = r.gasLeft();
		throw RevertDiyInstruction{ { r.output().toVector(), 0, r.output().size() } };
		break;
	case EVMC_OUT_OF_GAS:
	case EVMC_FAILURE:
		BOOST_THROW_EXCEPTION(OutOfGas());

	case EVMC_UNDEFINED_INSTRUCTION:
		BOOST_THROW_EXCEPTION(BadInstruction());

	case EVMC_BAD_JUMP_DESTINATION:
		BOOST_THROW_EXCEPTION(BadJumpDestination());

	case EVMC_STACK_OVERFLOW:
		BOOST_THROW_EXCEPTION(OutOfStack());

	case EVMC_STACK_UNDERFLOW:
		BOOST_THROW_EXCEPTION(StackUnderflow());

	case EVMC_STATIC_MODE_VIOLATION:
		BOOST_THROW_EXCEPTION(DisallowedStateChange());

	case EVMC_REJECTED:
		//cwarn << "Execution rejected by EVMC, executing with default VM implementation";
		//return VMFactory::create(VMKind::Legacy)->exec(io_gas, _ext, _onOp);
		BOOST_THROW_EXCEPTION(RejectJIT());
	default:
		BOOST_THROW_EXCEPTION(InternalVMError{} << errinfo_evmcStatusCode(r.status()));
		break;
	}
}

} // namespace ripple