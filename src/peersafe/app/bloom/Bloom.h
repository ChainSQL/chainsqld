#pragma once

#include <ripple/basics/base_uint.h>
#include <ripple/basics/Slice.h>

namespace ripple {
class Bloom
{
public:
    Bloom();
    Bloom(uint2048 const& bloom);

    void
    add(Slice const& data);

    bool
    test(Slice const& data);

    uint2048&
    value();

private:
    // bloomValues returns the bytes (index-value pairs) to set for the given data
    std::tuple<uint32_t, uint8_t, uint32_t, uint8_t, uint32_t, uint8_t>
    bloomValues(Slice const& data);

private:
    uint2048 bloom_;
};

bool
bloomLookup(uint2048 const& bloom, Slice topic);
}
