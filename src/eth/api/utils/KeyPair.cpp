
#include <secp256k1/include/secp256k1.h>
#include <eth/api/utils/KeyPair.h>
#include <eth/vm/utils/keccak.h>

namespace ripple {
    extern secp256k1_context const* getCtx();
}

namespace eth {



template <std::size_t KeySize>
bool
toPublicKey(
    Secret const& _secret,
    unsigned _flags,
    std::array<byte, KeySize>& o_serializedPubkey)
{
    auto* ctx = ripple::getCtx();
    secp256k1_pubkey rawPubkey;
    // Creation will fail if the secret key is invalid.
    if (!secp256k1_ec_pubkey_create(ctx, &rawPubkey, _secret.data()))
        return false;
    size_t serializedPubkeySize = o_serializedPubkey.size();
    secp256k1_ec_pubkey_serialize(
        ctx,
        o_serializedPubkey.data(),
        &serializedPubkeySize,
        &rawPubkey,
        _flags);
    assert(serializedPubkeySize == o_serializedPubkey.size());
    return true;
}

Public
toPublic(Secret const& _secret)
{
    std::array<byte, 65> serializedPubkey;
    if (!toPublicKey(_secret, SECP256K1_EC_UNCOMPRESSED, serializedPubkey))
        return {};

    // Expect single byte header of value 0x04 -- uncompressed public key.
    assert(serializedPubkey[0] == 0x04);

    // Create the Public skipping the header.
    return Public{&serializedPubkey[1], Public::ConstructFromPointer};
}

Address
toAddress(Public const& _public)
{
    h256 hash;
    sha3(_public.data(), _public.size, hash.data());
    return right160(hash);
}


KeyPair::KeyPair(Secret const& _sec) : 
    m_secret(_sec), 
    m_public(toPublic(_sec))
{
    // Assign address only if the secret key is valid.
    if (m_public)
        m_address = toAddress(m_public);
}

KeyPair
KeyPair::create()
{
    while (true)
    {
        KeyPair keyPair(Secret::random());
        if (keyPair.address())
            return keyPair;
    }
}

}