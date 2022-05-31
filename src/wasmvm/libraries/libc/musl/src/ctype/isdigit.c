#include <ctype.h>
#include "libc.h"
#undef isdigit

int isdigit(int c)
{
	return (unsigned)c-'0' < 10;
}

int __isdigit_l(int c, locale_t l)
{
	return isdigit(c);
}

#ifdef __APPLE__
   int isdigit_l(int c, locale_t l) {
      return __isdigit_l(c,l);
   }
#else
   weak_alias(__isdigit_l, isdigit_l);
#endif
