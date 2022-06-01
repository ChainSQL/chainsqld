#include "stdio_impl.h"

static FILE *volatile dummy_file = 0;

#ifdef __APPLE__
   static FILE *volatile __stdin_used = 0;
   static FILE *volatile __stdout_used = 0;
   static FILE *volatile __stderr_used = 0;
#else
   weak_alias(dummy_file, __stdin_used);
   weak_alias(dummy_file, __stdout_used);
   weak_alias(dummy_file, __stderr_used);
#endif

static void close_file(FILE *f)
{
	if (!f) return;
	FFINALLOCK(f);
	if (f->wpos > f->wbase) f->write(f, 0, 0);
	if (f->rpos < f->rend) f->seek(f, f->rpos-f->rend, SEEK_CUR);
}

void __stdio_exit(void)
{
	FILE *f;
	for (f=*__ofl_lock(); f; f=f->next) close_file(f);
	close_file(__stdin_used);
	close_file(__stdout_used);
}

#ifdef __APPLE__
   void __stdio_exit_needed(void) {
      return __stdio_exit();
   }
#else
   weak_alias(__stdio_exit, __stdio_exit_needed);
#endif
