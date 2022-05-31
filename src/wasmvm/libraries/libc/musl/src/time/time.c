#include <time.h>
//#include "syscall.h"

__attribute__((eosio_wasm_import))
extern uint64_t current_time();

time_t time(time_t *t)
{
	time_t elapsed = (time_t)(current_time() / 1000000ll);
	if (t) *t = elapsed;
	return elapsed;
}
