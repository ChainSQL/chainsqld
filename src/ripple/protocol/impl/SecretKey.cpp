//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012, 2013 Ripple Labs Inc.

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

#include <ripple/basics/contract.h>
#include <ripple/basics/strHex.h>
#include <ripple/beast/utility/rngfill.h>
#include <ripple/crypto/GenerateDeterministicKey.h>
#include <ripple/crypto/csprng.h>
#include <ripple/crypto/secure_erase.h>
#include <ripple/protocol/SecretKey.h>
#include <peersafe/crypto/hashBaseObj.h>
#include <peersafe/crypto/ECIES.h>
#include <peersafe/basics/TypeTransform.h>

// #include <ripple/protocol/digest.h>
#include <ripple/protocol/impl/secp256k1.h>
#include <cstring>
#include <ed25519-donna/ed25519.h>
#include <peersafe/gmencrypt/GmCheck.h>
#include <ripple/crypto/RFC1751.h>
#include <eth/tools/Common.h>

namespace ripple {
using namespace eth;

SecretKey::~SecretKey()
{
    secure_erase(buf_, sizeof(buf_));
}

SecretKey::SecretKey(std::array<std::uint8_t, 32> const& key, KeyType keyT)
{
    std::memcpy(buf_, key.data(), key.size());
    keyTypeInt_ = keyT;
}

SecretKey::SecretKey(Slice const& slice, KeyType keyT)
{
    if (slice.size() != sizeof(buf_))
        LogicError("SecretKey::SecretKey: invalid size");
    std::memcpy(buf_, slice.data(), sizeof(buf_));
    keyTypeInt_ = keyT;
}

std::string
SecretKey::to_string() const
{
    return strHex(*this);
}

//------------------------------------------------------------------------------
/** Produces a sequence of secp256k1 key pairs. */
class Generator
{
private:
    Blob gen_;  // VFALCO compile time size?

public:
    explicit Generator(Seed const& seed)
    {
        // FIXME: Avoid copying the seed into a uint128 key only to have
        //        generateRootDeterministicPublicKey copy out of it.
        uint128 ui;
        std::memcpy(ui.data(), seed.data(), seed.size());
        gen_ = generateRootDeterministicPublicKey(ui);
    }

    /** Generate the nth key pair.

        The seed is required to produce the private key.
    */
    std::pair<PublicKey, SecretKey>
    operator()(Seed const& seed, std::size_t ordinal) const
    {
        // FIXME: Avoid copying the seed into a uint128 key only to have
        //        generatePrivateDeterministicKey copy out of it.
        uint128 ui;
        std::memcpy(ui.data(), seed.data(), seed.size());
        auto gsk = generatePrivateDeterministicKey(gen_, ui, ordinal);
        auto gpk = generatePublicDeterministicKey(gen_, ordinal);
        SecretKey const sk(Slice{gsk.data(), gsk.size()});
        PublicKey const pk(Slice{gpk.data(), gpk.size()});
        secure_erase(ui.data(), ui.size());
        secure_erase(gsk.data(), gsk.size());
        return {pk, sk};
    }
};

//------------------------------------------------------------------------------

Buffer
signDigest(PublicKey const& pk, SecretKey const& sk, uint256 const& digest)
{
    unsigned char sig[72];
    size_t len = sizeof(sig);
    // if (nullptr == hEObj)
    auto const type = publicKeyType(pk.slice());
    if (! type)
        LogicError("signDigest: invalid type");

    switch(*type)
    {
    case KeyType::secp256k1:
    {
        BOOST_ASSERT(sk.size() == 32);
        secp256k1_ecdsa_signature sig_imp;
        if (secp256k1_ecdsa_sign(
                secp256k1Context(),
                &sig_imp,
                reinterpret_cast<unsigned char const*>(digest.data()),
                reinterpret_cast<unsigned char const*>(sk.data()),
                secp256k1_nonce_function_rfc6979,
                nullptr) != 1)
            LogicError("sign: secp256k1_ecdsa_sign failed");

        if (secp256k1_ecdsa_signature_serialize_der(
                secp256k1Context(), sig, &len, &sig_imp) != 1)
            LogicError("sign: secp256k1_ecdsa_signature_serialize_der failed");
        break;
	}
	case KeyType::gmalg:
    case KeyType::gmInCard:
	{
        GmEncrypt* hEObj = GmEncryptObj::getInstance();
		BOOST_ASSERT(sk.size() == 32);
        int secKeyType = sk.keyTypeInt_ == KeyType::gmalg ? hEObj->gmOutCard : hEObj->gmInCard;
		std::pair<int, int> pri4SignInfo = std::make_pair(secKeyType, sk.encrytCardIndex_);
		std::pair<unsigned char*, int> pri4Sign = std::make_pair((unsigned char*)sk.data(), sk.size());
        std::vector<unsigned char> signedDataV;
		unsigned long rv = hEObj->SM2ECCSign(pri4SignInfo, pri4Sign, (unsigned char*)digest.data(), digest.bytes, signedDataV);
        memcpy(sig, signedDataV.data(), signedDataV.size());
        len = signedDataV.size();
		if (rv)
		{
			LogicError("sign: SM2ECCsign failed");
		}
        break;
	}
    default:
        LogicError("signDigest: invalid type");
    }

    return Buffer{sig, len};
}

Buffer
sign(PublicKey const& pk, SecretKey const& sk, Slice const& m)
{
    auto const type = publicKeyType(pk.slice());
    if (!type)
        LogicError("sign: invalid type");

    switch (*type)
    {
        case KeyType::ed25519: {
            Buffer b(64);
            ed25519_sign(
                m.data(), m.size(), sk.data(), pk.data() + 1, b.data());
            return b;
        }
        case KeyType::secp256k1: {
            std::unique_ptr<hashBase> hasher = hashBaseObj::getHasher(CommonKey::sha);
            (*hasher)(m.data(), m.size());
            auto const digest = sha512_half_hasher::result_type(*hasher);

            secp256k1_ecdsa_signature sig_imp;
            if (secp256k1_ecdsa_sign(
                    secp256k1Context(),
                    &sig_imp,
                    reinterpret_cast<unsigned char const*>(digest.data()),
                    reinterpret_cast<unsigned char const*>(sk.data()),
                    secp256k1_nonce_function_rfc6979,
                    nullptr) != 1)
                LogicError("sign: secp256k1_ecdsa_sign failed");

            unsigned char sig[72];
            size_t len = sizeof(sig);
            if (secp256k1_ecdsa_signature_serialize_der(
                    secp256k1Context(), sig, &len, &sig_imp) != 1)
                LogicError(
                    "sign: secp256k1_ecdsa_signature_serialize_der failed");

            return Buffer{sig, len};
        }
        case KeyType::gmalg:
        case KeyType::gmInCard:
        {
            Blob signedDataV;
            unsigned char hashData[32] = {0};
            unsigned long hashDataLen = 32;

            GmEncrypt* hEObj = GmEncryptObj::getInstance();
            hEObj->SM3HashTotal(
                (unsigned char*)m.data(), m.size(), hashData, &hashDataLen);

            int secKeyType = sk.keyTypeInt_ == KeyType::gmalg ? hEObj->gmOutCard : hEObj->gmInCard;
            std::pair<int, int> pri4SignInfo =
                std::make_pair(secKeyType, sk.encrytCardIndex_);
            std::pair<unsigned char*, int> pri4Sign =
                std::make_pair((unsigned char*)sk.data(), sk.size());

            unsigned long rv = hEObj->SM2ECCSign(
                pri4SignInfo, pri4Sign, hashData, hashDataLen, signedDataV);
            if (rv)
            {
                LogicError("sign: SM2ECCSign failed");
            }
            return Buffer{signedDataV.data(), signedDataV.size()};
        }
        default:
            LogicError("sign: invalid type");
    }
}

static const u256 scSecp256k1n("115792089237316195423570985008687907852837564279074904382605163141518161494337");

Signature signEthDigest(SecretKey const& sk, uint256 const& digest)
{
    auto* ctx = secp256k1Context();
    secp256k1_ecdsa_recoverable_signature rawSig;
    if (!secp256k1_ecdsa_sign_recoverable(ctx, &rawSig, digest.data(), sk.data(), nullptr, nullptr))
        return {};

    Signature s;
    int v = 0;
    secp256k1_ecdsa_recoverable_signature_serialize_compact(ctx, s.data(), &v, &rawSig);

    SignatureStruct& ss = *reinterpret_cast<SignatureStruct*>(&s);
    ss.v = static_cast<byte>(v);
    const auto cuCheck = fromU256(scSecp256k1n / 2);
    if (ss.s > cuCheck)
    {
        ss.v = static_cast<byte>(ss.v ^ 1);
        ss.s = fromU256(scSecp256k1n - u256(ss.s));
    }
    assert(ss.s <= cuCheck);
    return s;
}
Blob
decrypt(const Blob& cipherBlob, const SecretKey& secret_key)
{
    GmEncrypt* hEObj = GmEncryptObj::getInstance();
    if (KeyType::secp256k1 == secret_key.keyTypeInt_ || KeyType::ed25519 == secret_key.keyTypeInt_)
    {
        // Blob secretBlob(secret_key.data(), secret_key.data() +secret_key.size());
        return ripple::asymDecrypt(cipherBlob, secret_key);
    }
    else
    {
        Blob resPlainText;
        int secKeyType = secret_key.keyTypeInt_ == KeyType::gmalg ? hEObj->gmOutCard : hEObj->gmInCard;
		std::pair<int, int> pri4DecryptInfo = 
            std::make_pair(secKeyType, secret_key.encrytCardIndex_);
        std::pair<unsigned char*, int> pri4Decrypt = 
            std::make_pair((unsigned char*)secret_key.data(), secret_key.size());
        hEObj->SM2ECCDecrypt(
            pri4DecryptInfo, pri4Decrypt, (unsigned char*)&cipherBlob[0], cipherBlob.size(), resPlainText);
		
        return resPlainText;
    }
}

boost::optional<SecretKey> getSecretKey(const std::string& secret)
{
    //tx_secret is actually master seed
    // if (GmEncryptObj::getInstance())
    if ('p' == secret[0])
    {
        std::string priKeyStrDe58 = decodeBase58Token(secret, TokenType::AccountSecret);
        if(priKeyStrDe58.empty())
        {
            boost::optional<SecretKey> ret;
            return ret;
        }
        else
            return SecretKey(Slice(priKeyStrDe58.c_str(), priKeyStrDe58.size()), KeyType::gmalg);
    }
    else if('x' == secret[0])
    {
        boost::optional<SecretKey> oSecret_key;
        if (secret.size() > 0)
        {
            auto seed = parseBase58<Seed>(secret);
            KeyType keyType = KeyType::secp256k1;
            oSecret_key = generateKeyPair(keyType, *seed).second;
        }
        return oSecret_key;
	}
	else {
		assert(0);
		boost::optional<SecretKey> ret;
		return ret;
	}
}

SecretKey
randomSecretKey()
{
    std::uint8_t buf[32];
    beast::rngfill(buf, sizeof(buf), crypto_prng());
    SecretKey sk(Slice{buf, sizeof(buf)});
    secure_erase(buf, sizeof(buf));
    return sk;
}

// VFALCO TODO Rewrite all this without using OpenSSL
//             or calling into GenerateDetermisticKey
SecretKey
generateSecretKey(KeyType type, Seed const& seed)
{
    if (type == KeyType::ed25519)
    {
        auto key = sha512Half_s(Slice(seed.data(), seed.size()));
        SecretKey sk = Slice{key.data(), key.size()};
        secure_erase(key.data(), key.size());
        return sk;
    }

    if (type == KeyType::secp256k1)
    {
        // FIXME: Avoid copying the seed into a uint128 key only to have
        //        generateRootDeterministicPrivateKey copy out of it.
        uint128 ps;
        std::memcpy(ps.data(), seed.data(), seed.size());
        auto const upk = generateRootDeterministicPrivateKey(ps);
        SecretKey sk = Slice{upk.data(), upk.size()};
        secure_erase(ps.data(), ps.size());
        return sk;
    }

    LogicError("generateSecretKey: unknown key type");
}

PublicKey
derivePublicKey(KeyType type, SecretKey const& sk)
{
    switch (type)
    {
        case KeyType::secp256k1: {
            secp256k1_pubkey pubkey_imp;
            if (secp256k1_ec_pubkey_create(
                    secp256k1Context(),
                    &pubkey_imp,
                    reinterpret_cast<unsigned char const*>(sk.data())) != 1)
                LogicError(
                    "derivePublicKey: secp256k1_ec_pubkey_create failed");

            unsigned char pubkey[33];
            std::size_t len = sizeof(pubkey);
            if (secp256k1_ec_pubkey_serialize(
                    secp256k1Context(),
                    pubkey,
                    &len,
                    &pubkey_imp,
                    SECP256K1_EC_COMPRESSED) != 1)
                LogicError(
                    "derivePublicKey: secp256k1_ec_pubkey_serialize failed");

            return PublicKey{Slice{pubkey, len}};
        }
        case KeyType::ed25519: {
            unsigned char buf[33];
            buf[0] = 0xED;
            ed25519_publickey(sk.data(), &buf[1]);
            return PublicKey(Slice{buf, sizeof(buf)});
        }
        case KeyType::gmalg: {
            GmEncrypt* hEObj = GmEncryptObj::getInstance();
            std::vector<unsigned char> tempPublickey;
            hEObj->generatePubFromPri(sk.data(), sk.size(), tempPublickey);
            return PublicKey(Slice(tempPublickey.data(), tempPublickey.size()));
        }
        default:
            LogicError("derivePublicKey: bad key type");
    };
}

std::pair<PublicKey, SecretKey>
generateKeyPair(KeyType type, Seed const& seed)
{
    switch (type)
    {
        case KeyType::secp256k1: {
            Generator g(seed);
            return g(seed, 0);
        }
        default:
        case KeyType::ed25519: {
            auto const sk = generateSecretKey(type, seed);
            return {derivePublicKey(type, sk), sk};
        }
        case KeyType::gmalg:
        {
            const int randomCheckLen = 32;  // 256bit = 32 byte
            GmEncrypt* hEObj = GmEncryptObj::getInstance();
            if (!hEObj->randomSingleCheck(randomCheckLen))
            {
                LogicError("randomSingleCheck failed!");
            }

            std::vector<unsigned char> tempPublickey;
            std::vector<unsigned char> tempPrivatekey;
            Seed rootSeed = generateSeed("masterpassphrase");
            std::string strRootSeed((char*)rootSeed.data(), rootSeed.size());
            std::string strSeed((char*)seed.data(), seed.size());
            bool isRoot = strRootSeed != strSeed ? false : true;
            hEObj->SM2GenECCKeyPair(tempPublickey, tempPrivatekey, isRoot);

            return std::make_pair(PublicKey(Slice(tempPublickey.data(), tempPublickey.size())),
                                  SecretKey(Slice(tempPrivatekey.data(), tempPrivatekey.size()), KeyType::gmalg));
        }
        
    }
}

std::pair<PublicKey, SecretKey>
randomKeyPair(KeyType type)
{
    if (KeyType::gmalg == type)
    {
		const int randomCheckLen = 32; //256bit = 32 byte
        GmEncrypt* hEObj = GmEncryptObj::getInstance();
        if (!hEObj->randomSingleCheck(randomCheckLen))
		{
			LogicError("randomSingleCheck failed!");
		}

        std::vector<unsigned char> tempPublickey;
        std::vector<unsigned char> tempPrivatekey;
        hEObj->SM2GenECCKeyPair(tempPublickey, tempPrivatekey);

        return std::make_pair(PublicKey(Slice(tempPublickey.data(), tempPublickey.size())),
                              SecretKey(Slice(tempPrivatekey.data(), tempPrivatekey.size()), type));
    }
    else
    {
        auto const sk = randomSecretKey();
        return{ derivePublicKey(type, sk), sk };
    }
}

template <>
boost::optional<SecretKey>
parseBase58(TokenType type, std::string const& s)
{
    auto const result = decodeBase58Token(s, type);
    if (result.empty())
        return boost::none;
    if (result.size() != 32)
        return boost::none;
    return SecretKey(makeSlice(result));
}

/** Encode a Secret in RFC1751 format */
std::string
secretKeyAs1751(SecretKey const& secretKey)
{
    std::string key;

    std::reverse_copy(secretKey.data(), secretKey.data() + 32, std::back_inserter(key));

    std::string encodedKey;
    RFC1751::getEnglishFromKey(encodedKey, key);
    return encodedKey;
}

}  // namespace ripple
