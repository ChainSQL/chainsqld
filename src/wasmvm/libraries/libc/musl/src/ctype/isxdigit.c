#include <ctype.h>
#include "libc.h"

int isxdigit(int c)
{
	return isdigit(c) || ((unsigned)c|32)-'a' < 6;
}

int __isxdigit_l(int c, locale_t l)
{
	return isxdigit(c);
}

#ifdef __APPLE__
   int isxdigit_l(int c, locale_t l) {
      return __isxdigit_l(c,l);
   }
#else
   weak_alias(__isxdigit_l, isxdigit_l);
#endif
