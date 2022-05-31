#include <ctype.h>
#include "libc.h"
#undef isprint

int isprint(int c)
{
	return (unsigned)c-0x20 < 0x5f;
}

int __isprint_l(int c, locale_t l)
{
	return isprint(c);
}

#ifdef __APPLE__
   int isprint_l(int c, locale_t l) {
      return __isprint_l(c,l);
   }
#else
   weak_alias(__isprint_l, isprint_l);
#endif
