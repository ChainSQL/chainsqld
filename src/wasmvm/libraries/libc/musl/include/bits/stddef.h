#pragma once

#ifdef EOSIO_NATIVE
   typedef unsigned long size_t;
   typedef long ptrdiff_t;
#else
   typedef unsigned int size_t;
   typedef int ptrdiff_t; //XXX intptr_t
#endif
