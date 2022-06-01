#include <stdlib.h>
#include "libc.h"

static void dummy() { }

#ifdef __APPLE__
   static void __funcs_on_quick_exit() {}
#else
   weak_alias(dummy, __funcs_on_quick_exit);
#endif

_Noreturn void quick_exit(int code)
{
	__funcs_on_quick_exit();
	_Exit(code);
}
