#include "libc.h"

char **__environ = 0;

#ifdef __APPLE__
char **_environ = 0;
char **environ = 0;
#else
   weak_alias(__environ, ___environ);
   weak_alias(__environ, _environ);
   weak_alias(__environ, environ);
#endif
