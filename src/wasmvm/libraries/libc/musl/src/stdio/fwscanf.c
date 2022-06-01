#include <stdio.h>
#include <stdarg.h>
#include <wchar.h>
#include "libc.h"

int fwscanf(FILE *restrict f, const wchar_t *restrict fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vfwscanf(f, fmt, ap);
	va_end(ap);
	return ret;
}

#ifdef __APPLE__
   int __isoc99_fwscanf(FILE *restrict f, const wchar_t *restrict fmt, ...)
   {
      int ret;
      va_list ap;
      va_start(ap, fmt);
      ret = vfwscanf(f, fmt, ap);
      va_end(ap);
      return ret;
   }
#else
   weak_alias(fwscanf,__isoc99_fwscanf);
#endif
