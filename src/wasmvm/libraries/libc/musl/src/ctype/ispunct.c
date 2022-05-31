#include <ctype.h>
#include "libc.h"

int ispunct(int c)
{
	return isgraph(c) && !isalnum(c);
}

int __ispunct_l(int c, locale_t l)
{
	return ispunct(c);
}

#ifdef __APPLE__
   int ispunct_l(int c, locale_t l) {
      return __ispunct_l(c,l);
   }
#else
   weak_alias(__ispunct_l, ispunct_l);
#endif
