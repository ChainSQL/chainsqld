#include <iostream>

#include "FakeExtVM.h"

#include <peersafe/vm/VMFactory.h>

namespace ripple {

FakeExtVM::State FakeExtVM::m_s;

FakeExtVM::FakeExtVM(EnvInfo const& envInfo, evmc_address _myAddress, 
	evmc_address _caller, evmc_address _origin,
	evmc_uint256be _value, evmc_uint256be _gasPrice,
	bytesConstRef _data, bytes _code, evmc_uint256be _codeHash, 
	int32_t _depth, bool _isCreate, bool _staticCall)
: ExtVMFace(envInfo, _myAddress, _caller, _origin, _value, _gasPrice,
	_data, _code, _codeHash, _depth, _isCreate, _staticCall) {

}

CreateResult FakeExtVM::create(evmc_uint256be const& endowment, int64_t const& gas,
	bytesConstRef const& code, Instruction op, evmc_uint256be const& salt) {
	evmc_address contractAddress = { {5,6,7,8} };
	CreateResult result = { EVMC_SUCCESS , std::move(owning_bytes_ref()), contractAddress };
	bytes newCode = code.toBytes();
	FakeExecutive execute(newCode);
	execute.create(contractAddress);
	return result;
}

CallResult FakeExtVM::call(CallParameters& p) {
	CallResult result = { EVMC_SUCCESS , std::move(owning_bytes_ref())};
	
	auto it = FakeExtVM::m_s.find(AccountID::fromVoid(p.codeAddress.bytes));
	if (it != FakeExtVM::m_s.end()) {
		bytes code = it->second;
		if (code.size()) {
			FakeExecutive execute(p.data, code);
			execute.call(p.codeAddress);
		}
	}

	return result;
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

int FakeExecutive::create(const evmc_address& contractAddress) {
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
	int64_t gas = 3000000;
	owning_bytes_ref result = vmc->exec(gas, ext);
	FakeExtVM::m_s[AccountID::fromVoid(myAddress.bytes)] = result.toBytes();
	return 0;
}

int FakeExecutive::call(const evmc_address& contractAddress) {
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
	int64_t gas = 3000000;
	owning_bytes_ref result = vmc->exec(gas, ext);
	return 0;
}


} // namespace ripple
