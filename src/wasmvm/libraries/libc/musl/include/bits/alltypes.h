#pragma once

#include <bits/wchar.h>
#include <bits/stdint.h>

#ifdef EOSIO_NATIVE
#define _Addr long long 
#define __INTPTR_WIDTH__ 64
#else
#define _Addr int
#endif


typedef ptrdiff_t ssize_t;

typedef __builtin_va_list va_list;
typedef __builtin_va_list __isoc_va_list;

typedef unsigned long wctype_t;

typedef struct __mbstate_t { unsigned __opaque1, __opaque2; } mbstate_t;

typedef int regoff_t; //questionable (_Addr in alltypes.h.in)

struct __locale_struct;

typedef struct __locale_struct * locale_t;

typedef struct _IO_FILE FILE;

typedef int64_t off_t;

typedef unsigned long wctype_t;

typedef long time_t;
typedef long suseconds_t;

struct timeval { time_t tv_sec; suseconds_t tv_usec; };
struct timespec { time_t tv_sec; long tv_nsec; };

typedef float float_t;
typedef double double_t;
