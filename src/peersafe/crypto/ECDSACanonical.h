//------------------------------------------------------------------------------
/*
 This file is part of chainsqld: https://github.com/chainsql/chainsqld
 Copyright (c) 2016-2018 Peersafe Technology Co., Ltd.
 
	chainsqld is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.
 
	chainsqld is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
 */
//==============================================================================

//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012, 2013 Ripple Labs Inc.

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

#ifndef RIPPLE_CRYPTO_ECDSACANONICAL_H_INCLUDED
#define RIPPLE_CRYPTO_ECDSACANONICAL_H_INCLUDED

#include <ripple/basics/Blob.h>

namespace ripple {

enum class ECDSA
{
    not_strict = 0,
    strict
};

/** Checks whether a secp256k1 ECDSA signature is canonical.
    Return value is true if the signature is canonical.
    If mustBeStrict is specified, the signature must be
    strictly canonical (one and only one valid form).
    The return value for something that is not an ECDSA
    signature is unspecified. (But the function will not crash.)
*/
bool isCanonicalECDSASig (void const* signature,
    std::size_t sigLen, ECDSA mustBeStrict);

inline bool isCanonicalECDSASig (Blob const& signature,
    ECDSA mustBeStrict)
{
    return signature.empty() ? false :
        isCanonicalECDSASig (&signature[0], signature.size(), mustBeStrict);
}

/** Converts a canonical secp256k1 ECDSA signature to a
    fully-canonical one. Returns true if the original signature
    was already fully-canonical. The behavior if something
    that is not a canonical secp256k1 ECDSA signature is
    passed is unspecified. The signature buffer must be large
    enough to accommodate the largest valid fully-canonical
    secp256k1 ECDSA signature (72 bytes).
*/
bool makeCanonicalECDSASig (void *signature, std::size_t& sigLen);

}

#endif
