#include <ctype.h>
#include "libc.h"
#undef islower

int islower(int c)
{
	return (unsigned)c-'a' < 26;
}

int __islower_l(int c, locale_t l)
{
	return islower(c);
}

#ifdef __APPLE__
   int islower_l(int c, locale_t l) {
      return __islower_l(c,l);
   }
#else
   weak_alias(__islower_l, islower_l);
#endif
