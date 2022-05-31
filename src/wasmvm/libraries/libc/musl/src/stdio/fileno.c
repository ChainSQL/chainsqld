#include "stdio_impl.h"

int fileno(FILE *f)
{
	/* f->fd never changes, but the lock must be obtained and released
	 * anyway since this function cannot return while another thread
	 * holds the lock. */
	FLOCK(f);
	FUNLOCK(f);
	return f->fd;
}

#ifdef __APPLE__
   int fileno_unlocked(FILE *f) {
      return fileno(f);
   }
#else
   weak_alias(fileno, fileno_unlocked);
#endif
