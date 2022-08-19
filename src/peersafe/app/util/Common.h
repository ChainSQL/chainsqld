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


#ifndef RIPPLE_RPC_COMMON_UTIL_H_INCLUDED
#define RIPPLE_RPC_COMMON_UTIL_H_INCLUDED

#include <unordered_set>
#include <ripple/basics/base_uint.h>
#include <ripple/protocol/STTx.h>
#include <ripple/basics/Slice.h>
#include <boost/format.hpp>
#include <boost/multiprecision/cpp_int.hpp>

namespace ripple {


using H256Set = std::unordered_set<uint256>;

// Get the current time in seconds since the epoch in UTC(ms)
uint64_t
utcTime();

bool
isHexID(std::string const& txid);


std::shared_ptr<STTx const>
makeSTTx(Slice sit);

template <class T>
std::string
inline toHexString(T value)
{
    return (boost::format("0x%x") % value).str();
}

std::string
inline dropsToWeiHex(uint64_t drops)
{
    boost::multiprecision::uint128_t balance;
    boost::multiprecision::multiply(balance, drops, std::uint64_t(1e12));
    return toHexString(balance);
}

}

#endif
