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

#include <ripple/basics/Log.h>
#include <ripple/net/RPCErr.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/jss.h>
#include <ripple/rpc/Context.h>
#include <ripple/shamap/SHAMap.h>
#include <peersafe/schema/Schema.h>

namespace ripple {

Json::Value
doShamapDump(RPC::JsonContext& context)
{
    Json::Value ret(Json::objectValue);

    Json::Value params(context.params);
    if (!params.isMember("root"))
    {
        return RPC::missing_field_error("root");
    }
    bool bIncludeHash = false;

    auto sRoot = params["root"].asString();
    if (params.isMember(jss::hash))
        bIncludeHash = params[jss::hash].asBool();

    auto mapPtr = std::make_shared<SHAMap>(
        SHAMapType::CONTRACT, context.app.getNodeFamily());
    auto rootHash = from_hex_text<uint256>(sRoot);
    if (mapPtr->fetchRoot(SHAMapHash{rootHash}, nullptr))
    {
        mapPtr->dump(bIncludeHash);
    }
    else
    {
        JLOG(context.j.warn())
            << "Get root failed for shamap root: " << to_string(rootHash);
    }

    return ret;
}

}  // namespace ripple
