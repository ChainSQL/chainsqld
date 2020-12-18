#include <peersafe/app/misc/PreContractFace.h>

namespace ripple {

    PrecompiledContract::PrecompiledContract(
        std::string const &_name, int64_t const &_startingBlock /*= 0*/)
        : m_cost(PrecompiledRegistrar::pricer(_name)),
          m_execute(PrecompiledRegistrar::executor(_name)),
          m_startingBlock(_startingBlock)
    {}

    PreContractFace::PreContractFace()
    {
        // for (unsigned i = 1; i <= 4; ++i)
        //     genesisState[AccountID(i)] = Account(0, 1);
        // Setup default precompiled contracts as equal to genesis of Frontier.
        // precompiled.insert(make_pair(Address(21), PrecompiledContract("ecrecover")));
        precompiled.insert(make_pair(AccountID(2), PrecompiledContract("sha256")));
        precompiled.insert(make_pair(AccountID(3), PrecompiledContract("ripemd160")));
        precompiled.insert(make_pair(AccountID(4), PrecompiledContract("identity")));

        // precompiled.insert({AccountID(25), PrecompiledContract("modexp")});
        // precompiled.insert({AccountID(26), PrecompiledContract("alt_bn128_G1_add")});
        // precompiled.insert({AccountID(27), PrecompiledContract("alt_bn128_G1_mul")});
        // precompiled.insert({AccountID(28), PrecompiledContract("alt_bn128_pairing_product")});
        // precompiled.insert({AccountID(29), PrecompiledContract("blake2_compression")});

        precompiled.insert(make_pair(AccountID(41), PrecompiledContract("sm3")));
    }
}
