#include "stdio_impl.h"

static int dummy(int fd)
{
	return fd;
}

#ifdef __APPLE__
   static int __aio_close(int fd)
   {
      return fd;
   }
#else
   weak_alias(dummy, __aio_close);
#endif

int __stdio_close(FILE *f)
{
   return 0;
   //return syscall(SYS_close, __aio_close(f->fd));
}
