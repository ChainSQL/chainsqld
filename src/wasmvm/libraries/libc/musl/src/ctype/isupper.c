#include <ctype.h>
#include "libc.h"
#undef isupper

int isupper(int c)
{
	return (unsigned)c-'A' < 26;
}

int __isupper_l(int c, locale_t l)
{
	return isupper(c);
}

#ifdef __APPLE__
   int isupper_l(int c, locale_t l) {
      return __isupper_l(c,l);
   }
#else
   weak_alias(__isupper_l, isupper_l);
#endif
