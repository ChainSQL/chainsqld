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

#ifndef RIPPLE_CONSENSUS_HOTSTUFF_CONFIG_H
#define RIPPLE_CONSENSUS_HOTSTUFF_CONFIG_H

#include <peersafe/consensus/hotstuff/impl/Types.h>

namespace ripple { namespace hotstuff {

struct Config {
	// self id
	Author id;
	// epoch in current runtime
	Epoch epoch;
	// generate a dummy block after timeout (seconds)
	int timeout;
	// whether we should generate a dummy block or shouldn't
	bool disable_nil_block;

	Config()
	: id()
	, epoch(0)
	, timeout(60)
	, disable_nil_block(false) {

	}
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_CONFIG_H