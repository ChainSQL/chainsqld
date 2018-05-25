#ifndef RIPPLE_APP_MISC_EXTVM_H_INCLUDED
#define RIPPLE_APP_MISC_EXTVM_H_INCLUDED

#include <peersafe/app/misc/SleOps.h>
#include <ripple/app/tx/impl/ApplyContext.h>
#include <functional>
#include <map>

namespace ripple
{

class ExtVMFace;
class EnvInfo;

/// Externality interface for the Virtual Machine providing access to world state.
class ExtVM : public ExtVMFace
{
public:
    /// Full constructor.
    ExtVM(SleOps& _s, EnvInfo const& _envInfo, Address _myAddress,
        Address _caller, Address _origin, u256 _value, u256 _gasPrice, bytesConstRef _data,
        bytesConstRef _code, h256 const& _codeHash, unsigned _depth, bool _isCreate,
        bool _staticCall)
      : ExtVMFace(_envInfo, _myAddress, _caller, _origin, _value, _gasPrice, _data, _code.toBytes(),
            _codeHash, _depth, _isCreate, _staticCall),
        m_s(_s)
    {
        // Contract: processing account must exist. In case of CALL, the ExtVM
        // is created only if an account has code (so exist). In case of CREATE
        // the account must be created first.
        assert(m_s.addressInUse(_myAddress));
    }

	/// Read storage location.
    virtual u256 store(u256 _n) override final;

	/// Write a value in storage.
	virtual void setStore(u256 _n, u256 _v) override final;

	/// Read address's code.
	virtual bytes const& codeAt(Address _a) override final { return m_s.code(_a); }

	/// @returns the size of the code in  bytes at the given address.
	virtual size_t codeSizeAt(Address _a) override final;

	/// Create a new contract.
	CreateResult create(u256 _endowment, u256& io_gas, bytesConstRef _code, Instruction _op, u256 _salt, OnOpFunc const& _onOp = {}) final;

	/// Create a new message call.
	CallResult call(CallParameters& _params) final;

	/// Read address's balance.
	virtual u256 balance(Address _a) override final { return m_s.balance(_a); }

	/// Does the account exist?
	virtual bool exists(Address _a) override final
	{
		if (evmSchedule().emptinessIsNonexistence())
			return m_s.accountNonemptyAndExisting(_a);
		else
			return m_s.addressInUse(_a);
	}

	/// Suicide the associated contract to the given address.
	virtual void suicide(Address _a) override final;

	/// Return the EVM gas-price schedule for this execution context.
	virtual EVMSchedule const& evmSchedule() const override final { return m_sealEngine.evmSchedule(envInfo().number()); }

	State const& state() const { return m_s; }

	/// Hash of a block if within the last 256 blocks, or h256() otherwise.
	h256 blockHash(u256 _number) override;

private:
	SleOps& m_s;  ///< A reference to the sleOp	
};

}

#endif