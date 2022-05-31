#include <ctype.h>
#include "libc.h"

int isalnum(int c)
{
	return isalpha(c) || isdigit(c);
}

int __isalnum_l(int c, locale_t l)
{
	return isalnum(c);
}

#if __APPLE__
int isalnum_l(int c, locale_t l) {
   return __isalnum(c, l);
}
#else
weak_alias(__isalnum_l, isalnum_l);
#endif
