#ifndef __H_CHAINSQL_VM_EXECUTIVE_H__
#define __H_CHAINSQL_VM_EXECUTIVE_H__

#include <peersafe/app/misc/SleOps.h>
#include <peersafe/app/misc/ExtVM.h>

namespace ripple {
	/**
	* @brief Message-call/contract-creation executor; useful for executing transactions.
	*
	* Two ways of using this class - either as a transaction executive or a CALL/CREATE executive.
	*
	* In the first use, after construction, begin with initialize(), then execute() and end with finalize(). Call go()
	* after execute() only if it returns false.
	*
	* In the second use, after construction, begin with call() or create() and end with
	* accrueSubState(). Call go() after call()/create() only if it returns false.
	*
	* Example:
	* @code
	* Executive e(state, blockchain, 0);
	* e.initialize(transaction);
	* if (!e.execute())
	*    e.go();
	* e.finalize();
	* @endcode
	*/
class STTx;
class Executive {
public:
	// Simple constructor; executive will operate on given state, with the given environment info.
	Executive(SleOps & _s, EnvInfo const& _envInfo, unsigned int _level);

	//No default constructor
	Executive() = delete;

	//Cannot copy from another object
	Executive(Executive const&) = delete;
	void operator=(Executive) = delete;

	/// Initializes the executive for evaluating a transaction. You must call finalize() at some point following this.
	//void initialize(BlobRef _transaction) { initialize(STTx(_transaction, CheckTransaction::None)); }
	void initialize();

	// initialize gas price depending on network load
	void initGasPrice();
	/// Finalise a transaction previously set up with initialize().
	/// @warning Only valid after initialize() and execute(), and possibly go().
	/// @returns true if the outermost execution halted normally, false if exceptionally halted.
	TER finalize();

	/// Begins execution of a transaction. You must call finalize() following this.
	/// @returns true if the transaction is done, false if go() must be called.
	bool execute();

	/// @returns the transaction from initialize().
	/// @warning Only valid after initialize().
	STTx const& t() const { return m_s.ctx().tx; }

	/// Set up the executive for evaluating a bare CREATE (contract-creation) operation.
	/// @returns false iff go() must be called (and thus a VM execution in required).
	bool create(AccountID const& _txSender, uint256 const& _endowment,
		uint256 const& _gasPrice, int64_t const& _gas, bytesConstRef const& _code, AccountID const& _originAddress);

	/// @returns false iff go() must be called (and thus a VM execution in required).
	bool createOpcode(AccountID const& _sender, uint256 const& _endowment,
		uint256 const& _gasPrice, int64_t const& _gas, bytesConstRef const& _code, AccountID const& _originAddress);

	/// Set up the executive for evaluating a bare CALL (message call) operation.
	/// @returns false if go() must be called (and thus a VM execution in required).
	bool call(AccountID const& _receiveAddress, AccountID const& _txSender, 
		uint256 const& _txValue, uint256 const& _gasPrice, bytesConstRef const& _txData, int64_t const& _gas);
	bool call(CallParametersR const& _cp, uint256 const& _gasPrice, AccountID const& _origin);

    void accrueSubState(SubState& _parentContext);

	/// Executes (or continues execution of) the VM.
	/// @returns false iff go() must be called again to finish the transaction.
	//bool go(OnOpFunc const& _onOp = OnOpFunc());
	bool go();

	/// @returns the new address for the created contract in the CREATE operation.
	AccountID newAddress() const { return m_newAddress; }

	///// Revert all changes made to the state by this execution.
	//void revert();

	/// @returns gas remaining after the transaction/operation. Valid after the transaction has been executed.
	int64_t gas() const { return m_gas; }
	/// @returns total gas used in the transaction/operation.
	/// @warning Only valid after finalise().
	int64_t gasUsed() const;

	owning_bytes_ref takeOutput() { return std::move(m_output); }

	/// @returns The exception that has happened during the execution if any.
	TER getException() const noexcept { return m_excepted; }
private:
	/// @returns false if go() must be called (and thus a VM execution in required).
	bool executeCreate(AccountID const& _txSender, uint256 const& _endowment,
		uint256 const& _gasPrice, int64_t const& _gas, bytesConstRef const& _code, AccountID const& _originAddress);

	beast::Journal getJ();
	void formatOutput(std::string msg);
	void formatOutput(owning_bytes_ref output);
private:
	SleOps& m_s;						///< The state to which this operation/transaction is applied.
										
	EnvInfo const& m_envInfo;					///< Information on the runtime environment.
	std::shared_ptr<ExtVM> m_ext;		///< The VM externality object for the VM execution or null if no VM is required. shared_ptr used only to allow ExtVM forward reference. This field does *NOT* survive this object.
	owning_bytes_ref m_output;			///< Execution output.
	bytes m_input;						///< Execution input.
	//ExecutionResult* m_res = nullptr;	///< Optional storage for execution results.

	unsigned m_depth = 0;				///< The context's call-depth.
	TER m_excepted = tesSUCCESS;		///< Details if the VM's execution resulted in an exception.
	int64_t m_refunded = 0;		        ///< The amount of gas refunded.
    int64_t m_baseGasRequired = 0;		///< The base amount of gas requried for executing this transaction.
	int64_t m_gas = 0;					///< gas remained
	//uint256 m_refunded = beast::zero;	///< The amount of gas refunded.

	//std::shared_ptr<const STTx> m_t;        ///< The original transaction.
	int64_t m_gasCost;
	uint32 m_gasPrice;

	bool m_isCreation = false;
	AccountID m_newAddress;
};
} // namespace ripple

#endif // !__H_CHAINSQL_VM_EXECUTIVE_H__
