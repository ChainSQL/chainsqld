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
    Copyright (c) 2014 Ripple Labs Inc.

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

#ifndef RIPPLE_CRYPTO_ECIES_H_INCLUDED
#define RIPPLE_CRYPTO_ECIES_H_INCLUDED

#include <ripple/basics/base_uint.h>
#include <ripple/basics/Blob.h>
#include <ripple/protocol/PublicKey.h>
#include <ripple/protocol/SecretKey.h>

namespace ripple {

// ECIES functions. These throw on failure

// encrypt/decrypt functions with integrity checking.
// Note that the other side must somehow know what keys to use
Blob encryptECIES (uint256 const& secretKey, Blob const& publicKey, Blob const& plaintext);
Blob decryptECIES (uint256 const& secretKey, Blob const& publicKey, Blob const& ciphertext);

Blob encrypt(Blob const& passBlob, PublicKey const& publicKey);
Blob decrypt(Blob const& cipherBlob, SecretKey const& secret_key);

} // ripple

#endif
