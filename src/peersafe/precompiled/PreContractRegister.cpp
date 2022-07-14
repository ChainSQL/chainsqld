#include <boost/multiprecision/cpp_int.hpp>
#include <peersafe/crypto/LibSnark.h>
#include <peersafe/precompiled/PreContractRegister.h>
#include <peersafe/basics/TypeTransform.h>
#include <ripple/protocol/digest.h>
#include <ripple/protocol/CommonKey.h>
#include <ripple/basics/Blob.h>
#include <ripple/basics/Slice.h>
#include <ripple/protocol/PublicKey.h>

using namespace std;

namespace ripple {

PrecompiledRegistrar* PrecompiledRegistrar::s_this = nullptr;

PrecompiledExecutor const& PrecompiledRegistrar::executor(std::string const& _name)
{
    if (!get()->m_execs.count(_name))
        BOOST_THROW_EXCEPTION(ExecutorNotFound());
    return get()->m_execs[_name];
}

PrecompiledPricer const& PrecompiledRegistrar::pricer(std::string const& _name)
{
    if (!get()->m_pricers.count(_name))
        BOOST_THROW_EXCEPTION(PricerNotFound());
    return get()->m_pricers[_name];
}

namespace
{

// // Big-endian to/from host endian conversion functions.

// /// Converts a templated integer value to the big-endian byte-stream represented on a templated collection.
// /// The size of the collection object will be unchanged. If it is too small, it will not represent the
// /// value properly, if too big then the additional elements will be zeroed out.
// /// @a Out will typically be either std::string or bytes.
// /// @a T will typically by unsigned, u160, u256 or bigint.
// template <class T, class Out>
// inline void toBigEndian(T _val, Out& o_out)
// {
// 	static_assert(std::is_same<eth::bigint, T>::value || !std::numeric_limits<T>::is_signed, "only unsigned types or bigint supported"); //bigint does not carry sign bit on shift
// 	for (auto i = o_out.size(); i != 0; _val >>= 8, i--)
// 	{
// 		T v = _val & (T)0xff;
// 		o_out[i - 1] = (typename Out::value_type)(uint8_t)v;
// 	}
// }

// /// Converts a big-endian byte-stream represented on a templated collection to a templated integer value.
// /// @a _In will typically be either std::string or bytes.
// /// @a T will typically by unsigned, u160, u256 or bigint.
// template <class T, class _In>
// inline T fromBigEndian(_In const& _bytes)
// {
// 	T ret = (T)0;
// 	for (auto i: _bytes)
// 		ret = (T)((ret << 8) | (eth::byte)(typename std::make_unsigned<decltype(i)>::type)i);
// 	return ret;
// }

int64_t linearPricer(unsigned _base, unsigned _word, eth::bytesConstRef _in)
{
    int64_t const s = _in.size();
    int64_t const b = _base;
    int64_t const w = _word;
    return b + (s + 31) / 32 * w;
}

 ETH_REGISTER_PRECOMPILED_PRICER(ecrecover)
 (eth::bytesConstRef /*_in*/, int64_t const& /*_blockNumber*/)
 {
     return 3000;
 }

 ETH_REGISTER_PRECOMPILED(ecrecover)(eth::bytesConstRef _in)
 {
     struct
     {
         uint256 hash;
         uint256 v;
         uint256 r;
         uint256 s;
     } in;

     memcpy(&in, _in.data(), min(_in.size(), sizeof(in)));

     uint256 ret;
     uint v = (uint)(eth::u256)in.v;
     if (v >= 27 && v <= 28)
     {
         ripple::SignatureStruct sig(in.r, in.s, (byte)(v - 27));
         if (sig.isValid())
         {
             try
             {
                 Blob rec = ripple::recover(sig, in.hash);
                 if (rec.size() != 0)
                 {
                     eth::sha3(rec.data(), rec.size(), ret.data());
                     memset(ret.data(), 0, 12);
                     return {true, eth::bytes(ret.data(), ret.data() + ret.size())};
                 }
             }
             catch (...) {}
         }
     }
     return {true, {}};
 }

ETH_REGISTER_PRECOMPILED_PRICER(sha256)
(eth::bytesConstRef _in, int64_t const& /*_blockNumber*/)
{
    return linearPricer(60, 12, _in);
}

ETH_REGISTER_PRECOMPILED(sha256)(eth::bytesConstRef _in)
{
    auto hashRet = sha512Half<CommonKey::sha>(Slice(_in.data(), _in.size()));
    return {true, Blob(hashRet.begin(), hashRet.end())};
}

ETH_REGISTER_PRECOMPILED_PRICER(sm3)
(eth::bytesConstRef _in, int64_t const& /*_blockNumber*/)
{
    return linearPricer(60, 12, _in);
}

ETH_REGISTER_PRECOMPILED(sm3)(eth::bytesConstRef _in)
{
    auto hashRet = sha512Half<CommonKey::sm3>(Slice(_in.data(), _in.size()));
    return {true, Blob(hashRet.begin(), hashRet.end())};
}

ETH_REGISTER_PRECOMPILED_PRICER(ripemd160)
(eth::bytesConstRef _in, int64_t const& /*_blockNumber*/)
{
    return linearPricer(600, 120, _in);
}

ETH_REGISTER_PRECOMPILED(ripemd160)(eth::bytesConstRef _in)
{
    // return {true, h256(dev::ripemd160(_in), h256::AlignRight).asBytes()};
    ripemd160_hasher rh;
    rh(_in.data(), _in.size());
    auto rhRet = ripemd160_hasher::result_type(rh);
    return {true, Blob(rhRet.begin(), rhRet.end())};
}

ETH_REGISTER_PRECOMPILED_PRICER(identity)
(eth::bytesConstRef _in, int64_t const& /*_blockNumber*/)
{
    return linearPricer(15, 3, _in);
}

ETH_REGISTER_PRECOMPILED(identity)(eth::bytesConstRef _in)
{
    return {true, _in.toBytes()};
}

ETH_REGISTER_PRECOMPILED_PRICER(enbase58)
(eth::bytesConstRef _in, int64_t const& /*_blockNumber*/)
{
    return linearPricer(60, 12, _in);
}

ETH_REGISTER_PRECOMPILED(enbase58)(eth::bytesConstRef _in/*, std::uint8_t tokenType = (std::uint8_t)TokenType::AccountID*/)
{
    auto enRet = base58EncodeToken(TokenType::AccountID, _in.data(), _in.size());
    return { true, Blob(enRet.begin(), enRet.end()) };
}

ETH_REGISTER_PRECOMPILED_PRICER(debase58)
(eth::bytesConstRef _in, int64_t const& /*_blockNumber*/)
{
    return linearPricer(60, 12, _in);
}

ETH_REGISTER_PRECOMPILED(debase58)(eth::bytesConstRef _in/*, std::uint8_t tokenType = (std::uint8_t)TokenType::AccountID*/)
{
    auto enRet = decodeBase58Token(std::string((const char*)_in.data(), _in.size()), TokenType::AccountID);
    return { true, Blob(enRet.begin(), enRet.end()) };
}

// Parse _count bytes of _in starting with _begin offset as big endian int.
// If there's not enough bytes in _in, consider it infinitely right-padded with zeroes.
eth::bigint parseBigEndianRightPadded(eth::bytesConstRef _in, eth::bigint const& _begin, eth::bigint const& _count)
{
    if (_begin > _in.count())
        return 0;
    assert(_count <= numeric_limits<size_t>::max() / 8); // Otherwise, the return value would not fit in the memory.

    size_t const begin{_begin};
    size_t const count{_count};

    // crop _in, not going beyond its size
    eth::bytesConstRef cropped = _in.cropped(begin, min(count, _in.count() - begin));

    eth::bigint ret = ripple::fromBigEndian<eth::bigint>(cropped);
    // shift as if we had right-padding zeroes
    assert(count - cropped.count() <= numeric_limits<size_t>::max() / 8);
    ret <<= 8 * (count - cropped.count());

    return ret;
}

 ETH_REGISTER_PRECOMPILED(modexp)(eth::bytesConstRef _in)
 {
     eth::bigint const baseLength(parseBigEndianRightPadded(_in, 0, 32));
     eth::bigint const expLength(parseBigEndianRightPadded(_in, 32, 32));
     eth::bigint const modLength(parseBigEndianRightPadded(_in, 64, 32));
     assert(modLength <= numeric_limits<size_t>::max() / 8); // Otherwise gas should be too expensive.
     assert(baseLength <= numeric_limits<size_t>::max() / 8); // Otherwise, gas should be too expensive.
     if (modLength == 0 && baseLength == 0)
         return {true, eth::bytes{}}; // This is a special case where expLength can be very big.
     assert(expLength <= numeric_limits<size_t>::max() / 8);

     eth::bigint const base(parseBigEndianRightPadded(_in, 96, baseLength));
     eth::bigint const exp(parseBigEndianRightPadded(_in, 96 + baseLength, expLength));
     eth::bigint const mod(parseBigEndianRightPadded(_in, 96 + baseLength + expLength, modLength));

     eth::bigint const result = mod != 0 ? boost::multiprecision::powm(base, exp, mod) : eth::bigint{0};

     size_t const retLength(modLength);
     eth::bytes ret(retLength);
     ripple::toBigEndian(result, ret);

     return {true, ret};
 }

 namespace
 {
     int64_t expLengthAdjust(int64_t const& _expOffset, int64_t const& _expLength, eth::bytesConstRef _in)
     {
         if (_expLength <= 32)
         {
             eth::bigint const exp(parseBigEndianRightPadded(_in, _expOffset, _expLength));
             return exp ? boost::multiprecision::msb(exp) : 0;
         }
         else
         {
             eth::bigint const expFirstWord(parseBigEndianRightPadded(_in, _expOffset, 32));
             size_t const highestBit(expFirstWord ? boost::multiprecision::msb(expFirstWord) : 0);
             return 8 * (_expLength - 32) + highestBit;
         }
     }

     int64_t multComplexity(int64_t const& _x)
     {
         if (_x <= 64)
             return _x * _x;
         if (_x <= 1024)
             return (_x * _x) / 4 + 96 * _x - 3072;
         else
             return (_x * _x) / 16 + 480 * _x - 199680;
     }
 }

 ETH_REGISTER_PRECOMPILED_PRICER(modexp)(eth::bytesConstRef _in, int64_t const&)
 {
     int64_t const baseLength(parseBigEndianRightPadded(_in, 0, 32));
     int64_t const expLength(parseBigEndianRightPadded(_in, 32, 32));
     int64_t const modLength(parseBigEndianRightPadded(_in, 64, 32));

     int64_t const maxLength(max(modLength, baseLength));
     int64_t const adjustedExpLength(expLengthAdjust(baseLength + 96, expLength, _in));

     return multComplexity(maxLength) * max<int64_t>(adjustedExpLength, 1) / 20;
 }

 ETH_REGISTER_PRECOMPILED(alt_bn128_G1_add)(eth::bytesConstRef _in)
 {
     return peersafe::alt_bn128_G1_add(_in);
 }

 ETH_REGISTER_PRECOMPILED_PRICER(alt_bn128_G1_add)
 (eth::bytesConstRef /*_in*/, int64_t const& _blockNumber)
 {
     return 150;
     // return _blockNumber < _chainParams.istanbulForkBlock ? 500 : 150;
 }

 ETH_REGISTER_PRECOMPILED(alt_bn128_G1_mul)(eth::bytesConstRef _in)
 {
     return  peersafe::alt_bn128_G1_mul(_in);
 }

 ETH_REGISTER_PRECOMPILED_PRICER(alt_bn128_G1_mul)
 (eth::bytesConstRef /*_in*/, int64_t const& _blockNumber)
 {
     return 6000;
     // return _blockNumber < _chainParams.istanbulForkBlock ? 40000 : 6000;
 }

 ETH_REGISTER_PRECOMPILED(alt_bn128_pairing_product)(eth::bytesConstRef _in)
 {
     return  peersafe::alt_bn128_pairing_product(_in);
 }

 ETH_REGISTER_PRECOMPILED_PRICER(alt_bn128_pairing_product)
 (eth::bytesConstRef _in, int64_t const& _blockNumber)
 {
     auto const k = _in.size() / 192;
     return 45000 + k * 34000;
     // return _blockNumber < _chainParams.istanbulForkBlock ? 100000 + k * 80000 : 45000 + k * 34000;
 }

// ETH_REGISTER_PRECOMPILED(blake2_compression)(eth::bytesConstRef _in)
// {
//     static constexpr size_t roundsSize = 4;
//     static constexpr size_t stateVectorSize = 8 * 8;
//     static constexpr size_t messageBlockSize = 16 * 8;
//     static constexpr size_t offsetCounterSize = 8;
//     static constexpr size_t finalBlockIndicatorSize = 1;
//     static constexpr size_t totalInputSize = roundsSize + stateVectorSize + messageBlockSize +
//                                              2 * offsetCounterSize + finalBlockIndicatorSize;

//     if (_in.size() != totalInputSize)
//         return {false, {}};

//     auto const rounds = fromBigEndian<uint32_t>(_in.cropped(0, roundsSize));
//     auto const stateVector = _in.cropped(roundsSize, stateVectorSize);
//     auto const messageBlockVector = _in.cropped(roundsSize + stateVectorSize, messageBlockSize);
//     auto const offsetCounter0 =
//         _in.cropped(roundsSize + stateVectorSize + messageBlockSize, offsetCounterSize);
//     auto const offsetCounter1 = _in.cropped(
//         roundsSize + stateVectorSize + messageBlockSize + offsetCounterSize, offsetCounterSize);
//     uint8_t const finalBlockIndicator =
//         _in[roundsSize + stateVectorSize + messageBlockSize + 2 * offsetCounterSize];

//     if (finalBlockIndicator != 0 && finalBlockIndicator != 1)
//         return {false, {}};

//     return {true, dev::crypto::blake2FCompression(rounds, stateVector, offsetCounter0,
//                       offsetCounter1, finalBlockIndicator, messageBlockVector)};
// }

// ETH_REGISTER_PRECOMPILED_PRICER(blake2_compression)
// (eth::bytesConstRef _in, int64_t const&)
// {
//     auto const rounds = fromBigEndian<uint32_t>(_in.cropped(0, 4));
//     return rounds;
// }
}

}
