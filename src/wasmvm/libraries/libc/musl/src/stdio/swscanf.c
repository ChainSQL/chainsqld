#include <stdarg.h>
#include <wchar.h>
#include "libc.h"

int swscanf(const wchar_t *restrict s, const wchar_t *restrict fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vswscanf(s, fmt, ap);
	va_end(ap);
	return ret;
}

#ifdef __APPLE__
   int __isoc99_swscanf(const wchar_t *restrict s, const wchar_t *restrict fmt, ...)
   {
      int ret;
      va_list ap;
      va_start(ap, fmt);
      ret = vswscanf(s, fmt, ap);
      va_end(ap);
      return ret;
   }
#else
   weak_alias(swscanf,__isoc99_swscanf);
#endif
