#include "stdio_impl.h"

int (getc_unlocked)(FILE *f)
{
	return getc_unlocked(f);
}

#ifdef __APPLE__
   int fgetc_unlocked(FILE *f){
      return getc_unlocked(f);
   }
   int _IO_getc_unlocked(FILE *f){
      return getc_unlocked(f);
   }
#else
   weak_alias (getc_unlocked, fgetc_unlocked);
   weak_alias (getc_unlocked, _IO_getc_unlocked);
#endif
