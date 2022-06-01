#include "stdio_impl.h"

int fgetpos(FILE *restrict f, fpos_t *restrict pos)
{
	off_t off = __ftello(f);
	if (off < 0) return -1;
	*(off_t *)pos = off;
	return 0;
}

#ifdef __APPLE__
   int fgetpos64(FILE *restrict f, fpos_t *restrict pos) {
      return fgetpos(f,pos);
   }
#else
   LFS64(fgetpos);
#endif
