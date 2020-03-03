#ifndef _H_CHAINSQL_VMFACE_H__
#define _H_CHAINSQL_VMFACE_H__

#include <memory>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/exception/exception.hpp>

#include "Common.h"
#include "ExtVMFace.h"

namespace ripple {

/// Base class for all exceptions.
struct Exception : virtual std::exception, virtual boost::exception
{
	const char* what() const noexcept override { return boost::diagnostic_information_what(*this); }
};

struct VMException : Exception {};
#define ETH_SIMPLE_EXCEPTION_VM(X) struct X: VMException { const char* what() const noexcept override { return #X; } }
ETH_SIMPLE_EXCEPTION_VM(BadInstruction);
ETH_SIMPLE_EXCEPTION_VM(BadJumpDestination);
ETH_SIMPLE_EXCEPTION_VM(OutOfGas);
ETH_SIMPLE_EXCEPTION_VM(OutOfStack);
ETH_SIMPLE_EXCEPTION_VM(StackUnderflow);
ETH_SIMPLE_EXCEPTION_VM(DisallowedStateChange);
ETH_SIMPLE_EXCEPTION_VM(BufferOverrun);
ETH_SIMPLE_EXCEPTION_VM(RejectJIT);

/// Reports VM internal error. This is not based on VMException because it must be handled
/// differently than defined consensus exceptions.
struct InternalVMError : Exception {};

/// Error info for EVMC status code.
using errinfo_evmcStatusCode = boost::error_info<struct tag_evmcStatusCode, evmc_status_code>;

struct RevertInstruction : VMException
{
	explicit RevertInstruction(owning_bytes_ref&& _output) : m_output(std::move(_output)) {}
	RevertInstruction(RevertInstruction const&) = delete;
	RevertInstruction(RevertInstruction&&) = default;
	RevertInstruction& operator=(RevertInstruction const&) = delete;
	RevertInstruction& operator=(RevertInstruction&&) = default;

	char const* what() const noexcept override { return "Revert instruction"; }

	owning_bytes_ref&& output() { return std::move(m_output); }

private:
	owning_bytes_ref m_output;
};

//Diy exception,to throw real ter
struct RevertDiyInstruction :VMException
{
	explicit RevertDiyInstruction(owning_bytes_ref&& _output) : m_output(std::move(_output)) {}
	RevertDiyInstruction(RevertDiyInstruction const&) = delete;
	RevertDiyInstruction(RevertDiyInstruction&&) = default;
	RevertDiyInstruction& operator=(RevertDiyInstruction const&) = delete;
	RevertDiyInstruction& operator=(RevertDiyInstruction&&) = default;

	char const* what() const noexcept override { return "RevertDiy instruction"; }

	owning_bytes_ref&& output() { return std::move(m_output); }

private:
	owning_bytes_ref m_output;
};

class VMFace {
public:
	using pointer = std::shared_ptr<VMFace>;

	VMFace() = default;
	virtual ~VMFace() = default;
	VMFace(VMFace const&) = delete;
	VMFace& operator=(VMFace const&) = delete;

	virtual owning_bytes_ref exec(int64_t& gas, ExtVMFace& ext) = 0;
};

}

#endif // !_H_CHAINSQL_VMFACE_H__
