#include <stdio.h>
#include <stdarg.h>
#include <wchar.h>
#include "libc.h"

int vwscanf(const wchar_t *restrict fmt, va_list ap)
{
	return vfwscanf(stdin, fmt, ap);
}

#ifdef __APPLE__
   int __isoc99_vwscanf(const wchar_t *restrict fmt, va_list ap) {
      return vwscanf(fmt,ap);
   }
#else
   weak_alias(vwscanf,__isoc99_vwscanf);
#endif
