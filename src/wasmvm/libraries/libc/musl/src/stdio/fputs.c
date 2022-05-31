#include "stdio_impl.h"
#include <string.h>

int fputs(const char *restrict s, FILE *restrict f)
{
	size_t l = strlen(s);
	return (fwrite(s, 1, l, f)==l) - 1;
}

#ifdef __APPLE__
   int fputs_unlocked(const char *restrict s, FILE *restrict f) {
      return fputs(s,f);
   }
#else
   weak_alias(fputs, fputs_unlocked);
#endif
