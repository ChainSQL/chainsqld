#include <math.h>
#include "libc.h"

float remainderf(float x, float y)
{
	int q;
	return remquof(x, y, &q);
}

#ifdef __APPLE__
   float dremf(float x, float y) {
      return remainderf(x,y);
   }
#else
   weak_alias(remainderf, dremf);
#endif
