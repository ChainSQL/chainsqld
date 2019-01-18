#include <assert.h>
#include <iostream>
#include <cstdlib>

#include "ExtVMFace.h"
#include "Common.h"
#include <peersafe/core/Tuning.h>

namespace ripple {

int accountExists(evmc_context* _context, evmc_address const* _addr) noexcept
{
	auto& env = static_cast<ExtVMFace&>(*_context);
	return env.exists(*_addr) ? 1 : 0;
}

void getStorage(
	evmc_uint256be* o_result,
	evmc_context* _context,
	evmc_address const* _addr,
	evmc_uint256be const* _key
) noexcept
{
	(void)_addr;
	auto& env = static_cast<ExtVMFace&>(*_context);
	*o_result = env.store(*_key);
}

void setStorage(
	evmc_context* _context,
	evmc_address const* _addr,
	evmc_uint256be const* _key,
	evmc_uint256be const* _value
) noexcept
{
	(void)_addr;
	auto& env = static_cast<ExtVMFace&>(*_context);
        
    uint256 uNewValue = fromEvmC(*_value);
    uint256 uOldValue = fromEvmC(env.store(*_key));

    if (uNewValue == 0 && uOldValue != 0)       // If delete
        env.sub.refunds += STORE_REFUND_GAS;    // Increase refund counter

	env.setStore(*_key, *_value);
}

void getBalance(
	evmc_uint256be* o_result,
	evmc_context* _context,
	evmc_address const* _addr
) noexcept
{
	auto& env = static_cast<ExtVMFace&>(*_context);
	*o_result = env.balance(*_addr);
}

size_t getCodeSize(evmc_context* _context, evmc_address const* _addr)
{
	auto& env = static_cast<ExtVMFace&>(*_context);
	return env.codeSizeAt(*_addr);
}

size_t copyCode(evmc_context* _context, evmc_address const* _addr, size_t _codeOffset,
	byte* _bufferData, size_t _bufferSize)
{
	auto& env = static_cast<ExtVMFace&>(*_context);
	bytes const& code = env.codeAt(*_addr);

	// Handle "big offset" edge case.
	if (_codeOffset >= code.size())
		return 0;

	size_t maxToCopy = code.size() - _codeOffset;
	size_t numToCopy = std::min(maxToCopy, _bufferSize);
	std::copy_n(&code[_codeOffset], numToCopy, _bufferData);
	return numToCopy;
}

void selfdestruct(
	evmc_context* _context,
	evmc_address const* _addr,
	evmc_address const* _beneficiary
) noexcept
{
	(void)_addr;
	auto& env = static_cast<ExtVMFace&>(*_context);
	env.suicide(*_beneficiary);
}


void log(
	evmc_context* _context,
	evmc_address const* _addr,
	uint8_t const* _data,
	size_t _dataSize,
	evmc_uint256be const _topics[],
	size_t _numTopics
) noexcept
{
	(void)_addr;
	auto& env = static_cast<ExtVMFace&>(*_context);
	env.log(_topics, _numTopics, bytesConstRef{ _data, _dataSize });
}

int64_t evm_executeSQL(
	evmc_context* _context,
	evmc_address const* _addr,
    uint8_t _type,
	uint8_t const* _name,
	size_t _nameSize,
    uint8_t const* _raw,
    size_t _rawSize
) noexcept
{
    auto& env = static_cast<ExtVMFace&>(*_context);
    return env.executeSQL(_addr, _type, bytesConstRef{ _name, _nameSize }, bytesConstRef{ _raw, _rawSize });	
}

void getTxContext(evmc_tx_context* result, evmc_context* _context) noexcept
{
	auto& env = static_cast<ExtVMFace&>(*_context);
	result->tx_gas_price = env.gasPrice;
	result->tx_origin = env.origin;
	result->block_coinbase = env.envInfo().coin_base();
	result->block_number = env.envInfo().block_number();
	result->block_timestamp = env.envInfo().block_timestamp();
	result->block_gas_limit = env.envInfo().gasLimit();
    result->block_difficulty = evmc_uint256be{};
}

void getBlockHash(evmc_uint256be* o_hash, evmc_context* _envPtr, int64_t _number)
{
	auto& env = static_cast<ExtVMFace&>(*_envPtr);
	*o_hash = env.blockHash(_number);
}

void create(evmc_result* o_result, ExtVMFace& _env, evmc_message const* _msg) noexcept
{
	int64_t gas = _msg->gas;
	evmc_uint256be value = _msg->value;
	bytesConstRef init = { _msg->input_data, _msg->input_size };
	// ExtVM::create takes the sender address from .myAddress.
	assert(std::memcmp(_msg->sender.bytes, _env.myAddress.bytes, sizeof(_env.myAddress)) == 0);

	CreateResult result = _env.create(value, gas, init, Instruction::CREATE, { {0} });
	o_result->status_code = result.status;
	o_result->gas_left = static_cast<int64_t>(gas);
	o_result->release = nullptr;

	if (result.status == EVMC_SUCCESS)
	{
		o_result->create_address = result.address;
		o_result->output_data = nullptr;
		o_result->output_size = 0;
	}
	else
	{
		// Pass the output to the EVM without a copy. The EVM will delete it
		// when finished with it.

		// First assign reference. References are not invalidated when vector
		// of bytes is moved. See `.takeBytes()` below.
		o_result->output_data = result.output.data();
		o_result->output_size = result.output.size();

#ifdef DEBUG
		if (o_result->output_size) {
			// fix an issue that Stack around the variable 'result' was corrupted
			evmc_get_optional_data(o_result)->pointer = std::malloc(o_result->output_size);
			new(evmc_get_optional_data(o_result)->pointer) bytes(result.output.takeBytes());

			o_result->release = [](evmc_result const* _result)
			{
				uint8_t* data = (uint8_t*)evmc_get_const_optional_data(_result)->pointer;
				auto& output = reinterpret_cast<bytes const&>(*data);
				// Explicitly call vector's destructor to release its data.
				// This is normal pattern when placement new operator is used.
				output.~bytes();
				std::free(data);
			};
		}
#else
		// Place a new vector of bytes containing output in result's reserved memory.
		auto* data = evmc_get_optional_data(o_result);
		//static_assert(sizeof(bytes) <= sizeof(*data), "Vector is too big");
		new(data) bytes(result.output.takeBytes());
		// Set the destructor to delete the vector.
		o_result->release = [](evmc_result const* _result)
		{
			auto* data = evmc_get_const_optional_data(_result);
			auto& output = reinterpret_cast<bytes const&>(*data);
			// Explicitly call vector's destructor to release its data.
			// This is normal pattern when placement new operator is used.
			output.~bytes();
		};
#endif // DEBUG
	}
}

void call(evmc_result* o_result, evmc_context* _context, evmc_message const* _msg) noexcept
{
	assert(_msg->gas >= 0 && "Invalid gas value");
	auto& env = static_cast<ExtVMFace&>(*_context);

	if (_msg->kind == EVMC_CREATE)
		return create(o_result, env, _msg);

	CallParameters params;
	params.gas = _msg->gas;
	params.apparentValue = _msg->value;
	if (_msg->kind == EVMC_DELEGATECALL)
		params.valueTransfer = { {0} };
	else
		params.valueTransfer = params.apparentValue;
	params.senderAddress = _msg->sender;
	params.codeAddress = _msg->destination;
	params.receiveAddress =
		_msg->kind == EVMC_CALL ? params.codeAddress : env.myAddress;
	params.data = { _msg->input_data, _msg->input_size };
	params.staticCall = (_msg->flags & EVMC_STATIC) != 0;

	CallResult result = env.call(params);
	o_result->status_code = result.status;
	o_result->gas_left = params.gas;
	o_result->release = nullptr;

	// Pass the output to the EVM without a copy. The EVM will delete it
	// when finished with it.

	// First assign reference. References are not invalidated when vector
	// of bytes is moved. See `.takeBytes()` below.
	o_result->output_data = result.output.data();
	o_result->output_size = result.output.size();

#ifdef DEBUG
	if (o_result->output_size) {
		// fix an issue that Stack around the variable 'result' was corrupted
		evmc_get_optional_data(o_result)->pointer = std::malloc(o_result->output_size);
		new(evmc_get_optional_data(o_result)->pointer) bytes(result.output.takeBytes());

		o_result->release = [](evmc_result const* _result)
		{
			uint8_t* data = (uint8_t*)evmc_get_const_optional_data(_result)->pointer;
			auto& output = reinterpret_cast<bytes const&>(*data);
			// Explicitly call vector's destructor to release its data.
			// This is normal pattern when placement new operator is used.
			output.~bytes();
			std::free(data);
		};
	}
#else
	// Place a new vector of bytes containing output in result's reserved memory.
	auto* data = evmc_get_optional_data(o_result);
	//static_assert(sizeof(bytes) <= sizeof(*data), "Vector is too big");
	new(data) bytes(result.output.takeBytes());
	// Set the destructor to delete the vector.
	o_result->release = [](evmc_result const* _result)
	{
		auto* data = evmc_get_const_optional_data(_result);
		auto& output = reinterpret_cast<bytes const&>(*data);
		// Explicitly call vector's destructor to release its data.
		// This is normal pattern when placement new operator is used.
		output.~bytes();
	};
#endif

}

int64_t table_create(struct evmc_context* _context,
        const struct evmc_address* address,
        uint8_t const* _name,
        size_t _nameSize,
        uint8_t const* _raw,
		size_t _rawSize) {
    auto& env = static_cast<ExtVMFace&>(*_context);
    return env.table_create(address, bytesConstRef{_name, _nameSize}, 
            bytesConstRef{_raw, _rawSize});
}

int64_t table_rename(struct evmc_context* _context,
    const struct evmc_address* address,
    uint8_t const* _name,
    size_t _nameSize,
    uint8_t const* _raw,
	size_t _rawSize)
{
    auto& env = static_cast<ExtVMFace&>(*_context);
    return env.table_rename(address, bytesConstRef{ _name, _nameSize }, bytesConstRef{ _raw, _rawSize });
}
int64_t table_insert(struct evmc_context* _context,
    const struct evmc_address* address,
    uint8_t const* _name,
    size_t _nameSize,
    uint8_t const* _raw,
	size_t _rawSize)
{
    auto& env = static_cast<ExtVMFace&>(*_context);
    return env.table_insert(address, bytesConstRef{ _name, _nameSize }, bytesConstRef{ _raw, _rawSize });
}

int64_t table_delete(struct evmc_context* _context,
    const struct evmc_address* address,
    uint8_t const* _name,
    size_t _nameSize,
    uint8_t const* _raw,
	size_t _rawSize)
{
    auto& env = static_cast<ExtVMFace&>(*_context);
    return env.table_delete(address, bytesConstRef{ _name, _nameSize }, bytesConstRef{ _raw, _rawSize });
}
int64_t table_drop(struct evmc_context* _context,
    const struct evmc_address* address,
    uint8_t const* _name,
    size_t _nameSize)
{
    auto& env = static_cast<ExtVMFace&>(*_context);
    return env.table_drop(address, bytesConstRef{ _name, _nameSize });
}
int64_t table_update(struct evmc_context* _context,
    const struct evmc_address* address,
    uint8_t const* _name,
    size_t _nameSize,
    uint8_t const* _raw1,
    size_t _rawSize1,
    uint8_t const* _raw2,
    size_t _rawSize2)
{
    auto& env = static_cast<ExtVMFace&>(*_context);
    return env.table_update(address, bytesConstRef{ _name, _nameSize }, bytesConstRef{ _raw1, _rawSize1 }, bytesConstRef{ _raw2, _rawSize2 });
}

int64_t table_grant(struct evmc_context* _context,
    const struct evmc_address* address1,
    const struct evmc_address* address2,
    uint8_t const* _name,
    size_t _nameSize,
    uint8_t const* _row,
    size_t _rowSize) {
    auto& env = static_cast<ExtVMFace&>(*_context);
    return env.table_grant(address1, address2, 
            bytesConstRef{_name, _nameSize}, bytesConstRef{_row, _rowSize});
}

void table_get_handle(struct evmc_context* _context,
    const struct evmc_address* address,
    uint8_t const* _name,
    size_t _nameSize,
    uint8_t const* _raw,
    size_t _rawSize,
    struct evmc_uint256be* result) {
    auto& env = static_cast<ExtVMFace&>(*_context);
    *result =  env.table_get_handle(address, 
            bytesConstRef{_name, _nameSize}, bytesConstRef{_raw, _rawSize});
}

void table_get_lines(struct evmc_context* _context,
    const struct evmc_uint256be* handle,
    struct evmc_uint256be* result) {
    auto& env = static_cast<ExtVMFace&>(*_context);
    *result = env.table_get_lines(handle);
}

void table_get_columns(struct evmc_context* _context,
    const struct evmc_uint256be* handle,
    struct evmc_uint256be* result) {
    auto& env = static_cast<ExtVMFace&>(*_context);
    *result = env.table_get_columns(handle);
}

// TODO: Remove the copy to outBuf
size_t get_column_by_name(evmc_context* _context,
    const evmc_uint256be* _handle,
    size_t _row,
    uint8_t const* _column,
    size_t _columnSize,
    uint8_t* _outBuf, 
    size_t _outSize) {
    auto& env = static_cast<ExtVMFace&>(*_context);

    size_t size = env.table_get_by_key(_handle, _row, 
            bytesConstRef{_column, _columnSize}, _outBuf+32, _outSize-32);
    memcpy(_outBuf, toEvmC((uint256)size).bytes, 32);
    return size;
}

size_t get_column_by_index(evmc_context *_context,
    const evmc_uint256be *_handle,
    size_t _row,
    size_t _column,
    uint8_t *_outBuf, 
    size_t _outSize) {
    auto& env = static_cast<ExtVMFace&>(*_context);

    size_t size = env.table_get_by_index(_handle, _row, _column, 
            _outBuf+32, _outSize-32);
    memcpy(_outBuf, toEvmC((uint256)size).bytes, 32);
    return size;
}

void db_trans_begin(struct evmc_context* _context)
{
    auto& env = static_cast<ExtVMFace&>(*_context);
    env.db_trans_begin();
}
int64_t db_trans_submit(struct evmc_context* _context)
{
    auto& env = static_cast<ExtVMFace&>(*_context);
    return env.db_trans_submit();
}

void release_resource(struct evmc_context* _context)
{
    auto& env = static_cast<ExtVMFace&>(*_context);
    env.release_resource();
}

void get_column_len_by_name(evmc_context*_context, 
        const evmc_uint256be *_handle, 
        size_t _row,
        const uint8_t *_column, 
        size_t _size, 
        evmc_uint256be *_len) {
    auto &env = static_cast<ExtVMFace&>(*_context);
    *_len = env.get_column_len(_handle, _row, bytesConstRef{_column, _size});
}

void get_column_len_by_index(evmc_context*_context, 
        const evmc_uint256be *_handle, 
        size_t _row,
        size_t _column, 
        evmc_uint256be *_len) {
    auto &env = static_cast<ExtVMFace&>(*_context);
    *_len = env.get_column_len(_handle, _row, _column);
}

evmc_context_fn_table const fnTable = {
	accountExists,
	getStorage,
	setStorage,
	getBalance,
	getCodeSize,
	copyCode,
	selfdestruct,
	call,
	getTxContext,
	getBlockHash,
	log,
	evm_executeSQL,

    table_create,
    table_rename,
    table_insert,
    table_delete,
    table_drop,
    table_update,
    table_grant,

    table_get_handle,
    table_get_lines,
    table_get_columns,
    get_column_by_name,
    get_column_by_index,

    db_trans_begin,
    db_trans_submit,

    release_resource,

    get_column_len_by_name,
    get_column_len_by_index,
};

ExtVMFace::ExtVMFace(EnvInfo const& envInfo, evmc_address _myAddress, evmc_address _caller, evmc_address _origin,
		evmc_uint256be _value, evmc_uint256be _gasPrice, 
		bytesConstRef _data, bytes _code, evmc_uint256be _codeHash, int32_t _depth,
		bool _isCreate, bool _staticCall)
: evmc_context{&fnTable}
, myAddress(_myAddress)
, caller(_caller)
, origin(_origin)
, value(_value)
, gasPrice(_gasPrice)
, data(_data)
, code(_code)
, codeHash(_codeHash)
, depth(_depth)
, isCreate(_isCreate)
, staticCall(_staticCall)
, envInfo_(envInfo) {

}

} // namespace ripple
