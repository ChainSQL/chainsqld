#include "locale_impl.h"
#include "pthread_impl.h"
#include "libc.h"

locale_t __uselocale(locale_t new)
{
	pthread_t self = __pthread_self();
	locale_t old = self->locale;
	locale_t global = &libc.global_locale;

	if (new) self->locale = new == LC_GLOBAL_LOCALE ? global : new;

	return old == global ? LC_GLOBAL_LOCALE : old;
}

#ifdef __APPLE__
   locale_t uselocale(locale_t new) {
      return __uselocale(new);
   }
#else
   weak_alias(__uselocale, uselocale);
#endif
