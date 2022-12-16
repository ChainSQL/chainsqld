#ifndef CHAINSQL_APP_MISC_EXTVM_H_INCLUDED
#define CHAINSQL_APP_MISC_EXTVM_H_INCLUDED

#include <peersafe/app/misc/SleOps.h>
#include <ripple/app/tx/impl/ApplyContext.h>
#include <eth/vm/ExtVMFace.h>
#include <peersafe/precompiled/PreContractFace.h>
#include <peersafe/basics/TypeTransform.h>

#include <functional>
#include <map>

namespace ripple
{

class EnvInfoImpl : public eth::EnvInfo
{
public:
    EnvInfoImpl(int64_t iBlockNum, int64_t iGasLimit, uint64_t iDropsPerByte,int64_t iGasUsed,int64_t iChainID, const PreContractFace& pPreContractFaceIn)
        : EnvInfo(),
        pPreContractFace(pPreContractFaceIn)
    {
		iBlockNum_     = iBlockNum;
		iGasLimit_     = iGasLimit;
		iDropsPerByte_ = iDropsPerByte;
        iGasUsed_	   = iGasUsed;
        iChainID_	   = iChainID;
    }
       
	int64_t gasLimit() const override {
        return iGasLimit_;
    }
    
	int64_t block_number() const override{
        return iBlockNum_;
    }

	int64_t block_timestamp() const override {
		return std::chrono::seconds(std::time(NULL)).count();
	}

	uint64_t dropsPerByte() const override {
		return iDropsPerByte_;
	}

    const PreContractFace& preContractFace() const override {
        return pPreContractFace;
    }

	int64_t
    gasUsed() const override
    {
        return iGasUsed_;
    }

    int64_t
    chainID() const override
    {
        return iChainID_;
    } 
    
private:
	int64_t                   iBlockNum_;
    int64_t                   iGasLimit_;
	uint64_t                  iDropsPerByte_;
    const PreContractFace&    pPreContractFace;
    int64_t					  iGasUsed_;
    int64_t					  iChainID_;
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
		eth::bytesConstRef _data,
		bool _staticCall = false
	) : senderAddress(_senderAddress), codeAddress(_codeAddress), receiveAddress(_receiveAddress),
		valueTransfer(_valueTransfer), apparentValue(_apparentValue), gas(_gas), data(_data), staticCall(_staticCall) {}

	CallParametersR(eth::CallParameters const& p) :senderAddress(fromEvmC(p.senderAddress)),
		codeAddress(fromEvmC(p.codeAddress)),receiveAddress(fromEvmC(p.receiveAddress)),
		valueTransfer(fromEvmC(p.valueTransfer)), apparentValue(fromEvmC(p.apparentValue)),
		gas(p.gas),data(p.data),staticCall(p.staticCall)
	{
	}

	AccountID senderAddress;
	AccountID codeAddress;
	AccountID receiveAddress;
	uint256 valueTransfer;
	uint256 apparentValue;
	int64_t gas;
	eth::bytesConstRef data;
	bool staticCall = false;
};

class ExtVM : public eth::ExtVMFace
{
public:    
    ExtVM(SleOps& _s, eth::EnvInfo const&_envInfo, AccountID const& _myAddress,
		AccountID const& _caller, AccountID const& _origin, uint256 _value, uint256 _gasPrice, eth::bytesConstRef _data,
		eth::bytesConstRef _code, uint256 const& _codeHash, int32_t _depth, bool _isCreate,  bool _staticCall)
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
    virtual eth::bytes const& codeAt(evmc_address const& addr)  override final;

    /// @returns the size of the code in bytes at the given address.
    virtual size_t codeSizeAt(evmc_address const& addr)  override final;

	/// @returns the hash of the code at the given address.
	virtual evmc_uint256be codeHashAt(evmc_address const& addr) override final;

    /// Does the account exist?
    virtual bool exists(evmc_address const& addr) override final;

    /// Suicide the associated contract and give proceeds to the given address.
    virtual void selfdestruct(evmc_address const& addr) override final;

    /// Hash of a block if within the last 256 blocks, or h256() otherwise.
    virtual evmc_uint256be blockHash(int64_t  const& iSeq) override final;

    /// Create a new (contract) account.
    virtual eth::CreateResult create(evmc_uint256be const& endowment, int64_t& ioGas,
		eth::bytesConstRef const& code, eth::Instruction op, evmc_uint256be const& salt) override final;

    /// Make a new message call.
    virtual eth::CallResult call(eth::CallParameters&) override final;

    /// Revert any changes made (by any of the other calls).
    virtual void log(evmc_uint256be const* /*topics*/, size_t /*numTopics*/, eth::bytesConstRef const& data) override final;

    virtual int64_t executeSQL(evmc_address const* _addr, uint8_t _type, eth::bytesConstRef const& _name, eth::bytesConstRef const& _raw) override final;

	//
	virtual int64_t table_create(const struct evmc_address* address, eth::bytesConstRef const& _name, eth::bytesConstRef const& _raw) override final;
	virtual int64_t table_rename(const struct evmc_address* address, eth::bytesConstRef const& _name, eth::bytesConstRef const& _raw) override final;
	virtual int64_t table_insert(const struct evmc_address* address, eth::bytesConstRef const& _name, eth::bytesConstRef const& _raw) override final;
	virtual int64_t table_delete(const struct evmc_address* address, eth::bytesConstRef const& _name, eth::bytesConstRef const& _raw) override final;
	virtual int64_t table_drop(const struct evmc_address* address, eth::bytesConstRef const& _name) override final;
	virtual int64_t table_update(const struct evmc_address* address, eth::bytesConstRef const& _name, eth::bytesConstRef const& _rawUpdate, eth::bytesConstRef const& _rawCondition) override final;
	virtual int64_t table_grant(const struct evmc_address* address1, const struct evmc_address* address2, eth::bytesConstRef const& _name, eth::bytesConstRef const& _raw) override final;
	virtual evmc_uint256be table_get_handle(const struct evmc_address* address, eth::bytesConstRef const& _name, eth::bytesConstRef const& _raw) override final;
	virtual evmc_uint256be table_get_lines(const struct evmc_uint256be *handle) override final;
	virtual evmc_uint256be table_get_columns(const struct evmc_uint256be *handle) override final;

	virtual
		size_t table_get_by_key(const evmc_uint256be *_handle,
			size_t _row, eth::bytesConstRef const& _column,
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
			size_t _row, eth::bytesConstRef const &_column) override final;
	virtual
		evmc_uint256be get_column_len(const evmc_uint256be *_handle,
			size_t _row, size_t _column) override final;

    virtual int64_t account_set(const struct evmc_address* address,
        uint32_t _uflag, bool _bset) override final;
    virtual int64_t transfer_fee_set(const struct evmc_address *address,
		eth::bytesConstRef const& _Rate, eth::bytesConstRef const& _Min, eth::bytesConstRef const& _Max) override final;
    virtual int64_t trust_set(const struct evmc_address *address,
		eth::bytesConstRef const& _value, eth::bytesConstRef const& _currency,
        const struct evmc_address *gateWay) override final;
    virtual int64_t trust_limit(const struct evmc_address *address, 
		eth::bytesConstRef const& _currency,
        uint64_t _power, 
        const struct evmc_address *gateWay) override final;
    virtual int64_t gateway_balance(const struct evmc_address *address,
		eth::bytesConstRef const& _currency,
        uint64_t _power, 
        const struct evmc_address *gateWay) override final;
    virtual int64_t pay(const struct evmc_address *address,
        const struct evmc_address *receiver, 
		eth::bytesConstRef const& _value, eth::bytesConstRef const& _sendMax, eth::bytesConstRef const&  _currency,
        const struct evmc_address *gateWay) override final;
    
    SleOps const& state() const { return oSle_; }

	AccountID contractAddress() { return fromEvmC(myAddress); }
public:
	ripple::TER check_address_invalidation(const struct evmc_address* address);
private:
	SleOps&                                                      oSle_;
};

}

#endif
