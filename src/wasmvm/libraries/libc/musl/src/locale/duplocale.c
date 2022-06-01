#include <stdlib.h>
#include <string.h>
#include "locale_impl.h"
#include "libc.h"

locale_t __duplocale(locale_t old)
{
	locale_t new = malloc(sizeof *new);
	if (!new) return 0;
	if (old == LC_GLOBAL_LOCALE) old = &libc.global_locale;
	*new = *old;
	return new;
}

#ifdef __APPLE__
   locale_t duplocale(locale_t old) {
      return __duplocale(old);
   }
#else
   weak_alias(__duplocale, duplocale);
#endif
