#ifndef HASH_BASE_H_INCLUDE
#define HASH_BASE_H_INCLUDE

// #include <ripple/protocol/CommonKey.h>
#include <ripple/beast/hash/endian.h>
#include <ripple/basics/base_uint.h>
#include <eth/vm/utils/keccak.h>

namespace ripple {
    class hashBase
    {  
    public:
        static beast::endian const endian = beast::endian::big;
    public:
        virtual ~hashBase() {};
        using result_type = ripple::uint256;
        virtual void operator()(void const *data, std::size_t size) noexcept = 0;
        virtual explicit operator result_type() noexcept = 0;
    };

#ifndef OLD_SHA3
    class Sha3 : public hashBase
    {
    public:
        Sha3();
        ~Sha3();
        
        void sha3Final(unsigned char *pHashData, unsigned long *pulHashDataLen);
        void operator()(void const* data, std::size_t size) noexcept override;
        explicit operator result_type() noexcept override;
    private:
        eth::sha3_context sha3Ctx;
    };
#endif
}

#endif
