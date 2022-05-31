#pragma once

//#include <stdlib.h>
#include <stdint.h>

namespace chainsql {
    namespace internal_use_do_not_use
    {
        extern "C"
        {
            void chainsql_assert( int32_t test, const void* msg );
            void chainsql_assert_message(int32_t test, const void*msg, int32_t msg_len);
        }
    }

    inline void check(bool test, const void* msg, uint32_t msg_len) {
        if (test == false) {
            internal_use_do_not_use::chainsql_assert_message(test, msg, msg_len);
        }
    }

    inline void check(bool test, const void* msg) {
        if (test == false) {
            internal_use_do_not_use::chainsql_assert(test, msg);
        }
    }
}