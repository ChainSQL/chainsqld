#include <wctype.h>
#include "libc.h"

#undef iswdigit

int iswdigit(wint_t wc)
{
	return (unsigned)wc-'0' < 10;
}

int __iswdigit_l(wint_t c, locale_t l)
{
	return iswdigit(c);
}

#ifdef __APPLE__
   int iswdigit_l(wint_t c, locale_t l) {
      return __iswdigit_l(c,l);
   }
#else
   weak_alias(__iswdigit_l, iswdigit_l);
#endif
