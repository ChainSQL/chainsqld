#include "time_impl.h"
#include <errno.h>

time_t mktime(struct tm *tm)
{
   return __tm_to_secs(tm);
}