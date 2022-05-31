#include <stdio.h>
#include <stdarg.h>
#include "libc.h"

int scanf(const char *restrict fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vscanf(fmt, ap);
	va_end(ap);
	return ret;
}

#ifdef __APPLE__
   int __isoc99_scanf(const char *restrict fmt, ...) {
      int ret;
      va_list ap;
      va_start(ap, fmt);
      ret = vscanf(fmt, ap);
      va_end(ap);
      return ret;
   }
#else
   weak_alias(scanf,__isoc99_scanf);
#endif
