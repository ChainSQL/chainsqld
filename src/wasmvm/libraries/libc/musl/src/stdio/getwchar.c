#include "stdio_impl.h"
#include <wchar.h>

wint_t getwchar(void)
{
	return fgetwc(stdin);
}

#ifdef __APPLE__
   wint_t getwchar_unlocked(void) {
      return fgetwc(stdin);
   }
#else
   weak_alias(getwchar, getwchar_unlocked);
#endif
