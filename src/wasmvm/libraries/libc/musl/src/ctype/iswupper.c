#include <wctype.h>
#include "libc.h"

int iswupper(wint_t wc)
{
	return towlower(wc) != wc;
}

int __iswupper_l(wint_t c, locale_t l)
{
	return iswupper(c);
}

#ifdef __APPLE__
   int iswupper_l(wint_t c, locale_t l) {
      return __iswupper_l(c,l);
   }
#else
   weak_alias(__iswupper_l, iswupper_l);
#endif
