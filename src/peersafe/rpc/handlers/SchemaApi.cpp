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

#include <ripple/json/json_value.h>
#include <ripple/net/RPCErr.h>
#include <ripple/protocol/jss.h>
#include <ripple/rpc/Context.h>
#include <ripple/rpc/handlers/Handlers.h>
#include <ripple/rpc/impl/RPCHelpers.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/app/main/Application.h>
#include <peersafe/schema/SchemaManager.h>

namespace ripple {
/** General RPC command that can retrieve schema list.
	{
		account: <account>|<account_public_key>// optional
		ledger_hash: <string> // optional
		ledger_index: <string | unsigned integer> // optional
	}
*/

Json::Value getSchemaFromSle(std::shared_ptr<SLE const> & sle)
{
	Json::Value schema(Json::objectValue);

	if (!sle)
	{
		return schema;
	}

	schema[jss::schema_id]                   = to_string(sle->key());
	schema[jss::schema_name]                 = strCopy(sle->getFieldVL(sfSchemaName));
	schema[jss::schema_strategy]             = sle->getFieldU8(sfSchemaStrategy);
	if (sle->isFieldPresent(sfSchemaAdmin))
		schema[jss::schema_admin]            = to_string(sle->getAccountID(sfSchemaAdmin));
	if (sle->isFieldPresent(sfAnchorLedgerHash))
		schema[jss::anchor_ledge_hash]       = to_string(sle->getFieldH256(sfAnchorLedgerHash));

	Json::Value& jvValidators = (schema[jss::validators] = Json::arrayValue);
	for (auto& validator : sle->getFieldArray(sfValidators))
	{
		Json::Value val(Json::objectValue);
		auto publicKey = PublicKey(makeSlice(validator.getFieldVL(sfPublicKey)));
		val[jss::pubkey_validator] = Json::Value(toBase58(TokenType::NodePublic, publicKey));
		val[jss::val_signed] = validator.getFieldU8(sfSigned);

		jvValidators.append(val);
	}

	Json::Value& jvPeers = (schema[jss::peer_list] = Json::arrayValue);
	for (auto& peer : sle->getFieldArray(sfPeerList))
	{
		jvPeers.append(Json::Value(strCopy(peer.getFieldVL(sfEndpoint))));
	}

	return schema;
}

Json::Value doSchemaList(RPC::JsonContext&  context)
{
	std::shared_ptr<ReadView const> ledger;
	auto result = RPC::lookupLedger(ledger, context);
	if (ledger == nullptr)
		return result;

	Json::Value ret(Json::objectValue);
	ret[jss::schemas] = Json::Value(Json::arrayValue);

	boost::optional<AccountID> pAccountID = boost::none;
	if (context.params.isMember(jss::account))
	{
		pAccountID = parseBase58<AccountID>(context.params["account"].asString());
		if (pAccountID == boost::none)
		{
			return rpcError(rpcINVALID_PARAMS);
		}
	}

	bool bHasRunning=false,bRunning = false;
	bHasRunning = context.params.isMember(jss::running);
	bRunning = bHasRunning && context.params[jss::running].asBool();

	//This is a time-consuming process for a project that has many sles.
    auto sleIndex = ledger->read(keylet::schema_index());
    if (!sleIndex)
        return ret;
    else
    {
        auto& schemaIndexes = sleIndex->getFieldV256(sfSchemaIndexes);
        for (auto const& index : schemaIndexes)
        {
            auto key = Keylet(ltSCHEMA, index);
            auto sle = ledger->read(key);
            if (sle)
            {
                if (pAccountID != boost::none && sle->getAccountID(sfAccount) != *pAccountID)
                    continue;

                Json::Value schema = getSchemaFromSle(sle);
                if (bHasRunning && context.app.app().hasSchema(sle->key()) != bRunning)
                    continue;

                ret[jss::schemas].append(schema);
			}
		}
	}

	return ret;
}

Json::Value doSchemaInfo(RPC::JsonContext& context)
{
	if (!context.params.isMember(jss::schema))
		return rpcError(rpcINVALID_PARAMS);

	std::shared_ptr<ReadView const> ledger;
	auto result = RPC::lookupLedger(ledger, context);
	if (ledger == nullptr)
		return result;


	auto const schemaID = context.params[jss::schema].asString();
	auto key = Keylet(ltSCHEMA, from_hex_text<uint256>(schemaID));
	auto sle = ledger->read(key);
	if (!sle)
	{
		return rpcError(rpcNO_SCHEMA);
	}

	return getSchemaFromSle(sle);

}

Json::Value doSchemaAccept(RPC::JsonContext& context)
{
	if (!context.params.isMember(jss::schema))
		return rpcError(rpcINVALID_PARAMS);

	std::shared_ptr<ReadView const> ledger;
	auto result = RPC::lookupLedger(ledger, context);
	if (ledger == nullptr)
		return result;


	auto const schemaID = from_hex_text<uint256>(context.params[jss::schema].asString());
	auto sleKey = Keylet(ltSCHEMA, schemaID);
	auto sleSchema = ledger->read(sleKey);
	if (!sleSchema)
	{
		return rpcError(rpcNO_SCHEMA);
	}
	if (context.app.app().hasSchema(schemaID))
		return rpcError(rpcSCHEMA_CREATED);

	Json::Value jvResult(Json::objectValue);

	SchemaParams params{};
	params.readFromSle(sleSchema);
	bool bShouldCreate = false;
	for (auto& validator : params.validator_list)
    {
        if (context.app.app().getValidationPublicKey().size() != 0 &&
            validator.first == context.app.app().getValidationPublicKey())
        {
            bShouldCreate = true;
        }
    }
	if (!bShouldCreate)
		return rpcError(rpcSCHEMA_NOTMEMBER);

	auto ret = context.app.getOPs().createSchema(sleSchema, true);
	if (!ret.first)
	{
		JLOG(context.j.fatal())<< ret.second;
		jvResult[jss::error] = "internal error";
		jvResult[jss::error_message] = ret.second;
	}
	
	jvResult[jss::status] = jss::success;

	return jvResult;
}


Json::Value doSchemaStart(RPC::JsonContext& context)
{
	if (!context.params.isMember(jss::schema))
		return rpcError(rpcINVALID_PARAMS);

	std::shared_ptr<ReadView const> ledger;
	auto result = RPC::lookupLedger(ledger, context);
	if (ledger == nullptr)
		return result;

	auto const schema = context.params[jss::schema].asString();
	auto schemaID = from_hex_text<uint256>(schema);
	auto key = Keylet(ltSCHEMA, schemaID);
	auto sle = ledger->read(key);
	if (!sle)
	{
		return rpcError(rpcNO_SCHEMA);
	}
	if (!context.app.getSchemaManager().contains(schemaID))
	{
		 auto schemaPath =
            boost::filesystem::path(context.app.config().SCHEMA_PATH) /
            to_string(schemaID);
         bool bForceCreate =
            boost::filesystem::exists(schemaPath);
         context.app.getOPs().createSchema(sle, bForceCreate, true);
    }
	Json::Value jvResult(Json::objectValue);
	jvResult[jss::status] = jss::success;
	return jvResult;
}


}
