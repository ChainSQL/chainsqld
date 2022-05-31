#include "time_impl.h"
#include <errno.h>
#include "libc.h"

extern const char __utc[];

struct tm *__gmtime_r(const time_t *restrict t, struct tm *restrict tm)
{
	if (__secs_to_tm(*t, tm) < 0) {
		errno = EOVERFLOW;
		return 0;
	}
	tm->tm_isdst = 0;
	tm->__tm_gmtoff = 0;
	tm->__tm_zone = __utc;
	return tm;
}

#ifdef __APPLE__
struct tm *gmtime_r(const time_t *restrict t, struct tm *restrict tm) { return __gmtime_r(t,tm); }
#else
weak_alias(__gmtime_r, gmtime_r);
#endif
