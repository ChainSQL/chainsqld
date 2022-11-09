
#include <peersafe/app/bloom/Bloom.h>
#include <ripple/protocol/digest.h>

namespace ripple {

const int BloomByteLength = 256;

uint16_t
toBigEndian(uint8_t* b)
{
    return uint16_t(b[1]) | uint16_t(b[0]) << 8;
}

Bloom::Bloom()
{
}

Bloom::Bloom(uint2048 const& bloom) : bloom_(bloom)
{
}

void
Bloom::add(Slice const& data)
{
    auto tup = bloomValues(data);
    bloom_.data()[std::get<0>(tup)] = std::get<1>(tup);
    bloom_.data()[std::get<2>(tup)] = std::get<3>(tup);
    bloom_.data()[std::get<4>(tup)] = std::get<5>(tup);
}

bool
Bloom::test(Slice const& data)
{
    uint32_t i1, i2, i3;
    uint8_t v1, v2, v3;
    std::tie(i1, v1, i2, v2, i3, v3) = bloomValues(data);
    return v1 == (v1 & bloom_.data()[i1]) && 
           v2 == (v2 & bloom_.data()[i2]) &&
           v3 == (v3 & bloom_.data()[i3]);
}

std::tuple<uint32_t, uint8_t, uint32_t, uint8_t, uint32_t, uint8_t>
Bloom::bloomValues(Slice const& data)
{
    auto hash = sha512Half<CommonKey::sha3>(data);
    uint8_t hashbuf[6];
    std::memcpy(hashbuf, hash.data(), sizeof(hashbuf));

    // The actual bits to flip
    uint8_t v1 = uint8_t(1 << (hashbuf[1] & 0x7));
    uint8_t v2 = uint8_t(1 << (hashbuf[3] & 0x7));
    uint8_t v3 = uint8_t(1 << (hashbuf[5] & 0x7));
    // The indices for the bytes to OR in
    uint32_t i1 = BloomByteLength - uint32_t((toBigEndian(hashbuf) & 0x7ff) >> 3) - 1;
    uint32_t i2 = BloomByteLength - uint32_t((toBigEndian(hashbuf + 2) & 0x7ff) >> 3) - 1;
    uint32_t i3 = BloomByteLength - uint32_t((toBigEndian(hashbuf + 4) & 0x7ff) >> 3) - 1;
    return std::make_tuple(i1, v1, i2, v2, i3, v3);
}

uint2048&
Bloom::value()
{
    return bloom_;
}

bool
bloomLookup(uint2048 const& bloom, Slice topic)
{
    Bloom b(bloom);
    return b.test(topic);
}

}