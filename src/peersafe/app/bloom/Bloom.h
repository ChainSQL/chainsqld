#pragma once

#include <ripple/basics/base_uint.h>
#include <ripple/basics/Slice.h>

namespace ripple {
class Bloom
{
public:
    Bloom(uint2048 const& bloom);

private:
    uint2048 bloom;
};

bool
bloomLookup(uint2048 const& bloom, Slice topic);
}
