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

// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include <BeastConfig.h>
#include <ripple/crypto/impl/ec_key.h>
#include <peersafe/crypto/ECDSA.h>
#include <peersafe/crypto/ECDSACanonical.h>
#include <peersafe/crypto/ECDSAKey.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/hmac.h>
#include <cstdint>

namespace ripple  {

using openssl::ec_key;

static Blob ECDSASign (uint256 const& hash, const openssl::ec_key& key)
{
    Blob result;

    unsigned char sig[128];
    unsigned int  siglen = sizeof sig - 1;

    const unsigned char* p = hash.begin();

    if (ECDSA_sign (0, p, hash.size(), sig, &siglen, (EC_KEY*) key.get()))
    {
        size_t newlen = siglen;

        makeCanonicalECDSASig (sig, newlen);

        result.resize (newlen);
        memcpy (&result[0], sig, newlen);
    }

    return result;
}

Blob ECDSASign (uint256 const& hash, uint256 const& key)
{
    return ECDSASign (hash, ECDSAPrivateKey (key));
}

static bool ECDSAVerify (uint256 const& hash, std::uint8_t const* sig, size_t sigLen, EC_KEY* key)
{
    // -1 = error, 0 = bad sig, 1 = good
    return ECDSA_verify (0, hash.begin(), hash.size(), sig, sigLen, key) > 0;
}

static bool ECDSAVerify (uint256 const& hash, Blob const& sig, const openssl::ec_key& key)
{
    return key.valid() && ECDSAVerify (hash, sig.data(), sig.size(), (EC_KEY*) key.get());
}

bool ECDSAVerify (uint256 const& hash,
                  Blob const& sig,
                  std::uint8_t const* key_data,
                  std::size_t key_size)
{
    return ECDSAVerify (hash, sig, ECDSAPublicKey (key_data, key_size));
}

} // ripple
