#include "chainsqlib/core/check.h"
#include "chainsqlib/contracts/contract.h"

extern "C" {
    void chainsql_memcpy(void*,int64_t,int32_t);
}

class hello : public chainsql::contract {
public:
    using contract::contract;

    int32_t hi(int64_t data_ptr, int32_t size)
    {
        char buffer[256] = {0};
        chainsql_memcpy((void*)(buffer + buffer_bytes), data_ptr, size);
        buffer_bytes += size;
        echo(buffer);
        return buffer_bytes;
    }

private:
    void echo(const char *s)
    {
        chainsql::check(0, s);
    }

    int32_t buffer_bytes = 0;
    //char buffer[256] = {0};
};

CHAINSQL_DISPATCH(hello, (hi))