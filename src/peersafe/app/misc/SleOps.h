#ifndef CHAINSQL_APP_MISC_SLEOPS_H_INCLUDED
#define CHAINSQL_APP_MISC_SLEOPS_H_INCLUDED

#include <ripple/protocol/AccountID.h>
#include <ripple/protocol/UintTypes.h>
#include <ripple/protocol/STAmount.h>
#include <ripple/app/tx/impl/ApplyContext.h>
#include <ripple/basics/TaggedCache.h>
#include <peersafe/basics/TypeTransform.h>

namespace ripple {
enum ContractOpType {
	ContractCreation	= 1,			///< Transaction to create contracts - receiveAddress() is ignored.
	MessageCall			= 2,			///< Transaction to invoke a message call - receiveAddress() is used.
	ContractDeletion	= 3,				///
	LocalMessageCall    = 4
};
class SleOps
{
public:
    SleOps(ApplyContext& ctx);

	ApplyContext& ctx() { return ctx_; }

    SLE::pointer getSle(AccountID const & addr) const;

	/// Increament the account nonce.
	void incNonce(AccountID const& addr);
	/// Get the account nonce -- the number of transactions it has sent.
	/// @returns 0 if the address has never been used.
	uint32 getNonce(AccountID const& addr);

	uint32 requireAccountStartNonce() { return 1; }
	/// Set the account nonce.
	void setNonce(AccountID const& _addr, uint32 const& _newNonce);

	bool addressHasCode(AccountID const& addr);
	/// Sets the code of the account. Must only be called during / after contract creation.
	void setCode(AccountID const& _address, bytes&& _code);
	/// Get the code of an account.
	/// @returns bytes() if no account exists at that address.
	/// @warning The reference to the code is only valid until the access to
	///          other account. Do not keep it.
	bytes const& code(AccountID const& _addr);
	/// Get the code hash of an account.
	/// @returns EmptySHA3 if no account exists at that address or if there is no code associated with the address.
	uint256 codeHash(AccountID const& _contract);

	TER transferBalance(AccountID const& _from, AccountID const& _to, uint256 const& _value);

	TER doPayment(AccountID const& _from, AccountID const& _to, uint256 const& _value);

	//contract account can have zero zxc and exist on chainsql
	bool createContractAccount(AccountID const& _from, AccountID const& _to, uint256 const& _value);

	/// Clear the storage root hash of an account to the hash of the empty trie.
	void clearStorage(AccountID const& _contract);

	/// Add some amount to balance.
	/// Will initialise the address if it has never been used.
	void addBalance(AccountID const& _id, int64_t const& _amount);

	/// Subtract the @p _value amount from the balance of @p _addr account.
	/// @throws NotEnoughCash if the balance of the account is less than the
	/// amount to be subtrackted (also in case the account does not exist).
	TER subBalance(AccountID const& _addr, int64_t const& _value);

	int64_t balance(AccountID const& address);

	AccountID calcNewAddress(AccountID sender, int nonce);

    void PubContractEvents(const AccountID& contractID, uint256 const * aTopic, int iTopicNum, const Blob& byValue);

    void kill(AccountID sender);
private:
    ApplyContext &ctx_;
    TaggedCache <AccountID, Blob>             contractCacheCode_;
};

}

#endif
