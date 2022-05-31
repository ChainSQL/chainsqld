#include <ctype.h>
#include "libc.h"
#undef isspace

int isspace(int c)
{
	return c == ' ' || (unsigned)c-'\t' < 5;
}

int __isspace_l(int c, locale_t l)
{
	return isspace(c);
}

#ifdef __APPLE__
   int isspace_l(int c, locale_t l) {
      return __isspace_l(c,l);
   }
#else
   weak_alias(__isspace_l, isspace_l);
#endif
