#ifndef __H_CHAINSQL_VM_EXTVMFACE_H__
#define __H_CHAINSQL_VM_EXTVMFACE_H__

#include "Common.h"
#include <peersafe/basics/TypeTransform.h>

namespace ripple {

/// Reference to a slice of buffer that also owns the buffer.
///
/// This is extension to the concept C++ STL library names as array_view
/// (also known as gsl::span, array_ref, here vector_ref) -- reference to
/// continuous non-modifiable memory. The extension makes the object also owning
/// the referenced buffer.
///
/// This type is used by VMs to return output coming from RETURN instruction.
/// To avoid memory copy, a VM returns its whole memory + the information what
/// part of this memory is actually the output. This simplifies the VM design,
/// because there are multiple options how the output will be used (can be
/// ignored, part of it copied, or all of it copied). The decision what to do
/// with it was moved out of VM interface making VMs "stateless".
///
/// The type is movable, but not copyable. Default constructor available.
class owning_bytes_ref : public vector_ref<byte const>
{
public:
	owning_bytes_ref() = default;

	/// @param _bytes  The buffer.
	/// @param _begin  The index of the first referenced byte.
	/// @param _size   The number of referenced bytes.
	owning_bytes_ref(bytes&& _bytes, size_t _begin, size_t _size) :
		m_bytes(std::move(_bytes))
	{
		if (m_bytes.empty())
			return;
		// Set the reference *after* the buffer is moved to avoid
		// pointer invalidation.
		retarget(&m_bytes[_begin], _size);
	}

	owning_bytes_ref(owning_bytes_ref const&) = delete;
	owning_bytes_ref(owning_bytes_ref&&) = default;
	owning_bytes_ref& operator=(owning_bytes_ref const&) = delete;
	owning_bytes_ref& operator=(owning_bytes_ref&&) = default;

	/// Moves the bytes vector out of here. The object cannot be used any more.
	bytes&& takeBytes()
	{
		reset();  // Reset reference just in case.
		return std::move(m_bytes);
	}

private:
	bytes m_bytes;
};

struct SubState
{
    std::set<AccountID> suicides;			 ///< Any accounts that have suicided.
    uint64_t refunds = 0;                    ///< Refund counter of SSTORE nonzero->zero.

    SubState& operator+=(SubState const& _s)
    {
        for (auto it = suicides.begin(); it != suicides.end(); ++it)
            suicides.emplace(*it);        
        refunds += _s.refunds;        
        return *this;
    }

    void clear()
    {
        suicides.clear();        
        refunds = 0;
    }
};

struct CallParameters
{
	CallParameters() = default;
	CallParameters(
		evmc_address _senderAddress,
		evmc_address _codeAddress,
		evmc_address _receiveAddress,
		evmc_uint256be _valueTransfer,
		evmc_uint256be _apparentValue,
		int64_t _gas,
		bytesConstRef _data
	) : senderAddress(_senderAddress), codeAddress(_codeAddress), receiveAddress(_receiveAddress),
		valueTransfer(_valueTransfer), apparentValue(_apparentValue), gas(_gas), data(_data) {}
	evmc_address senderAddress;
	evmc_address codeAddress;
	evmc_address receiveAddress;
	evmc_uint256be valueTransfer;
	evmc_uint256be apparentValue;
	int64_t gas;
	bytesConstRef data;
	bool staticCall = false;
};

/// Represents a call result.
///
/// @todo: Replace with evmc_result in future.
struct CallResult
{
	evmc_status_code status;
	owning_bytes_ref output;

	CallResult(evmc_status_code status, owning_bytes_ref&& output)
		: status{ status }, output{ std::move(output) }
	{}
};

/// Represents a CREATE result.
///
/// @todo: Replace with evmc_result in future.
struct CreateResult
{
	evmc_status_code status;
	owning_bytes_ref output;
	evmc_address address; //h160 address

	CreateResult(evmc_status_code status, owning_bytes_ref&& output, evmc_address const& address)
		: status{ status }, output{ std::move(output) }, address(address)
	{}
};

class EnvInfo {
public:
	EnvInfo() = default;
	virtual ~EnvInfo() = default;

	virtual evmc_address const coin_base() const {        
		return evmc_address();
	}

	virtual int64_t const gasLimit() const {
		return 0;
	}

	virtual int64_t const block_number() const {
		return 0;
	}

	virtual int64_t const block_timestamp() const {
		return 0;
	}
};

class ExtVMFace : public evmc_context {
public:
	ExtVMFace(EnvInfo const& envInfo, evmc_address _myAddress, evmc_address _caller, evmc_address _origin,
		evmc_uint256be _value, evmc_uint256be _gasPrice, 
		bytesConstRef _data, bytes _code, evmc_uint256be _codeHash, int32_t _depth,
		bool _isCreate, bool _staticCall);
	virtual ~ExtVMFace() = default;

	ExtVMFace(ExtVMFace const&) = delete;
	ExtVMFace& operator=(ExtVMFace const&) = delete;

	/// Read storage location.
	virtual evmc_uint256be store(evmc_uint256be const&) { return evmc_uint256be(); }

	/// Write a value in storage.
	virtual void setStore(evmc_uint256be const&, evmc_uint256be const&) {}

	/// Read address's balance.
	virtual evmc_uint256be balance(evmc_address const&) { return evmc_uint256be(); }

	/// Read address's code.
	virtual bytes const& codeAt(evmc_address const&) { return NullBytes; }

	/// @returns the size of the code in bytes at the given address.
	virtual size_t codeSizeAt(evmc_address const&) { return 0; }

	/// Does the account exist?
	virtual bool exists(evmc_address const&) { return false; }

	/// Suicide the associated contract and give proceeds to the given address.
	virtual void suicide(evmc_address const&) { sub.suicides.insert(fromEvmC(myAddress)); }

	/// Create a new (contract) account.
	virtual CreateResult create(evmc_uint256be const&, int64_t&, 
		bytesConstRef const&, Instruction, evmc_uint256be const&) = 0;

	/// Make a new message call.
	virtual CallResult call(CallParameters&) = 0;

	/// Revert any changes made (by any of the other calls).
	virtual void log(evmc_uint256be const* topics, size_t numTopics, bytesConstRef const& _data) {  }

	/// Hash of a block if within the last 256 blocks, or h256() otherwise.
	virtual evmc_uint256be blockHash(int64_t  const&_number) = 0;
    
    /// DB operator
    virtual int64_t executeSQL(evmc_address const* _addr, uint8_t _type, bytesConstRef const& _name, bytesConstRef const& _raw) = 0;

    virtual int64_t table_create(const struct evmc_address* address, bytesConstRef const& _name, bytesConstRef const& _raw) { return 0; }
    virtual int64_t table_rename(const struct evmc_address* address, bytesConstRef const& _name, bytesConstRef const& _raw) { return 0; }
    virtual int64_t table_insert(const struct evmc_address* address, bytesConstRef const& _name, bytesConstRef const& _raw) { return 0; }
    virtual int64_t table_delete(const struct evmc_address* address, bytesConstRef const& _name, bytesConstRef const& _raw) { return 0; }
    virtual int64_t table_drop(const struct evmc_address* address, bytesConstRef const& _name) { return 0; }
    virtual int64_t table_update(const struct evmc_address* address, bytesConstRef const& _name, bytesConstRef const& _raw1, bytesConstRef const& _raw2) { return 0; }
    virtual int64_t table_grant(const struct evmc_address* address1, const struct evmc_address* address2, bytesConstRef const& _name, bytesConstRef const& _raw) { return 0; }
    virtual evmc_uint256be table_get_handle(const struct evmc_address* address, bytesConstRef const& _name, bytesConstRef const& _raw) { return evmc_uint256be(); }
    virtual evmc_uint256be table_get_lines(const struct evmc_uint256be *handle) { return evmc_uint256be(); }
    virtual evmc_uint256be table_get_columns(const struct evmc_uint256be *handle) { return evmc_uint256be(); }

    virtual 
    size_t table_get_by_key(const evmc_uint256be *_handle, 
            size_t _row, bytesConstRef const& _column, 
            uint8_t *_outBuf, size_t _outSize) { return 0; }
    virtual 
    size_t table_get_by_index(const evmc_uint256be *_handle, 
            size_t _row, size_t _column, uint8_t *_outBuf, 
            size_t _outSize) { return 0; }

    virtual void db_trans_begin() {}
    virtual int64_t db_trans_submit() { return 0; }
    virtual void release_resource() {}

    virtual 
    evmc_uint256be get_column_len(const evmc_uint256be *_handle, 
            size_t _row, bytesConstRef const &_column) {
        return evmc_uint256be();
    }
    virtual 
    evmc_uint256be get_column_len(const evmc_uint256be *_handle, 
            size_t _row, size_t _column) {
        return evmc_uint256be();
    }

	/// Get the execution environment information.
	EnvInfo const& envInfo() const { return envInfo_; }

	evmc_address myAddress;
	evmc_address caller;
	evmc_address origin;
	evmc_uint256be value;
	evmc_uint256be gasPrice;
	bytesConstRef data;
    SubState sub;             ///< Sub-band VM state (suicides, refund counter, logs).
	bytes code;
	evmc_uint256be codeHash;
	int32_t depth;
	bool isCreate = false;
	bool staticCall = false;

private:
	EnvInfo const& envInfo_;
};

} // namespace ripple

#endif // !__H_CHAINSQL_VM_EXTVMFACE_H__
