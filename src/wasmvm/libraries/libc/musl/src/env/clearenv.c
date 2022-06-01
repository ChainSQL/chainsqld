#define _GNU_SOURCE
#include <stdlib.h>
#include "libc.h"

static void dummy(char *old, char *new) {}

#ifdef __APPLE__
   static void __env_rm_add(char *old, char *new) {}
#else
   weak_alias(dummy, __env_rm_add);
#endif

int clearenv()
{
	char **e = __environ;
	__environ = 0;
	if (e) while (*e) __env_rm_add(*e++, 0);
	return 0;
}
