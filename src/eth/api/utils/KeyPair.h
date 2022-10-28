#ifndef ETH_API_UTIL_KEYPAIR_H_INCLUDED
#define ETH_API_UTIL_KEYPAIR_H_INCLUDED

#include <eth/tools/FixedHash.h>

namespace eth {
using Secret = SecureFixedHash<32>;

/// A public key: 64 bytes.
/// @NOTE This is not endian-specific; it's just a bunch of bytes.
using Public = h512;

using Address = h160;

/// Simple class that represents a "key pair".
/// All of the data of the class can be regenerated from the secret key
/// (m_secret) alone. Actually stores a tuplet of secret, public and address
/// (the right 160-bits of the public).
class KeyPair
{
public:
    /// Normal constructor - populates object from the given secret key.
    /// If the secret key is invalid the constructor succeeds, but public key
    /// and address stay "null".
    KeyPair(Secret const& _sec);

    /// Create a new, randomly generated object.
    static KeyPair
    create();

    Secret const&
    secret() const
    {
        return m_secret;
    }

    /// Retrieve the public key.
    Public const&
    pub() const
    {
        return m_public;
    }

    /// Retrieve the associated address of the public key.
    Address const&
    address() const
    {
        return m_address;
    }

    bool
    operator==(KeyPair const& _c) const
    {
        return m_public == _c.m_public;
    }
    bool
    operator!=(KeyPair const& _c) const
    {
        return m_public != _c.m_public;
    }

private:
    Secret m_secret;
    Public m_public;
    Address m_address;
};
}  // namespace eth
#endif