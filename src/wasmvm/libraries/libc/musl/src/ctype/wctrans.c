#include <wctype.h>
#include <string.h>
#include "libc.h"

wctrans_t wctrans(const char *class)
{
	if (!strcmp(class, "toupper")) return (wctrans_t)1;
	if (!strcmp(class, "tolower")) return (wctrans_t)2;
	return 0;
}

wint_t towctrans(wint_t wc, wctrans_t trans)
{
	if (trans == (wctrans_t)1) return towupper(wc);
	if (trans == (wctrans_t)2) return towlower(wc);
	return wc;
}

wctrans_t __wctrans_l(const char *s, locale_t l)
{
	return wctrans(s);
}

wint_t __towctrans_l(wint_t c, wctrans_t t, locale_t l)
{
	return towctrans(c, t);
}

#ifdef __APPLE__
   wctrans_t wctrans_l(const char *s, locale_t l)
   {
      return __wctrans_l(s,l);
   }

   wint_t towctrans_l(wint_t c, wctrans_t t, locale_t l)
   {
      return __towctrans_l(c,t,l);
   }
#else
   weak_alias(__wctrans_l, wctrans_l);
   weak_alias(__towctrans_l, towctrans_l);
#endif
