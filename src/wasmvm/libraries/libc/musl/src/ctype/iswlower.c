#include <wctype.h>
#include "libc.h"

int iswlower(wint_t wc)
{
	return towupper(wc) != wc;
}

int __iswlower_l(wint_t c, locale_t l)
{
	return iswlower(c);
}

#ifdef __APPLE__
   int iswlower_l(wint_t c, locale_t l) {
      return __iswlower_l(c,l);
   }
#else
   weak_alias(__iswlower_l, iswlower_l);
#endif
