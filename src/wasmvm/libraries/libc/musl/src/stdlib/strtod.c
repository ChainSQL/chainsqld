#include <stdlib.h>
#include "shgetc.h"
#include "floatscan.h"
#include "stdio_impl.h"
#include "libc.h"

static long double strtox(const char *s, char **p, int prec)
{
	FILE f = {
		.buf = (void *)s, .rpos = (void *)s,
		.rend = (void *)-1, .lock = -1
	};
	shlim(&f, 0);
	long double y = __floatscan(&f, prec, 1);
	off_t cnt = shcnt(&f);
	if (p) *p = cnt ? (char *)s + cnt : (char *)s;
	return y;
}

float strtof(const char *restrict s, char **restrict p)
{
	return strtox(s, p, 0);
}

double strtod(const char *restrict s, char **restrict p)
{
	return strtox(s, p, 1);
}

long double strtold(const char *restrict s, char **restrict p)
{
	return strtox(s, p, 2);
}

#ifdef __APPLE__
   float strtof_l(const char *restrict s, char **restrict p) {
      return strtof(s,p);
   }
   double strtod_l(const char *restrict s, char **restrict p) {
      return strtod(s,p);

   }
   long double strtold_l(const char *restrict s, char **restrict p) {
      return strtold(s,p);
   }
   float __strtof_l(const char *restrict s, char **restrict p) {
      return strtof(s,p);
   }
   double __strtod_l(const char *restrict s, char **restrict p) {
      return strtod(s,p);
   }
   long double __strtold_l(const char *restrict s, char **restrict p) {
      return strtold(s,p);
   }
#else
   weak_alias(strtof, strtof_l);
   weak_alias(strtod, strtod_l);
   weak_alias(strtold, strtold_l);
   weak_alias(strtof, __strtof_l);
   weak_alias(strtod, __strtod_l);
   weak_alias(strtold, __strtold_l);
#endif
