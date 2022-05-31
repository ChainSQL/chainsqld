#include "stdio_impl.h"

#undef ferror

int ferror(FILE *f)
{
	FLOCK(f);
	int ret = !!(f->flags & F_ERR);
	FUNLOCK(f);
	return ret;
}

#ifdef __APPLE__
   int ferror_unlocked(FILE *f) {
      return ferror(f);
   }
   int _IO_ferror_unlocked(FILE *f) {
      return ferror(f);
   }
#else
   weak_alias(ferror, ferror_unlocked);
   weak_alias(ferror, _IO_ferror_unlocked);
#endif
