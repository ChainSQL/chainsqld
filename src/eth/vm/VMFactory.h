#ifndef __H_CHAINSQL_VM_VMFACTORY_H__
#define __H_CHAINSQL_VM_VMFACTORY_H__

#include "VMFace.h"

namespace eth {

enum class VMKind {
	Interpreter,
	JIT
};

class VMFactory {
public:
	VMFactory() = delete;
	~VMFactory() = delete;

	static VMFace::pointer create(VMKind kind);
};

} // namespace ripple

#endif // __H_CHAINSQL_VM_VMFACTORY_H__