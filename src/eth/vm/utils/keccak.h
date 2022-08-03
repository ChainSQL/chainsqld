#pragma once

#include <cstdint>
#include <iostream>

namespace eth {

void keccak(uint8_t const *_data, uint64_t _size, uint8_t *o_hash);

void sha3(uint8_t const *_data, uint64_t _size, uint8_t *o_hash);

// The same as assert, but expression is always evaluated and result returned
}
