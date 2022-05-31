#include <string.h>
#include "libc.h"

void *__memrchr(const void *m, int c, size_t n)
{
	const unsigned char *s = m;
	c = (unsigned char)c;
	while (n--) if (s[n]==c) return (void *)(s+n);
	return 0;
}

#ifdef __APPLE__
   void *memrchr(const void *m, int c, size_t n) {
      return __memrchr(m,c,n);
   }
#else
   weak_alias(__memrchr, memrchr);
#endif
