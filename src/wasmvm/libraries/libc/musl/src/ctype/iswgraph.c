#include <wctype.h>
#include "libc.h"

int iswgraph(wint_t wc)
{
	/* ISO C defines this function as: */
	return !iswspace(wc) && iswprint(wc);
}

int __iswgraph_l(wint_t c, locale_t l)
{
	return iswgraph(c);
}

#ifdef __APPLE__
   int iswgraph_l(wint_t c, locale_t l) {
      return __iswgraph_l(c,l);
   }
#else
   weak_alias(__iswgraph_l, iswgraph_l);
#endif
