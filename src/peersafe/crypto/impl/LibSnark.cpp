// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2017-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.
#include <peersafe/crypto/LibSnark.h>
#include <ripple/basics/base_uint.h>
#include <peersafe/basics/TypeTransform.h>
#include <eth/vm/Common.h>

#include <libff/algebra/curves/alt_bn128/alt_bn128_g1.hpp>
#include <libff/algebra/curves/alt_bn128/alt_bn128_g2.hpp>
#include <libff/algebra/curves/alt_bn128/alt_bn128_pairing.hpp>
#include <libff/algebra/curves/alt_bn128/alt_bn128_pp.hpp>
#include <libff/common/profiling.hpp>

using namespace std;

namespace peersafe
{

DEV_SIMPLE_EXCEPTION(InvalidEncoding);

void initLibSnark() noexcept
{
	static bool s_initialized = []() noexcept
	{
		libff::inhibit_profiling_info = true;
		libff::inhibit_profiling_counters = true;
		libff::alt_bn128_pp::init_public_params();
		return true;
	}();
	(void)s_initialized;
}

libff::bigint<libff::alt_bn128_q_limbs> toLibsnarkBigint(ripple::Blob const& _x)
{
	libff::bigint<libff::alt_bn128_q_limbs> b;
	auto const N = b.N;
	constexpr size_t L = sizeof(b.data[0]);
	static_assert(sizeof(mp_limb_t) == L, "Unexpected limb size in libff::bigint.");
	for (size_t i = 0; i < N; i++)
		for (size_t j = 0; j < L; j++)
			b.data[N - 1 - i] |= mp_limb_t(_x[i * L + j]) << (8 * (L - 1 - j));
	return b;
}

ripple::Blob fromLibsnarkBigint(libff::bigint<libff::alt_bn128_q_limbs> const& _b)
{
	static size_t const N = static_cast<size_t>(_b.N);
	static size_t const L = sizeof(_b.data[0]);
	static_assert(sizeof(mp_limb_t) == L, "Unexpected limb size in libff::bigint.");
	ripple::Blob x;
	x.resize(32);
	for (size_t i = 0; i < N; i++)
		for (size_t j = 0; j < L; j++)
			x[i * L + j] = uint8_t(_b.data[N - 1 - i] >> (8 * (L - 1 - j)));
	return x;
}

libff::alt_bn128_Fq decodeFqElement(eth::bytesConstRef _data)
{
	// h256::AlignLeft ensures that the h256 is zero-filled on the right if _data
	// is too short.
	//h256 xbin(_data, h256::AlignLeft);
    ripple::Blob data = ripple::fromEvmC(_data);
	ripple::Blob xbin;
    // xbin.resize(32);
    xbin.insert(xbin.end(), data.begin(), data.begin() + 32);
	// TODO: Consider using a compiler time constant for comparison.
	/*if (u256(xbin) >= u256(fromLibsnarkBigint(libff::alt_bn128_Fq::mod)))
		BOOST_THROW_EXCEPTION(InvalidEncoding());*/
	return toLibsnarkBigint(xbin);
}

libff::alt_bn128_G1 decodePointG1(eth::bytesConstRef _data)
{
	libff::alt_bn128_Fq x = decodeFqElement(_data.cropped(0));
	libff::alt_bn128_Fq y = decodeFqElement(_data.cropped(32));
	if (x == libff::alt_bn128_Fq::zero() && y == libff::alt_bn128_Fq::zero())
		return libff::alt_bn128_G1::zero();
	libff::alt_bn128_G1 p(x, y, libff::alt_bn128_Fq::one());
	if (!p.is_well_formed())
		BOOST_THROW_EXCEPTION(InvalidEncoding());
	return p;
}

eth::bytes encodePointG1(libff::alt_bn128_G1 _p)
{
	if (_p.is_zero())
		return eth::bytes(64, 0);
	_p.to_affine_coordinates();
    ripple::Blob x = fromLibsnarkBigint(_p.X.as_bigint());
    ripple::Blob y = fromLibsnarkBigint(_p.Y.as_bigint());
    ripple::Blob retBlob;

    retBlob.insert(retBlob.end(), x.begin(), x.end());
    retBlob.insert(retBlob.end(), y.begin(), y.end());
	return std::move(retBlob);
}

libff::alt_bn128_Fq2 decodeFq2Element(eth::bytesConstRef _data)
{
	// Encoding: c1 (256 bits) c0 (256 bits)
	// "Big endian", just like the numbers
	return libff::alt_bn128_Fq2(
		decodeFqElement(_data.cropped(32)),
		decodeFqElement(_data.cropped(0))
	);
}

libff::alt_bn128_G2 decodePointG2(eth::bytesConstRef _data)
{
	libff::alt_bn128_Fq2 const x = decodeFq2Element(_data);
	libff::alt_bn128_Fq2 const y = decodeFq2Element(_data.cropped(64));
	if (x == libff::alt_bn128_Fq2::zero() && y == libff::alt_bn128_Fq2::zero())
		return libff::alt_bn128_G2::zero();
	libff::alt_bn128_G2 p(x, y, libff::alt_bn128_Fq2::one());
	if (!p.is_well_formed())
		BOOST_THROW_EXCEPTION(InvalidEncoding());
	return p;
}

pair<bool, eth::bytes> alt_bn128_pairing_product(eth::bytesConstRef _in)
{
	// Input: list of pairs of G1 and G2 points
	// Output: 1 if pairing evaluates to 1, 0 otherwise (left-padded to 32 bytes)

	size_t constexpr pairSize = 2 * 32 + 2 * 64;
	size_t const pairs = _in.size() / pairSize;
	if (pairs * pairSize != _in.size())
		// Invalid length.
		return { false, eth::bytes{} };

	try
	{
		initLibSnark();
		libff::alt_bn128_Fq12 x = libff::alt_bn128_Fq12::one();
		for (size_t i = 0; i < pairs; ++i)
		{
			eth::bytesConstRef const pair = _in.cropped(i * pairSize, pairSize);
			libff::alt_bn128_G1 const g1 = decodePointG1(pair);
			libff::alt_bn128_G2 const p = decodePointG2(pair.cropped(2 * 32));
			if (-libff::alt_bn128_G2::scalar_field::one() * p + p != libff::alt_bn128_G2::zero())
				// p is not an element of the group (has wrong order)
				return { false, eth::bytes() };
			if (p.is_zero() || g1.is_zero())
				continue; // the pairing is one
			x = x * libff::alt_bn128_miller_loop(
				libff::alt_bn128_precompute_G1(g1),
				libff::alt_bn128_precompute_G2(p)
				);
		}
		bool const result = libff::alt_bn128_final_exponentiation(x) == libff::alt_bn128_GT::one();
        ripple::Blob retBlob(32);
        eth::toBigEndian(result, retBlob);
		return { true, retBlob };
	}
	catch (InvalidEncoding const&)
	{
		// Signal the call failure for invalid input.
		return { false, eth::bytes{} };
	}
}

pair<bool, eth::bytes> alt_bn128_G1_add(eth::bytesConstRef _in)
{
	try
	{
		initLibSnark();
		libff::alt_bn128_G1 const p1 = decodePointG1(_in);
		libff::alt_bn128_G1 const p2 = decodePointG1(_in.cropped(32 * 2));
		return { true, encodePointG1(p1 + p2) };
	}
	catch (InvalidEncoding const&)
	{
		// Signal the call failure for invalid input.
		return { false, eth::bytes{} };
	}
}

pair<bool, eth::bytes> alt_bn128_G1_mul(eth::bytesConstRef _in)
{
	try
	{
		initLibSnark();
		libff::alt_bn128_G1 const p = decodePointG1(_in.cropped(0));
		libff::alt_bn128_G1 const result = toLibsnarkBigint(ripple::fromEvmC(_in.cropped(64))) * p;
		return { true, encodePointG1(result) };
	}
	catch (InvalidEncoding const&)
	{
		// Signal the call failure for invalid input.
		return { false, eth::bytes{} };
	}
}

}

