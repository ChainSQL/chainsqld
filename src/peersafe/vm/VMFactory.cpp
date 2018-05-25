#include "VMFactory.h"
#include "VMC.h"

#include <evmjit.h>

namespace ripple {

VMFace::pointer VMFactory::create(VMKind kind) {
	switch (kind)
	{
	case VMKind::JIT:
		return VMFace::pointer(new VMC{evmjit_create()});
	default:
		return VMFace::pointer(new VMC{evmjit_create()});
	}
}

} // namespace ripple