#ifndef CHAINSQL_APP_MISC_SLEOPS_H_INCLUDED
#define CHAINSQL_APP_MISC_SLEOPS_H_INCLUDED

#include <ripple/protocol/AccountID.h>
#include <ripple/protocol/UintTypes.h>
#include <ripple/protocol/STAmount.h>

namespace ripple {

class SleOps
{
public:
    SleOps() {}

    STAmount balance(AccountID const& account, Currency const& currency, int &scale) const;

    STBlob code(AccountID const& account) const;
    size_t codeSize(AccountID const& account) const;

    bool setCode(AccountID const& account, STBlob const&blobCode) const;

    /// Get the value of a storage position of an account.
    /// @returns 0 if no account exists at that address.
    uint256 storage(AccountID const& account, uint256 const& key) const;

    /// Set the value of a storage position of an account.
    void setStorage(AccountID const& account, uint256 const& key, uint256 const& value);
};

}

#endif
