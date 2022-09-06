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


#include <cctype>
#include <chrono>
#include <peersafe/app/util/NetworkUtil.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <peersafe/schema/PeerManagerImp.h>
namespace ripple {

void
notify(
    Schema& app,
    protocol::NodeEvent ne,
    RCLCxLedger const& ledger,
    bool haveCorrectLCL,
    beast::Journal journal)
{
    protocol::TMStatusChange s;

    if (!haveCorrectLCL)
        s.set_newevent(protocol::neLOST_SYNC);
    else
        s.set_newevent(ne);

    s.set_ledgerseq(ledger.seq());
    s.set_networktime(app.timeKeeper().now().time_since_epoch().count());
    s.set_ledgerhashprevious(
        ledger.parentID().begin(),
        std::decay_t<decltype(ledger.parentID())>::bytes);
    s.set_ledgerhash(
        ledger.id().begin(), std::decay_t<decltype(ledger.id())>::bytes);

    s.set_schemaid(app.schemaId().begin(), uint256::size());

    std::uint32_t uMin, uMax;
    if (!app.getLedgerMaster().getFullValidatedRange(uMin, uMax))
    {
        uMin = 0;
        uMax = 0;
    }
    else
    {
        // Don't advertise ledgers we're not willing to serve
        uMin = std::max(uMin, app.getLedgerMaster().getEarliestFetch());
    }
    s.set_firstseq(uMin);
    s.set_lastseq(uMax);

    app.peerManager().foreach(
        send_always(std::make_shared<Message>(s, protocol::mtSTATUS_CHANGE)));
    JLOG(journal.trace()) << "send status change to peer";
}
}