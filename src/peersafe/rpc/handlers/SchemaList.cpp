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

namespace ripple {
/** General RPC command that can retrieve schema list.
	{
		account: <account>|<account_public_key>// optional
		ledger_hash: <string> // optional
		ledger_index: <string | unsigned integer> // optional
	}
*/
Json::Value doSchemaList(RPC::Context&  context)
{
	std::shared_ptr<ReadView const> ledger;
	auto result = RPC::lookupLedger(ledger, context);
	if (ledger == nullptr)
		return result;

	Json::Value ret(Json::objectValue);
	ret[jss::schemas] = Json::Value(Json::arrayValue);

	//This is a time-consuming process for a project that has many sles.
	for (auto sle : ledger->sles)
	{
		if (sle->getType() == ltSCHEMA)
		{
			Json::Value schema(Json::objectValue);
			schema[jss::schema_id] = to_string(sle->key());
			schema[jss::schema_name] = strCopy(sle->getFieldVL(sfSchemaName));
			schema[jss::schema_strategy] = sle->getFieldU8(sfSchemaStrategy);
			if (sle->isFieldPresent(sfSchemaAdmin))
				schema[jss::schema_admin] = to_string(sle->getAccountID(sfAccount));
			if (sle->isFieldPresent(sfAnchorLedgerHash))
				schema[jss::anchor_ledge_hash] = to_string(sle->getFieldH256(sfAnchorLedgerHash));

			Json::Value& jvValidators = (schema[jss::validators] = Json::arrayValue);
			for (auto& validator : sle->getFieldArray(sfValidators))
			{
				auto publicKey = PublicKey(makeSlice(validator.getFieldVL(sfPublicKey)));
				jvValidators.append(Json::Value(toBase58(
					TokenType::NodePublic, publicKey)));
			}

			Json::Value& jvPeers = (schema[jss::peer_list] = Json::arrayValue);
			for (auto& peer : sle->getFieldArray(sfPeerList))
			{
				jvPeers.append(Json::Value(strCopy(peer.getFieldVL(sfEndPoint))));
			}
			ret[jss::schemas].append(schema);
		}
	}
	return ret;
}
}