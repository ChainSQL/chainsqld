#ifndef CHAINSQL_BASICS_TYPETRANSFORM_H_INCLUDED
#define CHAINSQL_BASICS_TYPETRANSFORM_H_INCLUDED

#include <ripple/protocol/AccountID.h>
#include <ripple/basics/base_uint.h>
#include <peersafe/vm/Common.h>
#include <evmc/evmc.h>

namespace ripple {


    inline evmc_address & toEvmC(AccountID const& addr)
    {        
        return const_cast<evmc_address&>(reinterpret_cast<evmc_address const&>(addr));
    }

    inline evmc_uint256be & toEvmC(uint256 const& _h)
    {
        return const_cast<evmc_uint256be&>(reinterpret_cast<evmc_uint256be const&>(_h));
    }

    inline uint256 & fromEvmC(evmc_uint256be const& _n)
    {
        return const_cast<uint256&>(reinterpret_cast<uint256 const&>(_n));
    }

    inline uint256 const * fromEvmC(evmc_uint256be const* _n)
    {
        return reinterpret_cast<uint256 const*>(_n);
    }

    inline AccountID const& fromEvmC(evmc_address const& _addr)
    {        
		return reinterpret_cast<AccountID const&>(_addr);
    }

	inline int64_t fromUint256(uint256 _n)
	{
		return be64toh(((std::uint64_t*) _n.end())[-1]);
	}

	inline int64_t fromUint256(evmc_uint256be const& _n)
	{
		uint256 n = fromEvmC(_n);
		return fromUint256(n);
	}

	inline Blob fromEvmC(bytesConstRef const& data) {
		return std::move(data.toBytes());
	}

	inline static const evmc_address noAddress() 
	{
		static evmc_address no_address = toEvmC(noAccount());
		return no_address;
	}

}

#endif
