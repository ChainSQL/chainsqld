#include "stdio_impl.h"

int fsetpos(FILE *f, const fpos_t *pos)
{
	return __fseeko(f, *(const off_t *)pos, SEEK_SET);
}

#ifdef __APPLE__
   int fsetpos64(FILE *f, const fpos_t *pos) {
      return fsetpos(f,pos);
   }
#else
   LFS64(fsetpos);
#endif
