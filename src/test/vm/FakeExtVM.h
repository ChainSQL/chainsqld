#ifndef __H_TEST_FAKEEXTVM_H__
#define __H_TEST_FAKEEXTVM_H__

#include <string>
#include <map>
#include <functional>
#include <peersafe/vm/ExtVMFace.h>
#include <ripple/protocol/AccountID.h>

namespace ripple {

class FakeExtVM : public ExtVMFace {
public:
	FakeExtVM(EnvInfo const& envInfo, evmc_address _myAddress, evmc_address _caller, evmc_address _origin,
		evmc_uint256be _value, evmc_uint256be _gasPrice,
		bytesConstRef _data, bytes _code, evmc_uint256be _codeHash, int32_t _depth,
		bool _isCreate, bool _staticCall);

	CreateResult create(evmc_uint256be const&, int64_t&,
		bytesConstRef const&, Instruction, evmc_uint256be const&) final;
	CallResult call(CallParameters&) final;
	evmc_uint256be blockHash(int64_t  const&_number) final;

	bool exists(evmc_address const&) final;
	size_t codeSizeAt(evmc_address const& addr) final;
	evmc_uint256be store(evmc_uint256be const&) final;
	void setStore(evmc_uint256be const&, evmc_uint256be const&) final;

	using State = std::map<AccountID, bytes>;
	using KV = std::map<size_t, std::string>;
	static State m_s;
	static KV m_kv;
private:
};

class FakeExecutive {
public:
	FakeExecutive(const bytesConstRef& data, const bytes& code);
	FakeExecutive(const bytes& code);
	FakeExecutive(const bytesConstRef& data, const evmc_address& contractAddress);
	~FakeExecutive() = default;

	owning_bytes_ref create(const evmc_address& contractAddress, int64_t &gas);
	owning_bytes_ref call(const evmc_address& contractAddress, int64_t &gas);
private:
	const bytesConstRef& data_;
	const bytes& code_;
};

} // namespace ripple

#endif // !__H_TEST_FAKEEXTVM_H__
