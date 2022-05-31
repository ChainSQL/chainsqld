#include <ctype.h>
#include "libc.h"

int tolower(int c)
{
	if (isupper(c)) return c | 32;
	return c;
}

int __tolower_l(int c, locale_t l)
{
	return tolower(c);
}

#ifdef __APPLE__
   int tolower_l(int c, locale_t l) {
      return __tolower_l(c,l);
   }
#else
   weak_alias(__tolower_l, tolower_l);
#endif
