#include "stdio_impl.h"

void clearerr(FILE *f)
{
	FLOCK(f);
	f->flags &= ~(F_EOF|F_ERR);
	FUNLOCK(f);
}

#ifdef __APPLE__
  void clearerr_unlocked(FILE* f) {
     return clearerr(f);
  } 
#else
   weak_alias(clearerr, clearerr_unlocked);
#endif
