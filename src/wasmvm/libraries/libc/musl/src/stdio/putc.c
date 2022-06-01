#include "stdio_impl.h"

int putc(int c, FILE *f)
{
	if (f->lock < 0 || !__lockfile(f))
		return putc_unlocked(c, f);
	c = putc_unlocked(c, f);
	__unlockfile(f);
	return c;
}

#ifdef __APPLE__
   int _IO_putc(int c, FILE *f) {
      return putc(c,f);
   }
#else
   weak_alias(putc, _IO_putc);
#endif
