#include "chrono"
#include "cerrno"
#include <time.h>

_LIBCPP_BEGIN_NAMESPACE_STD

namespace chrono
{
   system_clock::time_point

   system_clock::from_time_t(time_t t) _NOEXCEPT
   {
      return system_clock::time_point(seconds(t));
   }
} // ns chrono

_LIBCPP_END_NAMESPACE_STD