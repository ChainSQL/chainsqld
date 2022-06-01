#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "libc.h"

char *__strchrnul(const char *, int);

static void dummy(char *old, char *new) {}

#ifdef __APPLE__
   static void __env_rm_add(char *old, char *new) {}
#else
   weak_alias(dummy, __env_rm_add);
#endif

int unsetenv(const char *name)
{
	size_t l = __strchrnul(name, '=') - name;
	if (!l || name[l]) {
		errno = EINVAL;
		return -1;
	}
	if (__environ) {
		char **e = __environ, **eo = e;
		for (; *e; e++)
			if (!strncmp(name, *e, l) && l[*e] == '=')
				__env_rm_add(*e, 0);
			else if (eo != e)
				*eo++ = *e;
			else
				eo++;
		if (eo != e) *eo = 0;
	}
	return 0;
}
