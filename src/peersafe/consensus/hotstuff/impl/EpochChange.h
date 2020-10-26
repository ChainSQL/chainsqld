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

#ifndef RIPPLE_CONSENSUS_HOTSTUFF_EPOCHCHANGE_H
#define RIPPLE_CONSENSUS_HOTSTUFF_EPOCHCHANGE_H

#include <vector>

#include <peersafe/consensus/hotstuff/impl/Types.h>

namespace ripple { namespace hotstuff {

struct EpochChange {
	enum Change {
		None,
		Add,
		Remove
	};
	ripple::LedgerInfo ledger_info;
	Epoch epoch;
	Change change;
	std::vector<Author> auhtors;

	EpochChange()
	: ledger_info()
	, epoch(0)
	, change(Change::None)
	, auhtors() {

	}
};

} // namespace hotstuff
} // namespace ripple

#endif // RIPPLE_CONSENSUS_HOTSTUFF_CONFIG_H