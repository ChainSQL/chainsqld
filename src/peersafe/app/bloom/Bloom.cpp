
#include <peersafe/app/bloom/Bloom.h>

namespace ripple {
Bloom::Bloom(uint2048 const& bloom) : bloom(bloom)
{
}

bool
bloomLookup(uint2048 const& bloom, Slice topic)
{
    Bloom b(bloom);
    return false;
}

}