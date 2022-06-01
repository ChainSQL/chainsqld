#include "stdio_impl.h"
#include "libc.h"

static size_t do_read(FILE *f, unsigned char *buf, size_t len)
{
	return __string_read(f, buf, len);
}

int vsscanf(const char *restrict s, const char *restrict fmt, va_list ap)
{
	FILE f = {
		.buf = (void *)s, .cookie = (void *)s,
		.read = do_read, .lock = -1
	};
	return vfscanf(&f, fmt, ap);
}

#ifdef __APPLE__
   int __isoc99_vsscanf(const char *restrict s, const char *restrict fmt, va_list ap) {
      return vsscanf(s,fmt,ap);
   }
#else
   weak_alias(vsscanf,__isoc99_vsscanf);
#endif
