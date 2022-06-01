#include <ctype.h>
#include "libc.h"

int toupper(int c)
{
	if (islower(c)) return c & 0x5f;
	return c;
}

int __toupper_l(int c, locale_t l)
{
	return toupper(c);
}

#ifdef __APPLE__
   int toupper_l(int c, locale_t l) {
      return __toupper_l(c,l);
   }
#else
   weak_alias(__toupper_l, toupper_l);
#endif
