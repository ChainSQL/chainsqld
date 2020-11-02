//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2015 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#ifndef RIPPLE_CRYPTO_KEYTYPE_H_INCLUDED
#define RIPPLE_CRYPTO_KEYTYPE_H_INCLUDED

#include <string>
#include <boost/optional.hpp>

namespace ripple {

enum class KeyType
{
    invalid = -1,
    unknown = invalid,

    secp256k1 = 0,
    ed25519   = 1,
    gmalg     = 2,
};

inline
boost::optional<KeyType>
keyTypeFromString (std::string const& s)
{
	if (s == "secp256k1")  return KeyType::secp256k1;
	if (s == "ed25519")  return KeyType::ed25519;
	if (s == "gmalg")      return KeyType::gmalg;

	return KeyType::invalid;
}

inline
char const*
to_string (KeyType type)
{
	return   type == KeyType::secp256k1 ? "secp256k1"
		: type == KeyType::ed25519 ? "ed25519"
		: type == KeyType::gmalg ? "gmalg"
		: "INVALID";
}

template <class Stream>
inline
Stream& operator<<(Stream& s, KeyType type)
{
    return s << to_string(type);
}

}

#endif