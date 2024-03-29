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

#include <peersafe/schema/Schema.h>
#include <ripple/basics/Log.h>
#include <ripple/json/json_value.h>
#include <ripple/net/RPCErr.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/jss.h>
#include <ripple/rpc/Context.h>
#include <boost/algorithm/string/predicate.hpp>
#include <ripple/beast/core/LexicalCast.h>
#include <ripple/nodestore/Database.h>
#include <ripple/app/ledger/LedgerMaster.h>

namespace ripple {

    Json::Value doNodeSize(RPC::JsonContext& context)
    {
        if (!context.params.isMember(jss::node_size))
        {
            Json::Value ret(Json::objectValue);

            ret[jss::node_size] = (unsigned int)context.app.config().NODE_SIZE;

            switch (context.app.config().NODE_SIZE)
            {
            case 0:
                ret[jss::node_size] = "tiny"; break;
            case 1:
                ret[jss::node_size] = "small"; break;
            case 2:
                ret[jss::node_size] = "medium"; break;
            case 3:
                ret[jss::node_size] = "large"; break;
            case 4:
                ret[jss::node_size] = "huge"; break;
            default:
                ret[jss::node_size] = "unknown"; break;
            }

            return ret;
        }

        auto node_size = context.params[jss::node_size].asString();

        if (boost::iequals(node_size, "tiny"))
            context.app.config().NODE_SIZE = 0;
        else if (boost::iequals(node_size, "small"))
            context.app.config().NODE_SIZE = 1;
        else if (boost::iequals(node_size, "medium"))
            context.app.config().NODE_SIZE = 2;
        else if (boost::iequals(node_size, "large"))
            context.app.config().NODE_SIZE = 3;
        else if (boost::iequals(node_size, "huge"))
            context.app.config().NODE_SIZE = 4;
        else
        {
            try
            {
                context.app.config().NODE_SIZE =
                    beast::lexicalCastThrow<unsigned int>(node_size);
            }
            catch (std::exception const&)
            {
                context.app.config().NODE_SIZE = 0;
            }

            if (context.app.config().NODE_SIZE > 4)
                context.app.config().NODE_SIZE = 4;
        }

        //context.app.getNodeStore().tune(context.app.config().getSize(SizedItem::nodeCacheSize), std::chrono::seconds(context.app.config().getSize(SizedItem::nodeCacheAge)));
        //context.app.getLedgerMaster().tune(context.app.config().getSize(SizedItem::ledgerSize), std::chrono::seconds(context.app.config().getSize(SizedItem::ledgerAge)));
        //context.app.getNodeFamily().treecache().setTargetSize(context.app.config().getSize(SizedItem::treeCacheSize));
        //context.app.getNodeFamily().treecache().setTargetAge(std::chrono::seconds(context.app.config().getSize(SizedItem::treeCacheAge)));
        
        return Json::objectValue;
    }

} // ripple
