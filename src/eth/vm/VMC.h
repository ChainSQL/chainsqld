#ifndef __H_CHAINSQL_VMC_H__
#define __H_CHAINSQL_VMC_H__

#include <memory>
#include <assert.h>

//#include <evmjit/evmc/include/evmc/evmc.h>
#include <eth/evmc/include/evmc/evmc.h>

#include "VMFace.h"

namespace eth {

class VM {
public:
	explicit VM(evmc_vm* _instance) noexcept;

	~VM() { m_instance->destroy(m_instance); }

	VM(VM const&) = delete;
	VM& operator=(VM) = delete;

	class Result
	{
	public:
		explicit Result(evmc_result const& _result) :
			m_result(_result)
		{}

		~Result()
		{
			if (m_result.release)
				m_result.release(&m_result);
		}

		Result(Result&& _other) noexcept:
		m_result(_other.m_result)
		{
			// Disable releaser of the rvalue object.
			_other.m_result.release = nullptr;
		}

		Result(Result const&) = delete;
		Result& operator=(Result const&) = delete;

		evmc_status_code status() const
		{
			return m_result.status_code;
		}

		int64_t gasLeft() const
		{
			return m_result.gas_left;
		}

		bytesConstRef output() const
		{
			return{ m_result.output_data, m_result.output_size };
		}

	private:
		evmc_result m_result;
	};

	Result execute(ExtVMFace& ext, int64_t gas) {
		evmc_call_kind kind = ext.isCreate ? EVMC_CREATE : EVMC_CALL;
		uint32_t flags = ext.staticCall ? EVMC_STATIC : 0;
		assert(flags != EVMC_STATIC || kind == EVMC_CALL);  // STATIC implies a CALL.

		evmc_message msg = { kind, flags, ext.depth, gas,
			ext.myAddress, ext.caller, 
			ext.data.data(), ext.data.size(), ext.value, 
			ext.envInfo().dropsPerByte()};
		EvmCHost host{ ext };
		return Result{
			m_instance->execute(m_instance, &evmc::Host::get_interface(), host.to_context(),
			EVMC_CONSTANTINOPLE, &msg, ext.code.data(), ext.code.size())
			/*m_instance->execute(m_instance, &ext, EVMC_CONSTANTINOPLE,
				&msg, ext.code.data(), ext.code.size())*/
		};
	}

private:
	struct evmc_vm* m_instance = nullptr;
};

class VMC : public VM, public VMFace {
public:
	explicit VMC(struct evmc_vm* instance) :VM(instance) {};
	
	owning_bytes_ref exec(int64_t& gas, ExtVMFace& ext) final;
};

}

#endif // !__H_CHAINSQL_VMC_H__
