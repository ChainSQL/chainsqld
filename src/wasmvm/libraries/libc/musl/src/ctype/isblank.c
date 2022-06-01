#include <ctype.h>
#include "libc.h"

int isblank(int c)
{
	return (c == ' ' || c == '\t');
}

int __isblank_l(int c, locale_t l)
{
	return isblank(c);
}

#ifdef __APPLE__
   int isblank_l(int c, locale_t l) {
      return __isblank_l(c,l);
   }
#else
   weak_alias(__isblank_l, isblank_l);
#endif
