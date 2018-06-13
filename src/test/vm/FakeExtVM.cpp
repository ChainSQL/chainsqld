#include <iostream>

#include "FakeExtVM.h"

#include <peersafe/vm/VMFactory.h>

namespace ripple {

namespace test {
	template <class T, class _In>
	inline T fromBigEndian(_In const& _bytes)
	{
		T ret = (T)0;
		for (auto i : _bytes)
			ret = (T)((ret << 8) | (byte)(typename std::make_unsigned<decltype(i)>::type)i);
		return ret;
	}

	inline u256 fromEvmC(evmc_uint256be const& _n)
	{
		return fromBigEndian<u256>(_n.bytes);
	}

	inline evmc_uint256be toEvmC(const std::string& value)
	{
		evmc_uint256be evmc;
		std::memset(&evmc, 0, sizeof(evmc));
		std::memcpy(&evmc, value.c_str(), sizeof(evmc));
		return evmc;
		//return reinterpret_cast<evmc_uint256be const&>(_h);
	}

}

FakeExtVM::State FakeExtVM::m_s;
FakeExtVM::KV FakeExtVM::m_kv;

FakeExtVM::FakeExtVM(EnvInfo const& envInfo, evmc_address _myAddress, 
	evmc_address _caller, evmc_address _origin,
	evmc_uint256be _value, evmc_uint256be _gasPrice,
	bytesConstRef _data, bytes _code, evmc_uint256be _codeHash, 
	int32_t _depth, bool _isCreate, bool _staticCall)
: ExtVMFace(envInfo, _myAddress, _caller, _origin, _value, _gasPrice,
	_data, _code, _codeHash, _depth, _isCreate, _staticCall) {

}

CreateResult FakeExtVM::create(evmc_uint256be const& endowment, int64_t & gas,
	bytesConstRef const& code, Instruction op, evmc_uint256be const& salt) {
	evmc_address contractAddress = { {5,6,7,8} };
	bytes newCode = code.toBytes();
	FakeExecutive execute(newCode);
	auto result = execute.create(contractAddress, gas);
	return { EVMC_SUCCESS , std::move(result), contractAddress };
}

CallResult FakeExtVM::call(CallParameters& p) {
	auto it = FakeExtVM::m_s.find(AccountID::fromVoid(p.codeAddress.bytes));
	if (it != FakeExtVM::m_s.end()) {
		bytes code = it->second;
		if (code.size()) {
			FakeExecutive execute(p.data, code);
			auto result = execute.call(p.codeAddress, p.gas);
			return{ EVMC_SUCCESS, std::move(result) };
		}
	}

	return { EVMC_FAILURE , std::move(owning_bytes_ref())};
}

bool FakeExtVM::exists(evmc_address const& addr) {
	auto it = FakeExtVM::m_s.find(AccountID::fromVoid(addr.bytes));
	if (it != FakeExtVM::m_s.end()) {
		return true;
	}
	return false;
}

size_t FakeExtVM::codeSizeAt(evmc_address const& addr) {
	auto it = FakeExtVM::m_s.find(AccountID::fromVoid(addr.bytes));
	if (it != FakeExtVM::m_s.end()) {
		return it->second.size();
	}
	return 0;
}

evmc_uint256be FakeExtVM::store(evmc_uint256be const& key) {
	u256 k = test::fromEvmC(key);
	auto it = FakeExtVM::m_kv.find(k);
	if (it == FakeExtVM::m_kv.end()) {
		std::string v;
		FakeExtVM::m_kv[k] = v;
		return test::toEvmC(v);
	}
	return test::toEvmC(it->second);
}

void FakeExtVM::setStore(evmc_uint256be const& key, evmc_uint256be const& value) {
	u256 k = test::fromEvmC(key);
	//u256 v = test::fromEvmC(value);
	std::string v((const char*)value.bytes, sizeof(value));
	FakeExtVM::m_kv[k] = v;
}

evmc_uint256be FakeExtVM::blockHash(int64_t  const&_number) {
	return evmc_uint256be();
}


FakeExecutive::FakeExecutive(const bytesConstRef& data, const bytes& code)
: data_(data)
, code_(code) {
}

FakeExecutive::FakeExecutive(const bytes& code)
: data_{(uint8_t*)"",0}
, code_(code) {

}

FakeExecutive::FakeExecutive(const bytesConstRef& data, const evmc_address& contractAddress)
: data_(data)
, code_(FakeExtVM::m_s.find(AccountID::fromVoid(contractAddress.bytes))->second) {
}

owning_bytes_ref FakeExecutive::create(const evmc_address& contractAddress, int64_t& gas) {
	EnvInfo info;
	evmc_address myAddress = contractAddress;
	evmc_address caller = { { 1,1,0,0 } };
	evmc_address origin = { { 1,1,1,0 } };
	evmc_uint256be value = { { 0 } };
	evmc_uint256be gasPrice = { { 1,0,0 } };
	evmc_uint256be codeHash = { {0} };
	std::size_t hash = std::hash<std::string>{}(std::string(code_.begin(), code_.end()));
	std::memcpy(&codeHash, &hash, sizeof(hash));
	int32_t depth = 0;
	bool isCreate = true;
	bool staticCall = false;
	VMFace::pointer vmc = VMFactory::create(VMKind::JIT);
	assert(vmc);
	FakeExtVM ext(info, myAddress, caller, origin, value, gasPrice,
		data_, code_, codeHash, depth, isCreate, staticCall);
	owning_bytes_ref result = vmc->exec(gas, ext);
	FakeExtVM::m_s[AccountID::fromVoid(myAddress.bytes)] = result.toBytes();
	return result;
}

owning_bytes_ref FakeExecutive::call(const evmc_address& contractAddress, int64_t& gas) {
	EnvInfo info;
	evmc_address myAddress = contractAddress;
	evmc_address caller = { {1,1,0,0} };
	evmc_address origin = { {1,1,1,0} };
	evmc_uint256be value = { {0} };
	evmc_uint256be gasPrice = { {1,0,0} };
	evmc_uint256be codeHash = { {0} };
	std::size_t hash = std::hash<std::string>{}(std::string(code_.begin(), code_.end()));
	std::memcpy(&codeHash, &hash, sizeof(hash));

	int32_t depth = 0;
	bool isCreate = false;
	bool staticCall = false;
	VMFace::pointer vmc = VMFactory::create(VMKind::JIT);
	assert(vmc);
	FakeExtVM ext(info, myAddress, caller, origin, value, gasPrice, 
		data_, code_, codeHash, depth, isCreate, staticCall);
	owning_bytes_ref result = vmc->exec(gas, ext);
	return result;
}


} // namespace ripple
