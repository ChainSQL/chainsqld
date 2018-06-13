#include <assert.h>

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