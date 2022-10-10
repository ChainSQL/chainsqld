#include "VMC.h"

namespace eth {

//VM::VM(evmc_vm* instance) noexcept 
//: m_instance(instance) {
//	assert(m_instance != nullptr);
//	assert(m_instance->abi_version == EVMC_ABI_VERSION);
//
//	// Set the options.
//	//for (auto& pair : evmcOptions())
//	//	m_instance->set_option(m_instance, pair.first.c_str(), pair.second.c_str());
//}

owning_bytes_ref VMC::exec(int64_t& gas, ExtVMFace& ext) {
	constexpr int64_t int64max = std::numeric_limits<int64_t>::max();
	(void)int64max;
	assert(gas <= int64max);
	assert(ext.envInfo().gasLimit() <= int64max);
	assert(ext.depth <= static_cast<size_t>(std::numeric_limits<int32_t>::max()));

	evmc_call_kind kind = ext.isCreate ? EVMC_CREATE : EVMC_CALL;
	uint32_t flags = ext.staticCall ? EVMC_STATIC : 0;
	assert(flags != EVMC_STATIC || kind == EVMC_CALL);  // STATIC implies a CALL.

	evmc_message msg = { kind, flags, ext.depth, gas,
		ext.myAddress, ext.caller,
		ext.data.data(), ext.data.size(), ext.value,
		ext.envInfo().dropsPerByte(), {} };
	EvmCHost host{ ext };

	//return Result{
		//m_instance->execute(m_instance, &evmc::Host::get_interface(), host.to_context(),
		//EVMC_CONSTANTINOPLE, &msg, ext.code.data(), ext.code.size())
		/*m_instance->execute(m_instance, &ext, EVMC_CONSTANTINOPLE,
			&msg, ext.code.data(), ext.code.size())*/
	//};

	auto r = execute(host, EVMC_ISTANBUL, msg, ext.code.data(), ext.code.size());
	// FIXME: Copy the output for now, but copyless version possible.
	auto output = owning_bytes_ref{ {&r.output_data[0], &r.output_data[r.output_size]}, 0, r.output_size };
	
	switch (r.status_code)
	{
	case EVMC_SUCCESS:
		gas = r.gas_left;
		// FIXME: Copy the output for now, but copyless version possible.
		return output;
	case EVMC_REVERT:
		gas = r.gas_left;
		throw RevertInstruction{ std::move(output) };
	case EVMC_REVERTDIY:
		gas = r.gas_left;
		throw RevertDiyInstruction{ std::move(output) };
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
		BOOST_THROW_EXCEPTION(InternalVMError{} << errinfo_evmcStatusCode(r.status_code));
		break;
	}
}

} // namespace ripple