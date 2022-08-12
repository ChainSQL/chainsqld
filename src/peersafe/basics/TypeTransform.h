#ifndef CHAINSQL_BASICS_TYPETRANSFORM_H_INCLUDED
#define CHAINSQL_BASICS_TYPETRANSFORM_H_INCLUDED

#include <ripple/protocol/AccountID.h>
#include <ripple/basics/base_uint.h>
#include <eth/vm/Common.h>
#include <ripple/basics/Blob.h>
#include <eth/evmc/include/evmc/evmc.h>

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

    inline std::uint64_t
    be64toh(uint64_t value)
    {
        
        #ifdef _MSC_VER
        return _byteswap_uint64(value);
        #else
        return __builtin_bswap64(value);
        #endif
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

	inline Blob fromEvmC(eth::bytesConstRef const& data) {
		return std::move(data.toBytes());
	}

	inline static const evmc_address noAddress() 
	{
		static evmc_address no_address = toEvmC(noAccount());
		return no_address;
	}

    inline Blob bigToBytes(eth::bigint _i, int size)
    {  
        Blob ret;
        ret.resize(size);
        unsigned char* b = &(ret.back());
        for (; _i; _i >>= 8)
            *(b--) = (unsigned char)(_i & 0xff);
        return ret;
    }

    inline uint256 fromU256(eth::u256 const& _n)
    {
        uint256 ret;
        Blob bytes = bigToBytes(eth::bigint(_n), ret.size());
        memcpy(ret.data(), bytes.data(), bytes.size());
        return ret;
    }

    inline uint160 fromH160(eth::h160 const& _n)
    {
        uint160 ret;
        memcpy(ret.data(), _n.data(), _n.size);
        return ret;
    }

}

#endif
