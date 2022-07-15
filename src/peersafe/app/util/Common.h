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
#include <ripple/app/consensus/RCLCxLedger.h>
#include <peersafe/schema/Schema.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <peersafe/schema/PeerManagerImp.h>
#include <ripple/overlay/predicates.h>
namespace ripple {


using H256Set = std::unordered_set<uint256>;

// Get the current time in seconds since the epoch in UTC(ms)
uint64_t
utcTime();

bool
isHexID(std::string const& txid);

/** Notify peers of a consensus state change

    @param ne Event type for notification
    @param ledger The ledger at the time of the state change
    @param haveCorrectLCL Whether we believ we have the correct LCL.
*/
void
notify(Schema& app, protocol::NodeEvent ne, RCLCxLedger const& ledger, bool haveCorrectLCL, beast::Journal journal);
}

#endif
