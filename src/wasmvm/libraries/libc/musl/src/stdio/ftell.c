#include "stdio_impl.h"
#include <limits.h>
#include <errno.h>

off_t __ftello_unlocked(FILE *f)
{
	off_t pos = f->seek(f, 0,
		(f->flags & F_APP) && f->wpos > f->wbase
		? SEEK_END : SEEK_CUR);
	if (pos < 0) return pos;

	/* Adjust for data in buffer. */
	return pos - (f->rend - f->rpos) + (f->wpos - f->wbase);
}

off_t __ftello(FILE *f)
{
	off_t pos;
	FLOCK(f);
	pos = __ftello_unlocked(f);
	FUNLOCK(f);
	return pos;
}

long ftell(FILE *f)
{
	off_t pos = __ftello(f);
	if (pos > LONG_MAX) {
		errno = EOVERFLOW;
		return -1;
	}
	return pos;
}

#ifdef __APPLE__
   off_t ftello(FILE *f) {
      return __ftello(f);
   }
   off_t ftello64(FILE *f) {
      return __ftello(f);
   }
#else
   weak_alias(__ftello, ftello);
   LFS64(ftello);
#endif

