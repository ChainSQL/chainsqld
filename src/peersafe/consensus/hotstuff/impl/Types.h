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

#ifndef RIPPLE_CONSENSUS_HOTSTUFF_TYPES_H
#define RIPPLE_CONSENSUS_HOTSTUFF_TYPES_H

#include <vector>
#include <string>

#include <ripple/basics/Buffer.h>
#include <ripple/basics/Slice.h>
#include <ripple/basics/Blob.h>
#include <ripple/protocol/digest.h>

namespace ripple { namespace hotstuff {

using BlockHash = sha512_half_hasher::result_type; 
using Command = std::vector<std::string>;

using ReplicaID = int;
using PublicKey = ripple::Slice;
using PrivateKey = ripple::Slice;
using Signature = ripple::Buffer;

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_TYPES_H