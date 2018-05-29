#ifndef CHAINSQL_APP_MISC_SLEOPS_H_INCLUDED
#define CHAINSQL_APP_MISC_SLEOPS_H_INCLUDED

#include <ripple/protocol/AccountID.h>
#include <ripple/protocol/UintTypes.h>
#include <ripple/protocol/STAmount.h>
#include <ripple/app/tx/impl/ApplyContext.h>
#include <peersafe/app/misc/SleOps.h>
#include <peersafe/app/misc/TypeTransform.h>


namespace ripple {

class SleOps
{
public:
    SleOps(ApplyContext& ctx):ctx_(ctx) {}

	const ApplyContext& ctx() { return ctx_; }

    SLE::pointer getSle(evmc_address const & addr) const;

	/// Increament the account nonce.
	void incNonce(evmc_address const& addr);
	/// Get the account nonce -- the number of transactions it has sent.
	/// @returns 0 if the address has never been used.
	uint32 getNonce(evmc_address const& addr);

	uint32 requireAccountStartNonce() { return 1; }
	/// Set the account nonce.
	void setNonce(evmc_address const& _addr, uint32 const& _newNonce);

	bool addressHasCode(evmc_address const& addr);
	/// Sets the code of the account. Must only be called during / after contract creation.
	void setCode(evmc_address const& _address, bytes&& _code);
	/// Get the code of an account.
	/// @returns bytes() if no account exists at that address.
	/// @warning The reference to the code is only valid until the access to
	///          other account. Do not keep it.
	bytes const& code(evmc_address const& _addr) const;
	/// Get the code hash of an account.
	/// @returns EmptySHA3 if no account exists at that address or if there is no code associated with the address.
	uint256 codeHash(evmc_address const& _contract) const;

	void transferBalance(evmc_address const& _from, evmc_address const& _to, uint256 const& _value);

	/// Clear the storage root hash of an account to the hash of the empty trie.
	void clearStorage(evmc_address const& _contract);
private:
    ApplyContext &ctx_;
};

}

#endif
