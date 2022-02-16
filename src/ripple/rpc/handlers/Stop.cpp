//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012-2014 Ripple Labs Inc.

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

#include <peersafe/schema/Schema.h>
#include <ripple/app/main/Application.h>
#include <ripple/json/json_value.h>
#include <ripple/rpc/impl/Handler.h>
#include <ripple/net/RPCErr.h>
#include <peersafe/schema/SchemaManager.h>
#include <peersafe/app/table/TableSync.h>
#include <mutex>

namespace ripple {

namespace RPC {
struct JsonContext;
}

Json::Value
doStop(RPC::JsonContext& context)
{
    uint256 schemaID = beast::zero;
    if (context.params.isMember(jss::schema))
    {
        auto const schema = context.params[jss::schema].asString();
        if (schema.length() < 64)
            return rpcError(rpcINVALID_PARAMS); 
        schemaID = from_hex_text<uint256>(schema);

    }

	if (!context.params.isMember(jss::schema) || schemaID.isZero())
    {
        std::unique_lock lock{context.app.getMasterMutex()};
        context.app.app().signalStop();
        return RPC::makeObjectValue(systemName() + " server stopping");
    }
    else
    {
         auto const schema = context.params[jss::schema].asString();
         auto schemaID = from_hex_text<uint256>(schema);
         if (context.app.getSchemaManager().contains(schemaID))
         {
             if(!context.app.app().getSchema(schemaID).isShutdown())
             {
                 context.app.app().getJobQueue().addJob(
                     jtSTOP_SCHEMA, "StopSchema", [context, schemaID](Job&) {
                         context.app.app().doStopSchema(schemaID);
                     });
                 return RPC::makeObjectValue("schemaID: " + schema + " server stopping");
             }
             context.app.getSchemaManager().removeSchema(schemaID);
             return RPC::makeObjectValue("schemaID: " + schema + " server stopped");
         }
    }
    return rpcError(rpcINVALID_PARAMS);
    
    
}

}  // namespace ripple
