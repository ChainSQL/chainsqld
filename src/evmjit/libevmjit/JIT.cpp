#include "JIT.h"

#include <cstddef>
#include <mutex>

#include "preprocessor/llvm_includes_end.h"
#include "preprocessor/llvm_includes_start.h"
#include <evmc/evmc.h>
#include <llvm/ADT/StringSwitch.h>
#include <llvm/ADT/Triple.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_os_ostream.h>

#include "BuildInfo.gen.h"
#include "Cache.h"
#include "Compiler.h"
#include "ExecStats.h"
#include "Ext.h"
#include "Optimizer.h"
#include "Utils.h"


// FIXME: Move these checks to evmc tests.
static_assert(sizeof(evmc_uint256be) == 32, "evmc_uint256be is too big");
static_assert(sizeof(evmc_address) == 20, "evmc_address is too big");
static_assert(sizeof(evmc_result) == 64, "evmc_result does not fit cache line");
static_assert(sizeof(evmc_message) <= 18 * 8, "evmc_message not optimally packed");
static_assert(offsetof(evmc_message, code_hash) % 8 == 0, "evmc_message.code_hash not aligned");

// Check enums match int size.
// On GCC/clang the underlying type should be unsigned int, on MSVC int
static_assert(
    sizeof(evmc_call_kind) == sizeof(int), "Enum `evmc_call_kind` is not the size of int");
static_assert(sizeof(evmc_revision) == sizeof(int), "Enum `evmc_revision` is not the size of int");

constexpr size_t optionalDataSize = sizeof(evmc_result) - offsetof(evmc_result, create_address);
static_assert(optionalDataSize == sizeof(evmc_result_optional_data), "");


namespace dev
{
namespace evmjit
{
using namespace eth::jit;

namespace
{
using ExecFunc = ReturnCode (*)(ExecutionContext*);

struct CodeMapEntry
{
    ExecFunc func = nullptr;
    size_t hits = 0;

    CodeMapEntry() = default;
    explicit CodeMapEntry(ExecFunc func) : func(func) {}
};

char toChar(evmc_revision rev)
{
    switch (rev)
    {
    case EVMC_FRONTIER:
        return 'F';
    case EVMC_HOMESTEAD:
        return 'H';
    case EVMC_TANGERINE_WHISTLE:
        return 'T';
    case EVMC_SPURIOUS_DRAGON:
        return 'S';
    case EVMC_BYZANTIUM:
        return 'B';
    case EVMC_CONSTANTINOPLE:
        return 'C';
    }
    LLVM_BUILTIN_UNREACHABLE;
}

/// Combine code hash and EVM revision into a printable code identifier.
std::string makeCodeId(evmc_uint256be codeHash, evmc_revision rev, uint32_t flags)
{
    static const auto hexChars = "0123456789abcdef";
    std::string str;
    str.reserve(sizeof(codeHash) * 2 + 1);
    for (auto b : codeHash.bytes)
    {
        str.push_back(hexChars[b >> 4]);
        str.push_back(hexChars[b & 0xf]);
    }
    str.push_back(toChar(rev));
    if (flags & EVMC_STATIC)
        str.push_back('S');
    return str;
}

void printVersion()
{
    std::cout << "Ethereum EVM JIT Compiler (http://github.com/ethereum/evmjit):\n"
              << "  EVMJIT version " << EVMJIT_VERSION << "\n"
#ifdef NDEBUG
              << "  Optimized build, "
#else
              << "  DEBUG build, "
#endif
              << __DATE__ << " (" << __TIME__ << ")\n"
              << std::endl;
}

namespace cl = llvm::cl;
cl::opt<bool> g_optimize{"O", cl::desc{"Optimize"}};
cl::opt<CacheMode> g_cache{"cache", cl::desc{"Cache compiled EVM code on disk"},
    cl::values(clEnumValN(CacheMode::off, "0", "Disabled"),
        clEnumValN(CacheMode::on, "1", "Enabled"),
        clEnumValN(CacheMode::read, "r", "Read only. No new objects are added to cache."),
        clEnumValN(CacheMode::write, "w", "Write only. No objects are loaded from cache."),
        clEnumValN(CacheMode::clear, "c", "Clear the cache storage. Cache is disabled."),
        clEnumValN(CacheMode::preload, "p", "Preload all cached objects."))};
cl::opt<bool> g_stats{"st", cl::desc{"Statistics"}};
cl::opt<bool> g_dump{"dump", cl::desc{"Dump LLVM IR module"}};

void parseOptions()
{
    static llvm::llvm_shutdown_obj shutdownObj{};
    cl::AddExtraVersionPrinter(printVersion);
    cl::ParseEnvironmentOptions("evmjit", "EVMJIT", "Ethereum EVM JIT Compiler");
}

class SymbolResolver;

class JITImpl : public evmc_instance
{
    std::unique_ptr<llvm::ExecutionEngine> m_engine;
    SymbolResolver const* m_memoryMgr = nullptr;
    mutable std::mutex x_codeMap;
    std::unordered_map<std::string, CodeMapEntry> m_codeMap;

    std::mutex m_complite;

    static llvm::LLVMContext& getLLVMContext()
    {
        // TODO: This probably should be thread_local, but for now that causes
        // a crash when MCJIT is destroyed.
        static llvm::LLVMContext llvmContext;
        return llvmContext;
    }

    void createEngine();

public:
    static JITImpl& instance()
    {
        // We need to keep this a singleton.
        // so we only call changeVersion on it.
        static JITImpl s_instance;
        return s_instance;
    }

    JITImpl();

    void checkMemorySize();

    llvm::ExecutionEngine& engine() { return *m_engine; }

    CodeMapEntry getExecFunc(std::string const& _codeIdentifier);
    void mapExecFunc(std::string const& _codeIdentifier, ExecFunc _funcAddr);

    ExecFunc compile(evmc_revision _rev, bool _staticCall, byte const* _code, uint64_t _codeSize,
        std::string const& _codeIdentifier);

    evmc_context_fn_table const* host = nullptr;

    evmc_message const* currentMsg = nullptr;
    std::vector<uint8_t> returnBuffer;

    std::vector<uint8_t> codeBuffer;

    size_t hitThreshold = 0;
};

int64_t call(evmc_context* _ctx, int _kind, int64_t _gas, evmc_address const* _address,
    evmc_uint256be const* _value, uint8_t const* _inputData, size_t _inputSize,
    uint8_t* _outputData, size_t _outputSize, uint8_t const** o_bufData, size_t* o_bufSize) noexcept
{
    // FIXME: Handle unexpected exceptions.
    auto& jit = JITImpl::instance();

    evmc_message msg;
    msg.destination = *_address;
    msg.sender = _kind != EVMC_DELEGATECALL ? jit.currentMsg->destination : jit.currentMsg->sender;
    msg.value = _kind != EVMC_DELEGATECALL ? *_value : jit.currentMsg->value;
    msg.input_data = _inputData;
    msg.input_size = _inputSize;
    msg.gas = _gas;
    msg.depth = jit.currentMsg->depth + 1;
    msg.flags = jit.currentMsg->flags;
    if (_kind == EVM_STATICCALL)
    {
        msg.kind = EVMC_CALL;
        msg.flags |= EVMC_STATIC;
    }
    else
        msg.kind = static_cast<evmc_call_kind>(_kind);

    // FIXME: Handle code hash.
    evmc_result result;
    jit.host->call(&result, _ctx, &msg);
    // FIXME: Clarify when gas_left is valid.
    int64_t r = result.gas_left;

    // Handle output. It can contain data from RETURN or REVERT opcodes.
    auto size = std::min(_outputSize, result.output_size);
    std::copy_n(result.output_data, size, _outputData);

    // Update RETURNDATA buffer.
    // The buffer is already cleared.
    jit.returnBuffer = {result.output_data, result.output_data + result.output_size};
    *o_bufData = jit.returnBuffer.data();
    *o_bufSize = jit.returnBuffer.size();

    if (_kind == EVMC_CREATE && result.status_code == EVMC_SUCCESS)
        std::copy_n(result.create_address.bytes, sizeof(result.create_address), _outputData);

    if (result.status_code != EVMC_SUCCESS)
        r |= EVM_CALL_FAILURE;

    if (result.release)
        result.release(&result);
    return r;
}

void formatOutput(int64_t ter, uint8_t const** o_bufData, size_t* o_bufSize)
{
    auto& jit = JITImpl::instance();
    if (ter != 0)
    {
        // Update RETURNDATA buffer.
        // The buffer is already cleared.
        auto str = std::to_string(ter);
        jit.returnBuffer.resize(str.size());
        std::copy(str.begin(), str.end(), jit.returnBuffer.begin());
        *o_bufData = jit.returnBuffer.data();
        *o_bufSize = jit.returnBuffer.size();
    }
}

int64_t table_create(struct evmc_context* _context, const struct evmc_address* address,
    uint8_t const* _name, size_t _nameSize, uint8_t const* _raw, size_t _rawSize,
    uint8_t const** o_bufData, size_t* o_bufSize)
{
    auto& jit = JITImpl::instance();
    int64_t ter = jit.host->table_create(_context, address, _name, _nameSize, _raw, _rawSize);
    formatOutput(ter, o_bufData, o_bufSize);
    return ter;
}

int64_t table_rename(struct evmc_context* _context, const struct evmc_address* address,
    uint8_t const* _name, size_t _nameSize, uint8_t const* _raw, size_t _rawSize,
    uint8_t const** o_bufData, size_t* o_bufSize)
{
    auto& jit = JITImpl::instance();
    int64_t ter = jit.host->table_rename(_context, address, _name, _nameSize, _raw, _rawSize);
    formatOutput(ter, o_bufData, o_bufSize);
    return ter;
}

int64_t table_delete(struct evmc_context* _context, const struct evmc_address* address,
    uint8_t const* _name, size_t _nameSize, uint8_t const* _raw, size_t _rawSize,
    uint8_t const** o_bufData, size_t* o_bufSize)
{
    auto& jit = JITImpl::instance();
    int64_t ter = jit.host->table_delete(_context, address, _name, _nameSize, _raw, _rawSize);
    formatOutput(ter, o_bufData, o_bufSize);
    return ter;
}

int64_t table_insert(struct evmc_context* _context, const struct evmc_address* address,
    uint8_t const* _name, size_t _nameSize, uint8_t const* _raw, size_t _rawSize,
    uint8_t const** o_bufData, size_t* o_bufSize)
{
    auto& jit = JITImpl::instance();
    int64_t ter = jit.host->table_insert(_context, address, _name, _nameSize, _raw, _rawSize);
    formatOutput(ter, o_bufData, o_bufSize);
    return ter;
}

int64_t table_drop(struct evmc_context* _context, const struct evmc_address* address,
    uint8_t const* _name, size_t _nameSize, uint8_t const** o_bufData, size_t* o_bufSize)
{
    auto& jit = JITImpl::instance();
    int64_t ter = jit.host->table_drop(_context, address, _name, _nameSize);
    formatOutput(ter, o_bufData, o_bufSize);
    return ter;
}

int64_t table_update(struct evmc_context* _context, const struct evmc_address* address,
    uint8_t const* _name, size_t _nameSize, uint8_t const* _raw1, size_t _rawSize1,
    uint8_t const* _raw2, size_t _rawSize2, uint8_t const** o_bufData, size_t* o_bufSize)
{
    auto& jit = JITImpl::instance();
    int64_t ter = jit.host->table_update(
        _context, address, _name, _nameSize, _raw1, _rawSize1, _raw2, _rawSize2);
    formatOutput(ter, o_bufData, o_bufSize);
    return ter;
}

int64_t table_grant(struct evmc_context* _context, const struct evmc_address* address1,
    const struct evmc_address* address2, uint8_t const* _name, size_t _nameSize,
    uint8_t const* _row, size_t _rowSize, uint8_t const** o_bufData, size_t* o_bufSize)
{
    auto& jit = JITImpl::instance();
    int64_t ter =
        jit.host->table_grant(_context, address1, address2, _name, _nameSize, _row, _rowSize);
    formatOutput(ter, o_bufData, o_bufSize);
    return ter;
}


int64_t db_trans_submit(struct evmc_context* _context, uint8_t const** o_bufData, size_t* o_bufSize)
{
    auto& jit = JITImpl::instance();
    int64_t ter = jit.host->db_trans_submit(_context);
    formatOutput(ter, o_bufData, o_bufSize);
    return ter;
}

uint8_t* evm_realloc(uint8_t* data, size_t size)
{
    uint8_t* newData = (uint8_t*)std::realloc(data, size);
    return newData;
}

/// A wrapper for new EVM-C copycode callback function.
size_t getCode(uint8_t** o_pCode, evmc_context* _ctx, evmc_address const* _address) noexcept
{
    auto& jit = JITImpl::instance();
    size_t codeSize = jit.host->get_code_size(_ctx, _address);
    jit.codeBuffer.resize(codeSize);  // Allocate needed memory to store the full code.

    // Copy the code to JIT's buffer and send the buffer reference back to LLVM.
    size_t size =
        jit.host->copy_code(_ctx, _address, 0, jit.codeBuffer.data(), jit.codeBuffer.size());
    *o_pCode = jit.codeBuffer.data();
    return size;
}

class SymbolResolver : public llvm::SectionMemoryManager
{
    llvm::JITSymbol findSymbol(std::string const& _name) override
    {
        auto& jit = JITImpl::instance();

        // Handle symbols' global prefix.
        // If in current DataLayout global symbols are prefixed, drop the
        // prefix from the name for local search.
        char prefix = jit.engine().getDataLayout().getGlobalPrefix();
        llvm::StringRef unprefixedName = (prefix != '\0' && _name[0] == prefix) ?
                                             llvm::StringRef{_name}.drop_front() :
                                             llvm::StringRef{_name};

        auto addr =
            llvm::StringSwitch<uint64_t>(unprefixedName)
                .Case("env_sha3", reinterpret_cast<uint64_t>(&keccak))
                .Case("evm.exists", reinterpret_cast<uint64_t>(jit.host->account_exists))
                .Case("evm.sload", reinterpret_cast<uint64_t>(jit.host->get_storage))
                .Case("evm.sstore", reinterpret_cast<uint64_t>(jit.host->set_storage))
                .Case("evm.balance", reinterpret_cast<uint64_t>(jit.host->get_balance))
                .Case("evm.codesize", reinterpret_cast<uint64_t>(jit.host->get_code_size))
                .Case("evm.code", reinterpret_cast<uint64_t>(getCode))
                .Case("evm.selfdestruct", reinterpret_cast<uint64_t>(jit.host->selfdestruct))
                .Case("evm.call", reinterpret_cast<uint64_t>(call))
                .Case("evm.get_tx_context", reinterpret_cast<uint64_t>(jit.host->get_tx_context))
                .Case("evm.blockhash", reinterpret_cast<uint64_t>(jit.host->get_block_hash))
                .Case("evm.log", reinterpret_cast<uint64_t>(jit.host->emit_log))
                .Case("evm.realloc", reinterpret_cast<uint64_t>(evm_realloc))
                .Case("evm.table_create", reinterpret_cast<uint64_t>(table_create))
                .Case("evm.table_rename", reinterpret_cast<uint64_t>(table_rename))
                .Case("evm.table_insert", reinterpret_cast<uint64_t>(table_insert))
                .Case("evm.table_delete", reinterpret_cast<uint64_t>(table_delete))
                .Case("evm.table_drop", reinterpret_cast<uint64_t>(table_drop))
                .Case("evm.table_update", reinterpret_cast<uint64_t>(table_update))
                .Case("evm.table_grant", reinterpret_cast<uint64_t>(table_grant))
                .Case(
                    "evm.table_get_handle", reinterpret_cast<uint64_t>(jit.host->table_get_handle))
                .Case("evm.table_get_lines", reinterpret_cast<uint64_t>(jit.host->table_get_lines))
                .Case("evm.table_get_columns",
                    reinterpret_cast<uint64_t>(jit.host->table_get_columns))
                .Case("evm.get_column_by_name",
                    reinterpret_cast<uint64_t>(jit.host->get_column_by_name))
                .Case("evm.get_column_by_index",
                    reinterpret_cast<uint64_t>(jit.host->get_column_by_index))
                .Case("evm.db_trans_begin", reinterpret_cast<uint64_t>(jit.host->db_trans_begin))
                .Case("evm.db_trans_submit", reinterpret_cast<uint64_t>(db_trans_submit))
                .Case("evm.exit_fun", reinterpret_cast<uint64_t>(jit.host->exit_fun))
                .Case("evm.get_column_len_by_name",
                    reinterpret_cast<uint64_t>(jit.host->get_column_len_by_name))
                .Case("evm.get_column_len_by_index",
                    reinterpret_cast<uint64_t>(jit.host->get_column_len_by_index))
                .Default(0);
        if (addr)
            return {addr, llvm::JITSymbolFlags::Exported};

        // Fallback to default implementation that would search for the symbol
        // in the current process. Use the original prefixed symbol name.
        // TODO: In the future we should control the whole set of requested
        //       symbols (like memcpy, memset, etc) to improve performance.
        return llvm::SectionMemoryManager::findSymbol(_name);
    }

    void reportMemorySize(size_t _addedSize)
    {
        m_totalMemorySize += _addedSize;

        if (!g_stats)
            return;

        if (m_totalMemorySize >= m_printMemoryLimit)
        {
            constexpr size_t printMemoryStep = 10 * 1024 * 1024;
            auto value = double(m_totalMemorySize) / printMemoryStep;
            std::cerr << "EVMJIT total memory size: " << (10 * value) << " MB\n";
            m_printMemoryLimit += printMemoryStep;
        }
    }

    uint8_t* allocateCodeSection(
        uintptr_t _size, unsigned _a, unsigned _id, llvm::StringRef _name) override
    {
        reportMemorySize(_size);
        return llvm::SectionMemoryManager::allocateCodeSection(_size, _a, _id, _name);
    }

    uint8_t* allocateDataSection(
        uintptr_t _size, unsigned _a, unsigned _id, llvm::StringRef _name, bool _ro) override
    {
        reportMemorySize(_size);
        return llvm::SectionMemoryManager::allocateDataSection(_size, _a, _id, _name, _ro);
    }

    size_t m_totalMemorySize = 0;
    size_t m_printMemoryLimit = 1024 * 1024;

public:
    size_t totalMemorySize() const { return m_totalMemorySize; }
};


CodeMapEntry JITImpl::getExecFunc(std::string const& _codeIdentifier)
{
    std::lock_guard<std::mutex> lock{x_codeMap};
    auto& entry = m_codeMap[_codeIdentifier];
    ++entry.hits;
    return entry;
}

void JITImpl::mapExecFunc(std::string const& _codeIdentifier, ExecFunc _funcAddr)
{
    std::lock_guard<std::mutex> lock{x_codeMap};
    m_codeMap[_codeIdentifier].func = _funcAddr;
}

ExecFunc JITImpl::compile(evmc_revision _rev, bool _staticCall, byte const* _code,
    uint64_t _codeSize, std::string const& _codeIdentifier)
{
    std::lock_guard<std::mutex> lock(m_complite);
    auto module = Cache::getObject(_codeIdentifier, getLLVMContext());
    if (!module)
    {
        // TODO: Listener support must be redesigned. These should be a feature of JITImpl
        // listener->stateChanged(ExecState::Compilation);
        assert(_code || !_codeSize);
        // TODO: Can the Compiler be stateless?
        module = Compiler({}, _rev, _staticCall, getLLVMContext())
                     .compile(_code, _code + _codeSize, _codeIdentifier);

        if (g_optimize)
        {
            // listener->stateChanged(ExecState::Optimization);
            optimize(*module);
        }

        prepare(*module);
    }

    if (g_dump)
    {
        llvm::raw_os_ostream cerr{std::cerr};
        module->print(cerr, nullptr);
    }


    m_engine->addModule(std::move(module));
    // listener->stateChanged(ExecState::CodeGen);
    return (ExecFunc)m_engine->getFunctionAddress(_codeIdentifier);
}

}  // anonymous namespace


ExecutionContext::~ExecutionContext() noexcept
{
    if (m_memData)
        std::free(m_memData);
}

bytes_ref ExecutionContext::getReturnData() const
{
    auto data = m_data->callData;
    auto size = static_cast<size_t>(m_data->callDataSize);

    if (data < m_memData || data >= m_memData + m_memSize || size == 0)
    {
        assert(size == 0);  // data can be an invalid pointer only if size is 0
        m_data->callData = nullptr;
        return {};
    }

    return bytes_ref{data, size};
}

extern "C" {

EXPORT evmc_instance* evmjit_create()
{
    // Let's always return the same instance. It's a bit of faking, but actually
    // this might be a compliant implementation.
    return &JITImpl::instance();
}

static void destroy(evmc_instance* instance)
{
    (void)instance;
    assert(instance == static_cast<void*>(&JITImpl::instance()));
}

static evmc_result execute(evmc_instance* instance, evmc_context* context, evmc_revision rev,
    evmc_message const* msg, uint8_t const* code, size_t code_size)
{
    auto& jit = *reinterpret_cast<JITImpl*>(instance);

    if (msg->depth == 0)
        jit.checkMemorySize();

    if (!jit.host)
        jit.host = context->fn_table;
    assert(jit.host == context->fn_table);  // Require the fn_table not to change.

    // TODO: Temporary keep track of the current message.
    evmc_message const* prevMsg = jit.currentMsg;
    jit.currentMsg = msg;

    RuntimeData rt;
    rt.code = code;
    rt.codeSize = code_size;
    rt.gas = msg->gas;
    rt.callData = msg->input_data;
    rt.callDataSize = msg->input_size;
    std::memcpy(&rt.apparentValue, &msg->value, sizeof(msg->value));
    std::memset(&rt.address, 0, 12);
    std::memcpy(&rt.address[12], &msg->destination, sizeof(msg->destination));
    std::memset(&rt.caller, 0, 12);
    std::memcpy(&rt.caller[12], &msg->sender, sizeof(msg->sender));
    rt.depth = msg->depth;

    ExecutionContext ctx{rt, context};

    evmc_result result;
    result.status_code = EVMC_SUCCESS;
    result.gas_left = 0;
    result.output_data = nullptr;
    result.output_size = 0;
    result.release = nullptr;

    auto codeIdentifier = makeCodeId(msg->code_hash, rev, msg->flags);
    auto codeEntry = jit.getExecFunc(codeIdentifier);
    auto func = codeEntry.func;
    if (!func)
    {
        // FIXME: We have a race condition here!

        if (codeEntry.hits <= jit.hitThreshold)
        {
            result.status_code = EVMC_REJECTED;
            return result;
        }

        if (g_stats)
            std::cerr << "EVMJIT Compile " << codeIdentifier << " (" << codeEntry.hits << ")\n";

        const bool staticCall = (msg->flags & EVMC_STATIC) != 0;
        func = jit.compile(rev, staticCall, ctx.code(), ctx.codeSize(), codeIdentifier);
        if (!func)
        {
            result.status_code = EVMC_INTERNAL_ERROR;
            return result;
        }
        jit.mapExecFunc(codeIdentifier, func);
    }

    auto returnCode = func(&ctx);

    if (returnCode == ReturnCode::Revert)
    {
        result.status_code = EVMC_REVERT;
        result.gas_left = rt.gas;
    }
    else if (returnCode == ReturnCode::RevertDiy)
    {
        result.status_code = EVMC_REVERTDIY;
        result.gas_left = rt.gas;
    }
    else if (returnCode == ReturnCode::OutOfGas)
    {
        // EVMJIT does not provide information what exactly type of failure
        // it was, so use generic EVM_FAILURE.
        result.status_code = EVMC_FAILURE;
    }
    else
    {
        // In case of success return the amount of gas left.
        result.gas_left = rt.gas;
    }

    if (returnCode == ReturnCode::Return || returnCode == ReturnCode::Revert ||
        returnCode == ReturnCode::RevertDiy)
    {
        auto out = ctx.getReturnData();
        result.output_data = std::get<0>(out);
        result.output_size = std::get<1>(out);
    }

    // Take care of the internal memory.
    if (ctx.m_memData)
    {
        // Use result's reserved data to store the memory pointer.

        evmc_get_optional_data(&result)->pointer = ctx.m_memData;

        // Set pointer to the destructor that will release the memory.
        result.release = [](evmc_result const* r) {
            std::free(evmc_get_const_optional_data(r)->pointer);
        };
        ctx.m_memData = nullptr;
    }

    jit.currentMsg = prevMsg;
    return result;
}

static int setOption(evmc_instance* instance, const char* name, const char* value) noexcept
{
    try
    {
        if (name == std::string{"hits-threshold"})
        {
            auto& jit = static_cast<JITImpl&>(*instance);
            jit.hitThreshold = std::stoul(value);
            return 1;
        }
        return 0;
    }
    catch (...)
    {
        return 0;
    }
}

}  // extern "C"

void JITImpl::createEngine()
{
    auto module = llvm::make_unique<llvm::Module>("", getLLVMContext());

    // FIXME: LLVM 3.7: test on Windows
    auto triple = llvm::Triple(llvm::sys::getProcessTriple());
    if (triple.getOS() == llvm::Triple::OSType::Win32)
        triple.setObjectFormat(llvm::Triple::ObjectFormatType::ELF);  // MCJIT does not support COFF
                                                                      // format
    module->setTargetTriple(triple.str());

    llvm::EngineBuilder builder(std::move(module));
    builder.setEngineKind(llvm::EngineKind::JIT);
    auto memoryMgr = llvm::make_unique<SymbolResolver>();
    m_memoryMgr = memoryMgr.get();
    builder.setMCJITMemoryManager(std::move(memoryMgr));
    builder.setOptLevel(g_optimize ? llvm::CodeGenOpt::Default : llvm::CodeGenOpt::None);
#ifndef NDEBUG
    builder.setVerifyModules(true);
#endif

    m_engine.reset(builder.create());

    // TODO: Update cache listener
    m_engine->setObjectCache(Cache::init(g_cache, nullptr));

    // FIXME: Disabled during API changes
    // if (preloadCache)
    //	Cache::preload(*m_engine, funcCache);
}

JITImpl::JITImpl()
  : evmc_instance({
        EVMC_ABI_VERSION,
        "evmjit",
        EVMJIT_VERSION,
        evmjit::destroy,
        evmjit::execute,
        evmjit::setOption,
    })
{
    parseOptions();

    bool preloadCache = g_cache == CacheMode::preload;
    if (preloadCache)
        g_cache = CacheMode::on;

    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();

    createEngine();
}

void JITImpl::checkMemorySize()
{
    constexpr size_t memoryLimit = 1000 * 1024 * 1024;

    if (m_memoryMgr->totalMemorySize() > memoryLimit)
    {
        if (g_stats)
            std::cerr << "EVMJIT reset!\n";

        std::lock_guard<std::mutex> lock{x_codeMap};
        m_codeMap.clear();
        m_engine.reset();
        createEngine();
    }
}

}  // namespace evmjit
}  // namespace dev
