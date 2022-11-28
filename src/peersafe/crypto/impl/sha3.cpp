//
//  sha3.cpp
//  chainsqld
//
//  Created by lascion on 2022/9/15.
//

#include <stdio.h>
#include <peersafe/crypto/hashBase.h>
#include <ripple/basics/contract.h>

namespace ripple {

#ifndef OLD_SHA3
Sha3::Sha3()
{
    eth::sha3_Init256(&sha3Ctx);
    eth::sha3_SetFlags(&sha3Ctx, eth::SHA3_FLAGS_KECCAK);
}

Sha3::~Sha3()
{}

void Sha3::operator()(void const* data, std::size_t size) noexcept
{
    eth::sha3_Update(&sha3Ctx, data, size);
}

Sha3::operator result_type() noexcept
{
    const uint8_t* h = (const uint8_t*)eth::sha3_Finalize(&sha3Ctx);

    Sha3::result_type result;
    std::copy(h, h + 32, result.begin());
    return result;
}
#endif

}
