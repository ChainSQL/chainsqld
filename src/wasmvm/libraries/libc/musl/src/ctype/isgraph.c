#include <ctype.h>
#include "libc.h"
#undef isgraph

int isgraph(int c)
{
	return (unsigned)c-0x21 < 0x5e;
}

int __isgraph_l(int c, locale_t l)
{
	return isgraph(c);
}

#ifdef __APPLE__
   int isgraph_l(int c, locale_t l) {
      return __isgraph_l(c,l);
   }
#else
   weak_alias(__isgraph_l, isgraph_l);
#endif
