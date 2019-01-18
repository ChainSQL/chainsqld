#ifndef CHAINSQL_APP_MISC_EXTVM_H_INCLUDED
#define CHAINSQL_APP_MISC_EXTVM_H_INCLUDED

#include <peersafe/app/misc/SleOps.h>
#include <ripple/app/tx/impl/ApplyContext.h>
#include <peersafe/vm/ExtVMFace.h>
#include <peersafe/basics/TypeTransform.h>

#include <functional>
#include <map>

namespace ripple
{

class EnvInfoImpl : public EnvInfo
{
public:
    EnvInfoImpl(int64_t iBlockNum, int64_t iGasLimit) : EnvInfo() 
    {
	iBlockNum_ = iBlockNum;
	iGasLimit_ = iGasLimit;
    }
       
	int64_t const gasLimit() const override {
        return iGasLimit_;
    }
    
	int64_t const block_number() const override{
        return iBlockNum_;
    }


	int64_t const block_timestamp() const override {
		return std::chrono::seconds(std::time(NULL)).count();
	}

    
private:
	int64_t                   iBlockNum_;
    int64_t                   iGasLimit_;
};

struct CallParametersR
{
	CallParametersR() = default;
	CallParametersR(
		AccountID _senderAddress,
		AccountID _codeAddress,
		AccountID _receiveAddress,
		uint256	  _valueTransfer,
		uint256   _apparentValue,
		int64_t _gas,
		bytesConstRef _data
	) : senderAddress(_senderAddress), codeAddress(_codeAddress), receiveAddress(_receiveAddress),
		valueTransfer(_valueTransfer), apparentValue(_apparentValue), gas(_gas), data(_data) {}
	CallParametersR(CallParameters const& p) :senderAddress(fromEvmC(p.senderAddress)),
		codeAddress(fromEvmC(p.codeAddress)),receiveAddress(fromEvmC(p.receiveAddress)),
		valueTransfer(fromEvmC(p.valueTransfer)), apparentValue(fromEvmC(p.apparentValue)),
		gas(p.gas),data(p.data)
	{
	}

	AccountID senderAddress;
	AccountID codeAddress;
	AccountID receiveAddress;
	uint256 valueTransfer;
	uint256 apparentValue;
	int64_t gas;
	bytesConstRef data;
	bool staticCall = false;
};

class ExtVM : public ExtVMFace
{
public:    
    ExtVM(SleOps& _s, EnvInfo const&_envInfo, AccountID const& _myAddress,
		AccountID const& _caller, AccountID const& _origin, uint256 _value, uint256 _gasPrice, bytesConstRef _data,
		bytesConstRef _code, uint256 const& _codeHash, int32_t _depth, bool _isCreate,  bool _staticCall)
      : ExtVMFace(_envInfo, toEvmC(_myAddress), toEvmC(_caller), toEvmC(_origin), toEvmC(_value),
		  toEvmC(_gasPrice), _data, _code.toBytes(), toEvmC(_codeHash), _depth, _isCreate, _staticCall),
		oSle_(_s)
    {
        // Contract: processing account must exist. In case of CALL, the ExtVM
        // is created only if an account has code (so exist). In case of CREATE
        // the account must be created first.
        
        //assert(m_s.addressInUse(_myAddress));
    }

	/// Read storage location.
    virtual evmc_uint256be store(evmc_uint256be const& key) override final;

	/// Write a value in storage.
	virtual void setStore(evmc_uint256be const& key, evmc_uint256be const& value) override final;

    /// Read address's balance.
    virtual evmc_uint256be balance(evmc_address const& addr) override final;

    /// Read address's code.
    virtual bytes const& codeAt(evmc_address const& addr)  override final;

    /// @returns the size of the code in bytes at the given address.
    virtual size_t codeSizeAt(evmc_address const& addr)  override final;

    /// Does the account exist?
    virtual bool exists(evmc_address const& addr) override final;

    /// Suicide the associated contract and give proceeds to the given address.
    virtual void suicide(evmc_address const& addr) override final;

    /// Hash of a block if within the last 256 blocks, or h256() otherwise.
    virtual evmc_uint256be blockHash(int64_t  const& iSeq) override final;

    /// Create a new (contract) account.
    virtual CreateResult create(evmc_uint256be const& endowment, int64_t& ioGas,
        bytesConstRef const& code, Instruction op, evmc_uint256be const& salt) override final;

    /// Make a new message call.
    virtual CallResult call(CallParameters&) override final;

    /// Revert any changes made (by any of the other calls).
    virtual void log(evmc_uint256be const* /*topics*/, size_t /*numTopics*/, bytesConstRef const& data) override final;

    virtual int64_t executeSQL(evmc_address const* _addr, uint8_t _type, bytesConstRef const& _name, bytesConstRef const& _raw) override final;

	//
	virtual int64_t table_create(const struct evmc_address* address, bytesConstRef const& _name, bytesConstRef const& _raw) override final;
	virtual int64_t table_rename(const struct evmc_address* address, bytesConstRef const& _name, bytesConstRef const& _raw) override final;
	virtual int64_t table_insert(const struct evmc_address* address, bytesConstRef const& _name, bytesConstRef const& _raw) override final;
	virtual int64_t table_delete(const struct evmc_address* address, bytesConstRef const& _name, bytesConstRef const& _raw) override final;
	virtual int64_t table_drop(const struct evmc_address* address, bytesConstRef const& _name) override final;
	virtual int64_t table_update(const struct evmc_address* address, bytesConstRef const& _name, bytesConstRef const& _rawUpdate, bytesConstRef const& _rawCondition) override final;
	virtual int64_t table_grant(const struct evmc_address* address1, const struct evmc_address* address2, bytesConstRef const& _name, bytesConstRef const& _raw) override final;
	virtual evmc_uint256be table_get_handle(const struct evmc_address* address, bytesConstRef const& _name, bytesConstRef const& _raw) override final;
	virtual evmc_uint256be table_get_lines(const struct evmc_uint256be *handle) override final;
	virtual evmc_uint256be table_get_columns(const struct evmc_uint256be *handle) override final;

	virtual
		size_t table_get_by_key(const evmc_uint256be *_handle,
			size_t _row, bytesConstRef const& _column,
			uint8_t *_outBuf, size_t _outSize) override final;
	virtual
		size_t table_get_by_index(const evmc_uint256be *_handle,
			size_t _row, size_t _column, uint8_t *_outBuf,
			size_t _outSize) override final;

	virtual void db_trans_begin() override final;
	virtual int64_t db_trans_submit() override final;
	virtual void release_resource() override final;

	//get field's value size
	virtual
		evmc_uint256be get_column_len(const evmc_uint256be *_handle,
			size_t _row, bytesConstRef const &_column) override final;
	virtual
		evmc_uint256be get_column_len(const evmc_uint256be *_handle,
			size_t _row, size_t _column) override final;
    
    SleOps const& state() const { return oSle_; }

	AccountID contractAddress() { return fromEvmC(myAddress); }
private:
	SleOps&                                                      oSle_;
    beast::Journal                                               journal_;
};

}

#endif
