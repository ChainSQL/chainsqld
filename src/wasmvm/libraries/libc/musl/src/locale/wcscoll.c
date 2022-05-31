#include <wchar.h>
#include <locale.h>
#include "locale_impl.h"
#include "libc.h"

/* FIXME: stub */
int __wcscoll_l(const wchar_t *l, const wchar_t *r, locale_t locale)
{
	return wcscmp(l, r);
}

int wcscoll(const wchar_t *l, const wchar_t *r)
{
	return __wcscoll_l(l, r, CURRENT_LOCALE);
}

#ifdef __APPLE__
   int wcscoll_l(const wchar_t *l, const wchar_t *r, locale_t locale) {
      return __wcscoll_l(l,r,locale);
   }
#else
   weak_alias(__wcscoll_l, wcscoll_l);
#endif
