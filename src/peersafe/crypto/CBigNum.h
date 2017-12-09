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

#ifndef RIPPLE_CRYPTO_CBIGNUM_H_INCLUDED
#define RIPPLE_CRYPTO_CBIGNUM_H_INCLUDED

#include <ripple/basics/base_uint.h>
#include <openssl/bn.h>

namespace ripple {

// VFALCO TODO figure out a way to remove the dependency on openssl in the
//         header. Maybe rewrite this to use cryptopp.

class CBigNum : public BIGNUM
{
public:
    CBigNum ();
    CBigNum (const CBigNum& b);
    CBigNum& operator= (const CBigNum& b);
    CBigNum (char n);
    CBigNum (short n);
    CBigNum (int n);
    CBigNum (long n);
    CBigNum (long long n);
    CBigNum (unsigned char n);
    CBigNum (unsigned short n);
    CBigNum (unsigned int n);
    CBigNum (unsigned long long n);
    explicit CBigNum (uint256 n);
    explicit CBigNum (Blob const& vch);
    CBigNum (unsigned char const* begin, unsigned char const* end);
    ~CBigNum ();

    void setuint (unsigned int n);
    unsigned int getuint () const;
    int getint () const;
    void setint64 (std::int64_t n);
    std::uint64_t getuint64 () const;
    void setuint64 (std::uint64_t n);
    void setuint256 (uint256 const& n);
    uint256 getuint256 ();
    void setvch (unsigned char const* begin, unsigned char const* end);
    void setvch (Blob const& vch);
    Blob getvch () const;
    CBigNum& SetCompact (unsigned int nCompact);
    unsigned int GetCompact () const;
    void SetHex (std::string const& str);
    std::string ToString (int nBase = 10) const;
    std::string GetHex () const;
    bool operator! () const;
    CBigNum& operator+= (const CBigNum& b);
    CBigNum& operator-= (const CBigNum& b);
    CBigNum& operator*= (const CBigNum& b);
    CBigNum& operator/= (const CBigNum& b);
    CBigNum& operator%= (const CBigNum& b);
    CBigNum& operator<<= (unsigned int shift);
    CBigNum& operator>>= (unsigned int shift);
    CBigNum& operator++ ();
    CBigNum& operator-- ();
    const CBigNum operator++ (int);
    const CBigNum operator-- (int);

    friend inline const CBigNum operator- (const CBigNum& a, const CBigNum& b);
    friend inline const CBigNum operator/ (const CBigNum& a, const CBigNum& b);
    friend inline const CBigNum operator% (const CBigNum& a, const CBigNum& b);

private:
    // private because the size of an unsigned long varies by platform

    void setulong (unsigned long n);
    unsigned long getulong () const;
};

const CBigNum operator+ (const CBigNum& a, const CBigNum& b);
const CBigNum operator- (const CBigNum& a, const CBigNum& b);
const CBigNum operator- (const CBigNum& a);
const CBigNum operator* (const CBigNum& a, const CBigNum& b);
const CBigNum operator/ (const CBigNum& a, const CBigNum& b);
const CBigNum operator% (const CBigNum& a, const CBigNum& b);
const CBigNum operator<< (const CBigNum& a, unsigned int shift);
const CBigNum operator>> (const CBigNum& a, unsigned int shift);

bool operator== (const CBigNum& a, const CBigNum& b);
bool operator!= (const CBigNum& a, const CBigNum& b);
bool operator<= (const CBigNum& a, const CBigNum& b);
bool operator>= (const CBigNum& a, const CBigNum& b);
bool operator< (const CBigNum& a, const CBigNum& b);
bool operator> (const CBigNum& a, const CBigNum& b);

//------------------------------------------------------------------------------

// VFALCO I believe only STAmount uses these
int BN_add_word64 (BIGNUM* a, std::uint64_t w);
int BN_sub_word64 (BIGNUM* a, std::uint64_t w);
int BN_mul_word64 (BIGNUM* a, std::uint64_t w);
std::uint64_t BN_div_word64 (BIGNUM* a, std::uint64_t w);

}

#endif
