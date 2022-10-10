#include "math.h"

int math::add(int a, int b)
{
    int ret = a + b;
    sum_ += ret;
    return sum_;
}

CHAINSQL_DISPATCH(math, (add))