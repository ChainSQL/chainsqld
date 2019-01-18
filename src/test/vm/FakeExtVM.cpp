#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <initializer_list>
#include <algorithm>
#include <cstdlib>

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

    inline 
    evmc_uint256be & toEvmC(uint256 const &_h) {
        return const_cast<evmc_uint256be&>(
                reinterpret_cast<evmc_uint256be const&>(_h));
    }

    // convert evmc address to string
    std::string evmcAddrToString(const evmc_address *addr) {
        if (!addr) {
            return "0x";
        }
        std::ostringstream os;
        os << std::setiosflags(std::ios::uppercase) << std::hex;
        os.fill('0');
        os << "0x";
        size_t len = sizeof(addr->bytes)/sizeof(addr->bytes[0]);
        for (size_t i=0; i!=len; ++i) {
            os << std::setw(2) << (int)(addr->bytes[i]);
        }
        return os.str();
    }

    template<typename T> inline
    void PrintInputParams(std::initializer_list<T> li) {
        std::for_each(li.begin(), li.end(), 
                [](const T &elem) { std::cout << elem << std::endl; });
    }

    template<class TKey, class TVal> inline
    void PrintInputParams(const char *instruction, 
            std::initializer_list<std::pair<TKey, TVal>> li) {
        std::cout << "****************************************\n";
        std::cout << instruction << ":" << std::endl;
        std::for_each(li.begin(), li.end(), 
                [](const std::pair<TKey, TVal> &elem) {
                    std::cout << "\t" << elem.first 
                        << "\t: " << elem.second << std::endl;
                });
        std::cout << "****************************************\n";
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

int64_t FakeExtVM::executeSQL(evmc_address const* _addr, uint8_t _type, bytesConstRef const& _name, bytesConstRef const& _raw) {
    return 0;
}

evmc_uint256be FakeExtVM::blockHash(int64_t  const&_number) {
	return evmc_uint256be();
}

int64_t FakeExtVM::table_create(const evmc_address *address,
        bytesConstRef const &_name, 
        bytesConstRef const &_raw) {
    test::PrintInputParams<const char*, std::string>("CreateTable", 
            {{"ownerAddr", test::evmcAddrToString(address)},
             {"tableName", _name.toString()}, 
             {"createStmt", _raw.toString()}}
            );
    return true;
}

int64_t FakeExtVM::table_rename(const evmc_address* address,
        bytesConstRef const &oname, 
        bytesConstRef const &nname) {
    test::PrintInputParams<const char*, std::string>("RenameTable", 
            {{"ownerAddr", test::evmcAddrToString(address)},
             {"oldName", oname.toString()}, 
             {"newName", nname.toString()}}
            );
    return true;
}

int64_t FakeExtVM::table_insert(const evmc_address *address,
        bytesConstRef const &name, 
        bytesConstRef const &stmt) {
    test::PrintInputParams<const char*, std::string>("InsertSQL", 
            {{"ownerAddr", test::evmcAddrToString(address)},
             {"tableName", name.toString()}, 
             {"insertStmt", stmt.toString()}}
            );
    return true;
}

int64_t FakeExtVM::table_delete(const evmc_address *address,
        bytesConstRef const &name, bytesConstRef const &stmt) {
    test::PrintInputParams<const char*, std::string>("DeleteSQL", 
            {{"ownerAddr", test::evmcAddrToString(address)},
             {"tableName", name.toString()}, 
             {"deleteStmt", stmt.toString()}}
            );
    return true;
}

int64_t FakeExtVM::table_drop(const evmc_address *address,
        bytesConstRef const &name) {
    test::PrintInputParams<const char*, std::string>("DropTable", 
            {{"ownerAddr", test::evmcAddrToString(address)},
             {"tableName", name.toString()}}
            );
    return true;
}

int64_t FakeExtVM::table_update(const evmc_address *address,
        bytesConstRef const &name, 
        bytesConstRef const &cond, 
        bytesConstRef const &upd) {
    test::PrintInputParams<const char*, std::string>("UpdateSQL", 
            {{"ownerAddr", test::evmcAddrToString(address)},
             {"tableName", name.toString()}, 
             {"condition", cond.toString()}, 
             {"updateValue", upd.toString()}}
            );
    return true;
}

int64_t FakeExtVM::table_grant(const evmc_address *owner,
        const evmc_address *to, 
        bytesConstRef const &name, 
        bytesConstRef const &stmt) {
    test::PrintInputParams<const char*, std::string>("GrantSQL", 
            {{"ownerAddr", test::evmcAddrToString(owner)},
             {"toAddr", test::evmcAddrToString(to)}, 
             {"tableName", name.toString()}, 
             {"grantStmt", stmt.toString()}}
            );
    return false;
}

evmc_uint256be FakeExtVM::table_get_handle(const evmc_address *address, 
        bytesConstRef const &name, 
        bytesConstRef const &stmt) {
    test::PrintInputParams<const char*, std::string>("SelectSQL", 
            {{"ownerAddr", test::evmcAddrToString(address)},
             {"tableName", name.toString()}, 
             {"condition", stmt.toString()}}
            );
    return test::toEvmC((uint256)10);
}

evmc_uint256be FakeExtVM::table_get_lines(const evmc_uint256be *handle) {
    test::PrintInputParams<const char*, u256>("GetRowSize", 
            {{"handle", test::fromEvmC(*handle)}}
            );
    return test::toEvmC((uint256)20);
}

evmc_uint256be FakeExtVM::table_get_columns(
        const evmc_uint256be *handle) {
    test::PrintInputParams<const char*, u256>("GetColSize", 
            {{"handle", test::fromEvmC(*handle)}}
            );
    return test::toEvmC((uint256)30);
}

size_t FakeExtVM::table_get_by_key(const evmc_uint256be *_handle, 
        size_t _row, 
        bytesConstRef const& _column, 
        uint8_t *_outBuf, 
        size_t _outSize) {
    unsigned uh = (unsigned)(test::fromEvmC(*_handle));
    std::string column = _column.toString();
    test::PrintInputParams<const char*, std::string>("GetColumnByName", 
            {{"handle", std::to_string(uh)}, 
             {"rowNum", std::to_string(_row)}, 
             {"colName", column}, 
             {"colLen", std::to_string(_outSize)}}
            );
    // to simulate 
    size_t copySize = column.size()<_outSize?column.size():_outSize;
    memcpy(_outBuf, column.c_str(), copySize);
    return copySize;
}

size_t FakeExtVM::table_get_by_index(const evmc_uint256be *_handle, 
        size_t _row, 
        size_t _column, 
        uint8_t *_outBuf, 
        size_t _outSize) {
    unsigned uh = (unsigned)(test::fromEvmC(*_handle));
    test::PrintInputParams<const char*, std::string>("GetColumnByIndex", 
            {{"handle", std::to_string(uh)}, 
             {"rowNum", std::to_string(_row)}, 
             {"colNum", std::to_string(_column)}, 
             {"colLen", std::to_string(_outSize)}}
            );
    // to simulate 
    const char * const value = "get column's value by index";
    size_t copySize = strlen(value)<_outSize?strlen(value):_outSize;
    memcpy(_outBuf, value, copySize);
    return copySize;
}

void FakeExtVM::db_trans_begin() {
    test::PrintInputParams<const char*, const char*>(
            "BeginTransaction", {{"parameters", "None"}});
}

int64_t FakeExtVM::db_trans_submit() {
    test::PrintInputParams<const char*, const char*>(
            "SubmitTransaction", {{"parameters", "None"}});
    return true;
}

void FakeExtVM::release_resource() {
    test::PrintInputParams<const char*, const char*>(
            "OutOfScope", {{"parameters", "None"}});
}

evmc_uint256be FakeExtVM::get_column_len(
        const evmc_uint256be *_handle, 
        size_t _rowNum, 
        bytesConstRef const &_column) {
    unsigned uh = (unsigned)(test::fromEvmC(*_handle));
    test::PrintInputParams<const char*, std::string>(
            "GetColumnLenByName", 
            {{"handle", std::to_string(uh)}, 
             {"rowNum", std::to_string(_rowNum)}, 
             {"colName", _column.toString()}}
            );
    return test::toEvmC((uint256)(_column.size()));
}

evmc_uint256be FakeExtVM::get_column_len(
        const evmc_uint256be *_handle, 
        size_t _row, 
        size_t _column) {
    unsigned uh = (unsigned)(test::fromEvmC(*_handle));
    test::PrintInputParams<const char*, std::string>(
            "GetColumnLenByIndex", 
            {{"handle", std::to_string(uh)}, 
             {"rowNum", std::to_string(_row)}, 
             {"colNum", std::to_string(_column)}}
            );
    return test::toEvmC((uint256)100);
}

// NOTE:this function is limited to only one parameter,
// and the type of the parameter is integer or convertible 
// to integeral type
void FakeExtVM::log(evmc_uint256be const* topics, 
        size_t numTopics, 
        bytesConstRef const &_data) {
    unsigned utopic = (unsigned)(test::fromEvmC(*topics));
    evmc_uint256be temp = test::toEvmC(_data.toString());
    unsigned data = (unsigned)(test::fromEvmC(temp));
    test::PrintInputParams<const char*, std::string>("Log", 
            {{"topics", std::to_string(utopic)}, 
             {"numTopics", std::to_string(numTopics)}, 
             {"data", std::to_string(data)}}
            );
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

owning_bytes_ref FakeExecutive::create(const evmc_address& contractAddress, 
	const evmc_uint256be& codeHash, int64_t &gas) {
	EnvInfo info;
	evmc_address myAddress = contractAddress;
	evmc_address caller = { { 1,1,0,0 } };
	evmc_address origin = { { 1,1,1,0 } };
	evmc_uint256be value = { { 0 } };
	evmc_uint256be gasPrice = { { 1,0,0 } };
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
