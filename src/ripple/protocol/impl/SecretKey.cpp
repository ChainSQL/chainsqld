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

#include <BeastConfig.h>
#include <ripple/basics/strHex.h>
#include <ripple/protocol/SecretKey.h>
#include <ripple/protocol/digest.h>
#include <ripple/protocol/impl/secp256k1.h>
#include <ripple/basics/contract.h>
#include <ripple/crypto/GenerateDeterministicKey.h>
#include <ripple/crypto/csprng.h>
#include <ripple/beast/crypto/secure_erase.h>
#include <ripple/beast/utility/rngfill.h>
#include <ed25519-donna/ed25519.h>
#include <cstring>
#include <peersafe/gmencrypt/hardencrypt/gmCheck.h>

namespace ripple {

SecretKey::~SecretKey()
{
    beast::secure_erase(buf_, sizeof(buf_));
}

SecretKey::SecretKey (std::array<std::uint8_t, 32> const& key)
{
    std::memcpy(buf_, key.data(), key.size());
}

SecretKey::SecretKey (Slice const& slice)
{
    if (slice.size() != sizeof(buf_))
        LogicError("SecretKey::SecretKey: invalid size");
    std::memcpy(buf_, slice.data(), sizeof(buf_));
}

std::string
SecretKey::to_string() const
{
    return strHex(data(), size());
}

//------------------------------------------------------------------------------
/** Produces a sequence of secp256k1 key pairs. */
class Generator
{
private:
    Blob gen_; // VFALCO compile time size?

public:
    explicit
    Generator (Seed const& seed)
    {
        // FIXME: Avoid copying the seed into a uint128 key only to have
        //        generateRootDeterministicPublicKey copy out of it.
        uint128 ui;
        std::memcpy(ui.data(),
            seed.data(), seed.size());
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
        SecretKey const sk(Slice
        { gsk.data(), gsk.size() });
        PublicKey const pk(Slice
        { gpk.data(), gpk.size() });
        beast::secure_erase(ui.data(), ui.size());
        beast::secure_erase(gsk.data(), gsk.size());
        return {pk, sk};
    }
};

//------------------------------------------------------------------------------

Buffer
signDigest (PublicKey const& pk, SecretKey const& sk,
    uint256 const& digest)
{
    unsigned char sig[72];
    size_t len = sizeof(sig);
    HardEncrypt* hEObj = HardEncryptObj::getInstance();
    if (nullptr == hEObj)
    {
        if (publicKeyType(pk.slice()) != KeyType::secp256k1)
            LogicError("sign: secp256k1 required for digest signing");
        BOOST_ASSERT(sk.size() == 32);
        secp256k1_ecdsa_signature sig_imp;
        if (secp256k1_ecdsa_sign(
            secp256k1Context(),
            &sig_imp,
            reinterpret_cast<unsigned char const*>(
                digest.data()),
            reinterpret_cast<unsigned char const*>(
                sk.data()),
            secp256k1_nonce_function_rfc6979,
            nullptr) != 1)
        LogicError("sign: secp256k1_ecdsa_sign failed");

		if (secp256k1_ecdsa_signature_serialize_der(
			secp256k1Context(),
			sig,
			&len,
			&sig_imp) != 1)
			LogicError("sign: secp256k1_ecdsa_signature_serialize_der failed");
	}
	else
	{
		if (publicKeyType(pk.slice()) != KeyType::gmalg)
			LogicError("sign: GM algorithm required for digest signing");
		BOOST_ASSERT(sk.size() == 32);
		std::pair<int, int> pri4SignInfo = std::make_pair(sk.keyTypeInt, sk.encrytCardIndex);
		std::pair<unsigned char*, int> pri4Sign = std::make_pair((unsigned char*)sk.data(), sk.size());
		unsigned long rv = hEObj->SM2ECCSign(pri4SignInfo, pri4Sign, (unsigned char*)digest.data(), digest.bytes, sig, (unsigned long*)&len);
		if (rv)
		{
			DebugPrint("ECCSign error! rv = 0x%04x", rv);
			LogicError("sign: SM2ECCsign failed");
		}
	}

    return Buffer{sig, len};
}

Buffer
sign (PublicKey const& pk,
    SecretKey const& sk, Slice const& m)
{
    auto const type =
        publicKeyType(pk.slice());
    if (! type)
        LogicError("sign: invalid type");
    switch(*type)
    {
    case KeyType::ed25519:
    {
        Buffer b(64);
        ed25519_sign(m.data(), m.size(),
            sk.data(), pk.data() + 1, b.data());
        return b;
    }
    case KeyType::secp256k1:
    {
        sha512_half_hasher h;
        h(m.data(), m.size());
        auto const digest =
            sha512_half_hasher::result_type(h);

        secp256k1_ecdsa_signature sig_imp;
        if(secp256k1_ecdsa_sign(
                secp256k1Context(),
                &sig_imp,
                reinterpret_cast<unsigned char const*>(
                    digest.data()),
                reinterpret_cast<unsigned char const*>(
                    sk.data()),
                secp256k1_nonce_function_rfc6979,
                nullptr) != 1)
            LogicError("sign: secp256k1_ecdsa_sign failed");

        unsigned char sig[72];
        size_t len = sizeof(sig);
        if(secp256k1_ecdsa_signature_serialize_der(
                secp256k1Context(),
                sig,
                &len,
                &sig_imp) != 1)
            LogicError("sign: secp256k1_ecdsa_signature_serialize_der failed");

        return Buffer{sig, len};
    }
    case KeyType::gmalg:
    {
        unsigned long rv = 0;
        unsigned char outData[256] = { 0 };
        unsigned long outDataLen = 256;
        unsigned char hashData[32] = { 0 };
        unsigned long hashDataLen = 32;

        HardEncrypt* hEObj = HardEncryptObj::getInstance();
        hEObj->SM3HashTotal((unsigned char*)m.data(), m.size(), hashData, &hashDataLen);

		std::pair<int, int> pri4SignInfo = std::make_pair(sk.keyTypeInt, sk.encrytCardIndex);
		std::pair<unsigned char*, int> pri4Sign = std::make_pair((unsigned char*)sk.data(), sk.size());

        rv = hEObj->SM2ECCSign(pri4SignInfo, pri4Sign, hashData, hashDataLen, outData, &outDataLen);
        if (rv)
        {
            DebugPrint("SM2ECCSign error! rv = 0x%04x", rv);
            LogicError("sign: SM2ECCSign failed");
        }
        return Buffer{ outData,outDataLen };
    }
    default:
        LogicError("sign: invalid type");
    }
}

Blob
decrypt(const Blob& cipherBlob, const SecretKey& secret_key)
{
    HardEncrypt* hEObj = HardEncryptObj::getInstance();
    if (nullptr != hEObj) //GM Algorithm
    {
        unsigned long rv = 0;
        unsigned char plain[512] = { 0 };
        unsigned long plainLen = 512;
		std::pair<int, int> pri4DecryptInfo = std::make_pair(secret_key.keyTypeInt, secret_key.encrytCardIndex);
        std::pair<unsigned char*, int> pri4Decrypt = std::make_pair((unsigned char*)secret_key.data(), secret_key.size());
        rv = hEObj->SM2ECCDecrypt(pri4DecryptInfo, pri4Decrypt, (unsigned char*)&cipherBlob[0], cipherBlob.size(), plain, &plainLen);
        if (rv)
        {
            DebugPrint("ECCDecrypt error! rv = 0x%04x", rv);
            return Blob();
        }
        DebugPrint("ECCDecrypt OK!");
        //Blob    vucPlainText(plain, plain + plainLen);
        return Blob(plain, plain + plainLen);
    }
    else
    {
        Blob secretBlob(secret_key.data(), secret_key.data() +secret_key.size());
        return RippleAddress::decryptPassword(cipherBlob, secretBlob);
    }
}

boost::optional<SecretKey> getSecretKey(const std::string& secret)
{
    //tx_secret is acturally masterseed
    if (HardEncryptObj::getInstance())
    {
        std::string privateKeyStrDe58 = decodeBase58Token(secret, TOKEN_ACCOUNT_SECRET);
        return SecretKey(Slice(privateKeyStrDe58.c_str(), strlen(privateKeyStrDe58.c_str())));
    }
    else
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
}

boost::optional<PublicKey> getPublicKey(const std::string& secret)
{
    //tx_secret is acturally masterseed
    boost::optional<PublicKey> oPublic_key;
    if (secret.size() > 0)
    {
        KeyType keyType = KeyType::secp256k1;
        if (HardEncryptObj::getInstance())
        {
            keyType = KeyType::gmalg;
            Seed seed = randomSeed();
            oPublic_key = generateKeyPair(keyType, seed).first;
        }
        else
        {
            auto seed = parseBase58<Seed>(secret);
            oPublic_key = generateKeyPair(keyType, *seed).first;
        }
    }
    return oPublic_key;
}

SecretKey
randomSecretKey()
{
    std::uint8_t buf[32];
    beast::rngfill(
        buf,
        sizeof(buf),
        crypto_prng());
    SecretKey sk(Slice{ buf, sizeof(buf) });
    beast::secure_erase(buf, sizeof(buf));
    return sk;
}

// VFALCO TODO Rewrite all this without using OpenSSL
//             or calling into GenerateDetermisticKey
SecretKey
generateSecretKey (KeyType type, Seed const& seed)
{
    if (type == KeyType::ed25519)
    {
        auto key = sha512Half_s(Slice(
            seed.data(), seed.size()));
        SecretKey sk = Slice{ key.data(), key.size() };
        beast::secure_erase(key.data(), key.size());
        return sk;
    }

    if (type == KeyType::secp256k1)
    {
        // FIXME: Avoid copying the seed into a uint128 key only to have
        //        generateRootDeterministicPrivateKey copy out of it.
        uint128 ps;
        std::memcpy(ps.data(),
            seed.data(), seed.size());
        auto const upk =
            generateRootDeterministicPrivateKey(ps);
        SecretKey sk = Slice{ upk.data(), upk.size() };
        beast::secure_erase(ps.data(), ps.size());
        return sk;
    }

    LogicError ("generateSecretKey: unknown key type");
}

PublicKey
derivePublicKey (KeyType type, SecretKey const& sk)
{
    switch(type)
    {
    case KeyType::secp256k1:
    {
        secp256k1_pubkey pubkey_imp;
        if(secp256k1_ec_pubkey_create(
                secp256k1Context(),
                &pubkey_imp,
                reinterpret_cast<unsigned char const*>(
                    sk.data())) != 1)
            LogicError("derivePublicKey: secp256k1_ec_pubkey_create failed");

        unsigned char pubkey[33];
        size_t len = sizeof(pubkey);
        if(secp256k1_ec_pubkey_serialize(
                secp256k1Context(),
                pubkey,
                &len,
                &pubkey_imp,
                SECP256K1_EC_COMPRESSED) != 1)
            LogicError("derivePublicKey: secp256k1_ec_pubkey_serialize failed");

        return PublicKey{Slice{pubkey,
            static_cast<std::size_t>(len)}};
    }
    case KeyType::ed25519:
    {
        unsigned char buf[33];
        buf[0] = 0xED;
        ed25519_publickey(sk.data(), &buf[1]);
        return PublicKey(Slice{ buf, sizeof(buf) });
    }
    default:
        LogicError("derivePublicKey: bad key type");
    };
}

std::pair<PublicKey, SecretKey>
generateKeyPair (KeyType type, Seed const& seed)
{
    switch(type)
    {
    case KeyType::secp256k1:
    {
        Generator g(seed);
        return g(seed, 0);
    }
    default:
    case KeyType::ed25519:
    {
        auto const sk = generateSecretKey(type, seed);
        return { derivePublicKey(type, sk), sk };
    }
    case KeyType::gmalg:
    {
		const int randomCheckLen = 32; //256bit = 32 byte
		if (!GMCheck::getInstance()->randomSingleCheck(randomCheckLen))
		{
			LogicError("randomSingleCheck failed!");
		}
        HardEncrypt* hEObj = HardEncryptObj::getInstance();
        std::pair<unsigned char*, int> tempPublickey;
        std::pair<unsigned char*, int> tempPrivatekey;
        Seed rootSeed = generateSeed("masterpassphrase");
        std::string strRootSeed((char*)rootSeed.data(), rootSeed.size());
        std::string strSeed((char*)seed.data(), seed.size());
        if (strRootSeed != strSeed)
        {
            hEObj->SM2GenECCKeyPair();
            tempPublickey = hEObj->getPublicKey();
            tempPrivatekey = hEObj->getPrivateKey();
        }
        else
        {
            tempPublickey = hEObj->getRootPublicKey();
            tempPrivatekey = hEObj->getRootPrivateKey();
        }
		SecretKey secretkeyTemp(Slice(tempPrivatekey.first, tempPrivatekey.second));
		secretkeyTemp.keyTypeInt = hEObj->gmOutCard;
        return std::make_pair(PublicKey(Slice(tempPublickey.first, tempPublickey.second)),
			secretkeyTemp);
    }
    }
}

std::pair<PublicKey, SecretKey>
randomKeyPair (KeyType type)
{
    if (KeyType::gmalg == type)
    {
		const int randomCheckLen = 32; //256bit = 32 byte
		if (!GMCheck::getInstance()->randomSingleCheck(randomCheckLen))
		{
			LogicError("randomSingleCheck failed!");
		}
        HardEncrypt* hEObj = HardEncryptObj::getInstance();
        std::pair<unsigned char*, int> tempPublickey;
        std::pair<unsigned char*, int> tempPrivatekey;
        hEObj->SM2GenECCKeyPair();
        tempPublickey = hEObj->getPublicKey();
        tempPrivatekey = hEObj->getPrivateKey();
		SecretKey secretkeyTemp(Slice(tempPrivatekey.first, tempPrivatekey.second));
		secretkeyTemp.keyTypeInt = hEObj->gmOutCard;
        return std::make_pair(PublicKey(Slice(tempPublickey.first, tempPublickey.second)),
			secretkeyTemp);
    }
    else
    {
        auto const sk = randomSecretKey();
        return{ derivePublicKey(type, sk), sk };
    }
}

template <>
boost::optional<SecretKey>
parseBase58 (TokenType type, std::string const& s)
{
    auto const result = decodeBase58Token(s, type);
    if (result.empty())
        return boost::none;
    if (result.size() != 32)
        return boost::none;
    return SecretKey(makeSlice(result));
}

} // ripple
