#include "libc.h"

struct __libc __libc;

size_t __hwcap;
size_t __sysinfo;
char *__progname=0, *__progname_full=0;

#ifdef __APPLE__
   char* program_invocation_short_name=0;
   char* program_invocation_name=0;
#else
   weak_alias(__progname, program_invocation_short_name);
   weak_alias(__progname_full, program_invocation_name);
#endif
