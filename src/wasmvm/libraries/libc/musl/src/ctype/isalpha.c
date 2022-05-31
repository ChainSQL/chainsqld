#include <ctype.h>
#include "libc.h"
#undef isalpha

int isalpha(int c)
{
	return ((unsigned)c|32)-'a' < 26;
}

int __isalpha_l(int c, locale_t l)
{
	return isalpha(c);
}

#ifdef __APPLE__
   int isalpha_l(int c, locale_t l) {
      return __isalpha_l(c,l);
   }
#else
   weak_alias(__isalpha_l, isalpha_l);
#endif
