#include "stdio_impl.h"
#include <wchar.h>

wint_t putwchar(wchar_t c)
{
	return fputwc(c, stdout);
}

#ifdef __APPLE__
   wint_t putwchar_unlocked(wchar_t c) {
      return putwchar(c);
   }
#else
   weak_alias(putwchar, putwchar_unlocked);
#endif
