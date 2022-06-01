#include <wctype.h>
#include <ctype.h>
#include "libc.h"

int iswblank(wint_t wc)
{
	return isblank(wc);
}

int __iswblank_l(wint_t c, locale_t l)
{
	return iswblank(c);
}

#ifdef __APPLE__
   int iswblank_l(wint_t c, locale_t l) {
      return __iswblank_l(c,l);
   }
#else
   weak_alias(__iswblank_l, iswblank_l);
#endif
