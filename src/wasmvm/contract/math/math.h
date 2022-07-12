#pragma once

#include <stdint.h>
#include <stdarg.h>

#include "chainsqlib/contracts/contract.h"

class math : public chainsql::contract {
public:
    using contract::contract;
    int add(int a, int b);
private:
    int sum_;
};