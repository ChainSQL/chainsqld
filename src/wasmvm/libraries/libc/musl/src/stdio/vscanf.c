#include <stdio.h>
#include <stdarg.h>
#include "libc.h"

int vscanf(const char *restrict fmt, va_list ap)
{
	return vfscanf(stdin, fmt, ap);
}

#ifdef __APPLE__
   int __isoc99_vscanf(const char *restrict fmt, va_list ap) {
      return vscanf(fmt,ap);
   }
#else
   weak_alias(vscanf,__isoc99_vscanf);
#endif
