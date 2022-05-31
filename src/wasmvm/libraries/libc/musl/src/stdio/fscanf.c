#include <stdio.h>
#include <stdarg.h>
#include "libc.h"

int fscanf(FILE *restrict f, const char *restrict fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vfscanf(f, fmt, ap);
	va_end(ap);
	return ret;
}

#ifdef __APPLE__
   int __isoc99_fscanf(FILE *restrict f, const char *restrict fmt, ...)
   {
      int ret;
      va_list ap;
      va_start(ap, fmt);
      ret = vfscanf(f, fmt, ap);
      va_end(ap);
      return ret;
   }
#else
   weak_alias(fscanf, __isoc99_fscanf);
#endif
