#include "stdio_impl.h"

#undef feof

int feof(FILE *f)
{
	FLOCK(f);
	int ret = !!(f->flags & F_EOF);
	FUNLOCK(f);
	return ret;
}

#ifdef __APPLE__
   int feof_unlocked(FILE *f) {
      return feof(f);
   }
   int _IO_feof_unlocked(FILE *f) {
      return feof(f);
   }
#else
   weak_alias(feof, feof_unlocked);
   weak_alias(feof, _IO_feof_unlocked);
#endif
