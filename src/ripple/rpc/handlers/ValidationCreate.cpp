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

#include <ripple/basics/Log.h>
#include <ripple/net/RPCErr.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/Seed.h>
#include <ripple/protocol/jss.h>
#include <ripple/rpc/Context.h>
#include <ripple/rpc/handlers/ValidationCreate.h>
#include <peersafe/gmencrypt/GmEncryptObj.h>

namespace ripple {

static
boost::optional<Seed>
validationSeed (std::string const &str)
{
    if (str.empty())
        return randomSeed ();

    return parseGenericSeed (str);
}

// {
//   secret: <string>   // optional
// }
//
// This command requires Role::ADMIN access because it makes
// no sense to ask an untrusted server for this.
Json::Value
doValidationCreate(RPC::JsonContext& context)
{
    std::string seedStr;
    KeyType keyType = CommonKey::chainAlgTypeG;
    if (context.params.isMember (jss::secret))
        seedStr = context.params[jss::secret].asString ();
    if (context.params.isMember (jss::key_type))
        keyType = *(keyTypeFromString(context.params[jss::key_type].asString()));

    return doFillValidationJson(keyType, seedStr);
}

Json::Value doFillValidationJson(KeyType keyType, std::string const &str)
{
    if (keyType == KeyType::invalid)
        return rpcError(rpcINVALID_PARAMS);

    Json::Value     obj(Json::objectValue);

    switch (keyType)
    {
        case KeyType::gmalg:
        {
            PublicKey pubKey;
            SecretKey secKey;
            if (!str.empty())
            {
                secKey = *(parseBase58<SecretKey>(TokenType::NodePrivate, str));
                pubKey = derivePublicKey(KeyType::gmalg, secKey);
            }
            else
            {
                auto publicPrivatePair = randomKeyPair(keyType);
                secKey = publicPrivatePair.second;
                pubKey = publicPrivatePair.first;
            }

            obj[jss::validation_public_key] = toBase58(TokenType::NodePublic, pubKey);
            obj[jss::validation_private_key] = toBase58(TokenType::NodePrivate, secKey);
            obj[jss::validation_public_key_hex] = strHex(pubKey);
            obj[jss::account_id] = toBase58(calcAccountID(pubKey));
            break;
        }
        case KeyType::secp256k1:
        case KeyType::ed25519:
        default:
        {
            auto seed = validationSeed(str);
            if (!seed)
                return rpcError(rpcBAD_SEED);

            auto const private_key = generateSecretKey(keyType, *seed);

            auto publicKey = derivePublicKey(keyType, private_key);
            obj[jss::validation_public_key_hex] = strHex(publicKey);
            obj[jss::validation_public_key] = toBase58(
                TokenType::NodePublic, publicKey);
            obj[jss::account_id] = toBase58(calcAccountID(publicKey));

            obj[jss::validation_private_key] = toBase58(
                TokenType::NodePrivate, private_key);

            obj[jss::validation_seed] = toBase58(*seed);
            obj[jss::validation_key] = seedAs1751(*seed);

            break;
        }
    }

    return obj;
}

}  // namespace ripple
