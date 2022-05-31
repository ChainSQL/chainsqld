#include <math.h>
#include "libc.h"

double remainder(double x, double y)
{
	int q;
	return remquo(x, y, &q);
}

#ifdef __APPLE__
   double drem(double x, double y) {
      return remainder(x,y);
   }
#else
   weak_alias(remainder, drem);
#endif
