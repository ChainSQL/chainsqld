#ifndef CHAINSQL_APP_MISC_SLEOPS_H_INCLUDED
#define CHAINSQL_APP_MISC_SLEOPS_H_INCLUDED

#include <ripple/protocol/AccountID.h>
#include <ripple/protocol/UintTypes.h>
#include <ripple/protocol/STAmount.h>
#include <ripple/app/tx/impl/ApplyContext.h>
#include <ripple/basics/TaggedCache.h>
#include <peersafe/basics/TypeTransform.h>
#include <peersafe/protocol/TableDefines.h>

namespace ripple {

class SleOps
{
public:
    SleOps(ApplyContext& ctx);
	// Release resources in destructor
	~SleOps();

	ApplyContext& ctx() { return ctx_; }

    SLE::pointer getSle(AccountID const & addr) const;

	/// Increament the account sequence.
	void incSequence(AccountID const& addr);
	/// Get the account sequence -- the number of transactions it has sent.
	/// @returns 0 if the address has never been used.
	uint32 getSequence(AccountID const& addr);

	const STTx& getTx();

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
	TER createContractAccount(AccountID const& _from, AccountID const& _to, uint256 const& _value);

	/// Clear the storage root hash of an account to the hash of the empty trie.
	void clearStorage(AccountID const& _contract);

	/// Add some amount to balance.
	/// Will initialise the address if it has never been used.
	void addBalance(AccountID const& _id, int64_t const& _amount);

	/// Subtract the @p _value amount from the balance of @p _addr account.
	/// @throws NotEnoughCash if the balance of the account is less than the
	/// amount to be subtrackted (also in case the account does not exist).
	TER subBalance(AccountID const& _addr, int64_t const& _value);

	int64_t disposeTableTx(STTx tx, AccountID const& _account, std::string _sTableName, std::string _tableNewName = "", bool bNewNameInDB = false);

	//db operators
	int64_t executeSQL(AccountID const& _account, AccountID const& _owner, TableOpType _iType, std::string _sTableName, std::string _sRaw);

	//table opeartion
	int64_t createTable(AccountID const& _account, std::string const& _sTableName, std::string const& _raw);
	int64_t dropTable(AccountID const& _account, std::string const& _sTableName);
	int64_t renameTable(AccountID const& _account, std::string const& _sTableName, std::string const& _sTableNewName);
	int64_t grantTable(AccountID const& _account, AccountID const& _account2, std::string const& _sTableName, std::string const& _raw);

	//CRUD operation
	int64_t insertData(AccountID const& _account, AccountID const& _owner, std::string const& _sTableName, std::string const& _raw);
	int64_t deleteData(AccountID const& _account, AccountID const& _owner, std::string const& _sTableName, std::string const& _raw);
	int64_t updateData(AccountID const& _account, AccountID const& _owner, std::string const& _sTableName, std::string const& _getRaw, std::string const& _updateRaw);

	//Select related
	uint256 getDataHandle(AccountID const& _account, AccountID const& _owner, std::string const& _sTableName, std::string const& _raw);
	uint256 getDataRowCount(uint256 const& _handle);
	uint256 getDataColumnCount(uint256 const& _handle);
	std::string	getByKey(uint256 const& _handle, size_t row, std::string const& _key);
	std::string	getByIndex(uint256 const& handle, size_t row, size_t column);
	void	releaseResource();

	//transaction related
	void	transactionBegin();
	int64_t	transactionCommit(AccountID const & _account, bool _bNeedVerify = true);
	void	resetTransactionCache();

	static void	addCommonFields(STObject& obj, AccountID const& _account);
	std::pair<bool,STArray>
			genTableFields(const ApplyContext &_ctx, AccountID const& _account,std::string _sTablename,std::string _tableNewName,bool bNewNameInDB);

	int64_t balance(AccountID const& address);

    void PubContractEvents(const AccountID& contractID, uint256 const * aTopic, int iTopicNum, const Blob& byValue);

    void kill(AccountID sender);
private:
    ApplyContext &ctx_;
	bool									  bTransaction_;
	std::map <AccountID, Blob>				  contractCacheCode_;
	std::vector<STTx>						  sqlTxsStatements_;
	std::vector<uint256>					  handleList_;
	std::map<std::string, uint160>			  sqlTxsNameInDB_;
};

}

#endif
