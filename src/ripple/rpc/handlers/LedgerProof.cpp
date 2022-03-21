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

#include <ripple/app/misc/Manifest.h>
#include <ripple/core/DatabaseCon.h>
#include <ripple/net/RPCErr.h>
#include <ripple/protocol/PublicKey.h>
#include <ripple/rpc/Context.h>
#include <ripple/rpc/impl/RPCHelpers.h>
#include <peersafe/schema/Schema.h>

namespace ripple {

// {
//   ledger_hash : <ledger>
//   ledger_index : <ledger_index>
// }
Json::Value
doLedgerProof(RPC::JsonContext& context)
{
    auto const& params = context.params;
    bool needsLedger = params.isMember(jss::ledger) ||
        params.isMember(jss::ledger_hash) || params.isMember(jss::ledger_index);
    if (!needsLedger)
        return rpcError(rpcINVALID_PARAMS);

    std::shared_ptr<ReadView const> ledger;
    auto result = RPC::lookupLedger(ledger, context);
    if (!ledger)
        return result;

    if (!ledger->info().validated)
    {
        return rpcError(rpcLGR_NOT_VALIDATED);
    }

    auto db = context.app.getLedgerDB().checkoutDb();

    boost::optional<std::string> sNodePubKey;
    Blob RawData;

    soci::blob sRawData(*db);
    soci::indicator rawDataPresent;

    std::ostringstream sqlSuffix;
    sqlSuffix << "WHERE LedgerSeq = " << ledger->info().seq
              << " AND LedgerHash = '" << ledger->info().hash << "'";
    std::string const sql =
        "SELECT NodePubKey, RawData "
        "FROM Validations " +
        sqlSuffix.str() + ";";

    soci::statement st =
        (db->prepare << sql,
         soci::into(sNodePubKey),
         soci::into(sRawData, rawDataPresent));

    st.execute();

    Json::Value proofs(Json::arrayValue);
    while (st.fetch())
    {
        if (sNodePubKey && rawDataPresent == soci::i_ok)
        {
            convert(sRawData, RawData);
            SerialIter sit(makeSlice(RawData));
            auto const publicKey =
                parseBase58<PublicKey>(TokenType::NodePublic, *sNodePubKey);
            if (!publicKey)
            {
                continue;
            }
            STValidation::pointer val = std::make_shared<STValidation>(
                std::ref(sit), *publicKey, [&](PublicKey const& pk) {
                    return calcNodeID(
                        context.app.validatorManifests().getMasterKey(pk));
                });
            Json::Value proof(Json::objectValue);
            if (val->getLedgerHash() == ledger->info().hash &&
                val->getFieldU32(sfLedgerSequence) == ledger->info().seq &&
                val->isFieldPresent(sfSignature))
            {
                proof[jss::validation_public_key] = *sNodePubKey;
                proof[jss::signing_time] = *(*val)[~sfSigningTime];
                proof[jss::signature] = strHex(val->getFieldVL(sfSignature));
            }
            proofs.append(proof);
        }
    }

    result[jss::status] = "success";
    result[jss::ledger_index] = ledger->info().seq;
    result[jss::ledger_hash] = to_string(ledger->info().hash);
    result[jss::proof] = proofs;

    return result;
}

}  // namespace ripple