#ifndef __H_TEST_FAKEEXTVM_H__
#define __H_TEST_FAKEEXTVM_H__

#include <string>
#include <map>
#include <functional>
#include <peersafe/vm/ExtVMFace.h>
#include <ripple/protocol/AccountID.h>

#include <boost/multiprecision/cpp_int.hpp>

using u256 = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<256, 256, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>;

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

    int64_t executeSQL(evmc_address const* _addr, uint8_t _type, bytesConstRef const& _name, bytesConstRef const& _raw) final;

    /** begin: instruction extension */
	int64_t table_create(const evmc_address *address,
            bytesConstRef const &_name, 
            bytesConstRef const &_raw) override final;

	int64_t table_rename(const evmc_address *address,
            bytesConstRef const &oname, 
            bytesConstRef const &nname) override final;

	int64_t table_insert(const evmc_address *address,
            bytesConstRef const &name, 
            bytesConstRef const &stmt) override final;

	int64_t table_delete(const evmc_address *address,
            bytesConstRef const &name, 
            bytesConstRef const &stmt) override final;

	int64_t table_drop(const evmc_address *address,
            bytesConstRef const &name) override final;

	int64_t table_update(const evmc_address *address,
            bytesConstRef const &name, 
            bytesConstRef const &cond, 
            bytesConstRef const &upd) override final;

	int64_t table_grant(const evmc_address *owner,
            const evmc_address *to, 
            bytesConstRef const &name, 
            bytesConstRef const &stmt) override final;

    // interface to select statement
    evmc_uint256be table_get_handle(const evmc_address *address, 
            bytesConstRef const &name, 
            bytesConstRef const &stmt) override final;

    evmc_uint256be table_get_lines(
            const evmc_uint256be *handle) override final;

    evmc_uint256be table_get_columns(
            const evmc_uint256be *handle) override final;

    size_t table_get_by_key(const evmc_uint256be *_handle, 
            size_t _row, bytesConstRef const& _column, 
            uint8_t *_outBuf, size_t _outSize) override final;

    size_t table_get_by_index(const evmc_uint256be *_handle, 
            size_t _row, size_t _column, uint8_t *_outBuf, 
            size_t _outSize) override final;

    // interface to transaction control
    void db_trans_begin() override final;
	int64_t db_trans_submit() override final;
    void release_resource() override final;

    evmc_uint256be get_column_len(const evmc_uint256be *handle, 
            size_t _rowNum, 
            bytesConstRef const &_column) override final; 
    evmc_uint256be get_column_len(const evmc_uint256be *_handle, 
            size_t _row, size_t _column) override final;

    /** end: Instruction extension */

	void log(evmc_uint256be const* topics, 
            size_t numTopics, 
            bytesConstRef const &_data) override final;

	using State = std::map<AccountID, bytes>;
	using KV = std::map<u256, std::string>;
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
	owning_bytes_ref create(const evmc_address& contractAddress, const evmc_uint256be& codeHash, int64_t &gas);
	owning_bytes_ref call(const evmc_address& contractAddress, int64_t &gas);
private:
	const bytesConstRef& data_;
	const bytes& code_;
};

} // namespace ripple

#endif // !__H_TEST_FAKEEXTVM_H__
