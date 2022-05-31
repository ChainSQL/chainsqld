#include <wctype.h>
#include "libc.h"

int iswalnum(wint_t wc)
{
	return iswdigit(wc) || iswalpha(wc);
}

int __iswalnum_l(wint_t c, locale_t l)
{
	return iswalnum(c);
}

#ifdef __APPLE__
   int iswalnum_l(wint_t c, locale_t l)
   {
      return __iswalnum_l(c,l);
   }

#else
   weak_alias(__iswalnum_l, iswalnum_l);
#endif
