#include "VMFactory.h"
#include "VMC.h"

#include <evmjit.h>
#include <eth/vm/executor/interpreter/interpreter.h>

namespace eth {

VMFace::pointer VMFactory::create(VMKind kind) {
	switch (kind)
	{
	case VMKind::JIT:
		return VMFace::pointer();
		//return VMFace::pointer(new VMC{evmjit_create()});
	case VMKind::Interpreter:
	default:
		return VMFace::pointer(new VMC{ evmc_create_aleth_interpreter() });
	}
}

} // namespace ripple