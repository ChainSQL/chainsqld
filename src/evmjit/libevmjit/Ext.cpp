#include <iostream>
#include <string>

#include "Ext.h"

#include "preprocessor/llvm_includes_end.h"
#include "preprocessor/llvm_includes_start.h"
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_ostream.h>

#include "Endianness.h"
#include "Memory.h"
#include "RuntimeManager.h"
#include "Type.h"

namespace dev
{
namespace eth
{
namespace jit
{
Ext::Ext(RuntimeManager& _runtimeManager, Memory& _memoryMan)
  : RuntimeHelper(_runtimeManager), m_memoryMan(_memoryMan)
{
    m_funcs = decltype(m_funcs)();
    m_argAllocas = decltype(m_argAllocas)();
    m_size = m_builder.CreateAlloca(Type::Size, nullptr, "env.size");
}

namespace
{
using FuncDesc = std::tuple<char const*, llvm::FunctionType*>;

llvm::FunctionType* getFunctionType(
    llvm::Type* _returnType, std::initializer_list<llvm::Type*> const& _argsTypes)
{
    return llvm::FunctionType::get(
        _returnType, llvm::ArrayRef<llvm::Type*>{_argsTypes.begin(), _argsTypes.size()}, false);
}

std::array<FuncDesc, sizeOf<EnvFunc>::value> const& getEnvFuncDescs()
{
    static std::array<FuncDesc, sizeOf<EnvFunc>::value> descs{{
        FuncDesc{
            "env_sload", getFunctionType(Type::Void, {Type::EnvPtr, Type::WordPtr, Type::WordPtr})},
        FuncDesc{"env_sstore",
            getFunctionType(Type::Void, {Type::EnvPtr, Type::WordPtr, Type::WordPtr})},
        FuncDesc{
            "env_sha3", getFunctionType(Type::Void, {Type::BytePtr, Type::Size, Type::WordPtr})},
        FuncDesc{"env_balance",
            getFunctionType(Type::Void, {Type::WordPtr, Type::EnvPtr, Type::WordPtr})},
        FuncDesc{"env_create",
            getFunctionType(Type::Void, {Type::EnvPtr, Type::GasPtr, Type::WordPtr, Type::BytePtr,
                                            Type::Size, Type::WordPtr})},
        FuncDesc{"env_call", getFunctionType(Type::Bool,
                                 {Type::EnvPtr, Type::GasPtr, Type::Gas, Type::WordPtr,
                                     Type::WordPtr, Type::WordPtr, Type::WordPtr, Type::WordPtr,
                                     Type::BytePtr, Type::Size, Type::BytePtr, Type::Size})},
        FuncDesc{"env_log",
            getFunctionType(Type::Void, {Type::EnvPtr, Type::BytePtr, Type::Size, Type::WordPtr,
                                            Type::WordPtr, Type::WordPtr, Type::WordPtr})},
        FuncDesc{"env_executeSQL",
            getFunctionType(Type::Size, {Type::EnvPtr, Type::WordPtr, Type::Byte, Type::BytePtr,
                                            Type::Size, Type::BytePtr, Type::Size})},
        FuncDesc{"env_blockhash",
            getFunctionType(Type::Void, {Type::EnvPtr, Type::WordPtr, Type::WordPtr})},
        FuncDesc{"env_extcode", getFunctionType(Type::BytePtr,
                                    {Type::EnvPtr, Type::WordPtr, Type::Size->getPointerTo()})},
    }};

    return descs;
}

llvm::Function* createFunc(EnvFunc _id, llvm::Module* _module)
{
    auto&& desc = getEnvFuncDescs()[static_cast<size_t>(_id)];
    return llvm::Function::Create(
        std::get<1>(desc), llvm::Function::ExternalLinkage, std::get<0>(desc), _module);
}

llvm::Function* getAccountExistsFunc(llvm::Module* _module)
{
    static const auto funcName = "evm.exists";
    auto func = _module->getFunction(funcName);
    if (!func)
    {
        // TODO: Mark the function as pure to eliminate multiple calls.
        auto i32 = llvm::IntegerType::getInt32Ty(_module->getContext());
        auto addrTy = llvm::IntegerType::get(_module->getContext(), 160);
        auto fty = llvm::FunctionType::get(i32, {Type::EnvPtr, addrTy->getPointerTo()}, false);
        func = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, funcName, _module);
    }
    return func;
}

llvm::Function* getGetStorageFunc(llvm::Module* _module)
{
    static const auto funcName = "evm.sload";
    auto func = _module->getFunction(funcName);
    if (!func)
    {
        // TODO: Mark the function as pure to eliminate multiple calls.
        auto addrTy = llvm::IntegerType::get(_module->getContext(), 160);
        auto fty = llvm::FunctionType::get(Type::Void,
            {Type::WordPtr, Type::EnvPtr, addrTy->getPointerTo(), Type::WordPtr}, false);
        func = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, funcName, _module);
        func->addAttribute(1, llvm::Attribute::NoAlias);
        func->addAttribute(1, llvm::Attribute::NoCapture);
        func->addAttribute(3, llvm::Attribute::ReadOnly);
        func->addAttribute(3, llvm::Attribute::NoAlias);
        func->addAttribute(3, llvm::Attribute::NoCapture);
        func->addAttribute(4, llvm::Attribute::ReadOnly);
        func->addAttribute(4, llvm::Attribute::NoAlias);
        func->addAttribute(4, llvm::Attribute::NoCapture);
    }
    return func;
}

llvm::Function* getSetStorageFunc(llvm::Module* _module)
{
    static const auto funcName = "evm.sstore";
    auto func = _module->getFunction(funcName);
    if (!func)
    {
        auto addrPtrTy = llvm::Type::getIntNPtrTy(_module->getContext(), 160);
        auto fty = llvm::FunctionType::get(
            Type::Void, {Type::EnvPtr, addrPtrTy, Type::WordPtr, Type::WordPtr}, false);
        func = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, funcName, _module);
        func->addAttribute(2, llvm::Attribute::ReadOnly);
        func->addAttribute(2, llvm::Attribute::NoAlias);
        func->addAttribute(2, llvm::Attribute::NoCapture);
        func->addAttribute(3, llvm::Attribute::ReadOnly);
        func->addAttribute(3, llvm::Attribute::NoAlias);
        func->addAttribute(3, llvm::Attribute::NoCapture);
    }
    return func;
}

llvm::Function* getGetBalanceFunc(llvm::Module* _module)
{
    static const auto funcName = "evm.balance";
    auto func = _module->getFunction(funcName);
    if (!func)
    {
        auto addrTy = llvm::IntegerType::get(_module->getContext(), 160);
        auto fty = llvm::FunctionType::get(
            Type::Void, {Type::WordPtr, Type::EnvPtr, addrTy->getPointerTo()}, false);
        func = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, funcName, _module);
        func->addAttribute(1, llvm::Attribute::NoAlias);
        func->addAttribute(1, llvm::Attribute::NoCapture);
        func->addAttribute(3, llvm::Attribute::ReadOnly);
        func->addAttribute(3, llvm::Attribute::NoAlias);
        func->addAttribute(3, llvm::Attribute::NoCapture);
    }
    return func;
}

llvm::Function* getGetCodeSizeFunc(llvm::Module* _module)
{
    static const auto funcName = "evm.codesize";
    auto func = _module->getFunction(funcName);
    if (!func)
    {
        auto addrTy = llvm::IntegerType::get(_module->getContext(), 160);
        auto fty =
            llvm::FunctionType::get(Type::Size, {Type::EnvPtr, addrTy->getPointerTo()}, false);
        func = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, funcName, _module);
        func->addAttribute(2, llvm::Attribute::ReadOnly);
        func->addAttribute(2, llvm::Attribute::NoAlias);
        func->addAttribute(2, llvm::Attribute::NoCapture);
    }
    return func;
}

llvm::Function* getGetCodeFunc(llvm::Module* _module)
{
    static const auto funcName = "evm.code";
    auto func = _module->getFunction(funcName);
    if (!func)
    {
        auto addrTy = llvm::IntegerType::get(_module->getContext(), 160);
        auto fty = llvm::FunctionType::get(Type::Size,
            {Type::BytePtr->getPointerTo(), Type::EnvPtr, addrTy->getPointerTo()}, false);
        func = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, funcName, _module);
        func->addAttribute(1, llvm::Attribute::NoAlias);
        func->addAttribute(1, llvm::Attribute::NoCapture);
        func->addAttribute(3, llvm::Attribute::ReadOnly);
        func->addAttribute(3, llvm::Attribute::NoAlias);
        func->addAttribute(3, llvm::Attribute::NoCapture);
    }
    return func;
}

llvm::Function* getSelfdestructFunc(llvm::Module* _module)
{
    static const auto funcName = "evm.selfdestruct";
    auto func = _module->getFunction(funcName);
    if (!func)
    {
        auto addrPtrTy = llvm::Type::getIntNPtrTy(_module->getContext(), 160);
        auto fty = llvm::FunctionType::get(Type::Void, {Type::EnvPtr, addrPtrTy, addrPtrTy}, false);
        func = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, funcName, _module);
        func->addAttribute(2, llvm::Attribute::ReadOnly);
        func->addAttribute(2, llvm::Attribute::NoAlias);
        func->addAttribute(2, llvm::Attribute::NoCapture);
        func->addAttribute(3, llvm::Attribute::ReadOnly);
        func->addAttribute(3, llvm::Attribute::NoAlias);
        func->addAttribute(3, llvm::Attribute::NoCapture);
    }
    return func;
}

llvm::Function* getLogFunc(llvm::Module* _module)
{
    static const auto funcName = "evm.log";
    auto func = _module->getFunction(funcName);
    if (!func)
    {
        auto addrPtrTy = llvm::Type::getIntNPtrTy(_module->getContext(), 160);
        auto fty = llvm::FunctionType::get(Type::Void,
            {Type::EnvPtr, addrPtrTy, Type::BytePtr, Type::Size, Type::WordPtr, Type::Size}, false);
        func = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, funcName, _module);
        func->addAttribute(3, llvm::Attribute::ReadOnly);
        func->addAttribute(3, llvm::Attribute::NoAlias);
        func->addAttribute(3, llvm::Attribute::NoCapture);
        func->addAttribute(5, llvm::Attribute::ReadOnly);
        func->addAttribute(5, llvm::Attribute::NoAlias);
        func->addAttribute(5, llvm::Attribute::NoCapture);
    }
    return func;
}

llvm::Function* getExecuteSQLFunc(llvm::Module* _module)
{
    static const auto funcName = "evm.executeSQL";
    auto func = _module->getFunction(funcName);
    if (!func)
    {
        auto addrTy = llvm::IntegerType::get(_module->getContext(), 160);
        auto fty = llvm::FunctionType::get(Type::Size,
            {Type::EnvPtr, addrTy->getPointerTo(), Type::Byte, Type::BytePtr, Type::Size,
                Type::BytePtr, Type::Size},
            false);
        func = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, funcName, _module);

        func->addAttribute(2, llvm::Attribute::ReadOnly);
        func->addAttribute(2, llvm::Attribute::NoAlias);
        func->addAttribute(2, llvm::Attribute::NoCapture);
        func->addAttribute(4, llvm::Attribute::ReadOnly);
        func->addAttribute(4, llvm::Attribute::NoAlias);
        func->addAttribute(4, llvm::Attribute::NoCapture);
        func->addAttribute(6, llvm::Attribute::ReadOnly);
        func->addAttribute(6, llvm::Attribute::NoAlias);
        func->addAttribute(6, llvm::Attribute::NoCapture);
    }
    return func;
}

llvm::Function* getTableCreateFunc(llvm::Module* _module)
{
    static const auto funcName = "evm.table_create";
    auto func = _module->getFunction(funcName);
    if (!func)
    {
        auto addrTy = llvm::IntegerType::get(_module->getContext(), 160);
        auto i64 = llvm::IntegerType::getInt64Ty(_module->getContext());
        auto fty = llvm::FunctionType::get(i64,
            {Type::EnvPtr, addrTy->getPointerTo(), Type::BytePtr, Type::Size, Type::BytePtr,
                Type::Size, Type::BytePtr->getPointerTo(), Type::Size->getPointerTo()},
            false);
        func = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, funcName, _module);

        func->addAttribute(2, llvm::Attribute::ReadOnly);
        func->addAttribute(2, llvm::Attribute::NoAlias);
        func->addAttribute(2, llvm::Attribute::NoCapture);
        func->addAttribute(3, llvm::Attribute::ReadOnly);
        func->addAttribute(3, llvm::Attribute::NoAlias);
        func->addAttribute(3, llvm::Attribute::NoCapture);
        func->addAttribute(5, llvm::Attribute::ReadOnly);
        func->addAttribute(5, llvm::Attribute::NoAlias);
        func->addAttribute(5, llvm::Attribute::NoCapture);
    }
    return func;
}

llvm::Function* getTableRenameFunc(llvm::Module* _module)
{
    static const auto funcName = "evm.table_rename";
    auto func = _module->getFunction(funcName);
    if (!func)
    {
        auto addrTy = llvm::IntegerType::get(_module->getContext(), 160);
        auto i64 = llvm::IntegerType::getInt64Ty(_module->getContext());
        auto fty = llvm::FunctionType::get(i64,
            {Type::EnvPtr, addrTy->getPointerTo(), Type::BytePtr, Type::Size, Type::BytePtr,
                Type::Size, Type::BytePtr->getPointerTo(), Type::Size->getPointerTo()},
            false);
        func = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, funcName, _module);

        func->addAttribute(2, llvm::Attribute::ReadOnly);
        func->addAttribute(2, llvm::Attribute::NoAlias);
        func->addAttribute(2, llvm::Attribute::NoCapture);
        func->addAttribute(3, llvm::Attribute::ReadOnly);
        func->addAttribute(3, llvm::Attribute::NoAlias);
        func->addAttribute(3, llvm::Attribute::NoCapture);
        func->addAttribute(5, llvm::Attribute::ReadOnly);
        func->addAttribute(5, llvm::Attribute::NoAlias);
        func->addAttribute(5, llvm::Attribute::NoCapture);
    }
    return func;
}
llvm::Function* getTableInsertFunc(llvm::Module* _module)
{
    static const auto funcName = "evm.table_insert";
    auto func = _module->getFunction(funcName);
    if (!func)
    {
        auto addrTy = llvm::IntegerType::get(_module->getContext(), 160);
        auto i64 = llvm::IntegerType::getInt64Ty(_module->getContext());
        auto fty = llvm::FunctionType::get(i64,
            {Type::EnvPtr, addrTy->getPointerTo(), Type::BytePtr, Type::Size, Type::BytePtr,
                Type::Size, Type::BytePtr->getPointerTo(), Type::Size->getPointerTo()},
            false);
        func = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, funcName, _module);

        func->addAttribute(2, llvm::Attribute::ReadOnly);
        func->addAttribute(2, llvm::Attribute::NoAlias);
        func->addAttribute(2, llvm::Attribute::NoCapture);
        func->addAttribute(3, llvm::Attribute::ReadOnly);
        func->addAttribute(3, llvm::Attribute::NoAlias);
        func->addAttribute(3, llvm::Attribute::NoCapture);
        func->addAttribute(5, llvm::Attribute::ReadOnly);
        func->addAttribute(5, llvm::Attribute::NoAlias);
        func->addAttribute(5, llvm::Attribute::NoCapture);
    }
    return func;
}
llvm::Function* getTableDeleteFunc(llvm::Module* _module)
{
    static const auto funcName = "evm.table_delete";
    auto func = _module->getFunction(funcName);
    if (!func)
    {
        auto addrTy = llvm::IntegerType::get(_module->getContext(), 160);
        auto i64 = llvm::IntegerType::getInt64Ty(_module->getContext());
        auto fty = llvm::FunctionType::get(i64,
            {Type::EnvPtr, addrTy->getPointerTo(), Type::BytePtr, Type::Size, Type::BytePtr,
                Type::Size, Type::BytePtr->getPointerTo(), Type::Size->getPointerTo()},
            false);
        func = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, funcName, _module);

        func->addAttribute(2, llvm::Attribute::ReadOnly);
        func->addAttribute(2, llvm::Attribute::NoAlias);
        func->addAttribute(2, llvm::Attribute::NoCapture);
        func->addAttribute(3, llvm::Attribute::ReadOnly);
        func->addAttribute(3, llvm::Attribute::NoAlias);
        func->addAttribute(3, llvm::Attribute::NoCapture);
        func->addAttribute(5, llvm::Attribute::ReadOnly);
        func->addAttribute(5, llvm::Attribute::NoAlias);
        func->addAttribute(5, llvm::Attribute::NoCapture);
    }
    return func;
}
llvm::Function* getTableDropFunc(llvm::Module* _module)
{
    static const auto funcName = "evm.table_drop";
    auto func = _module->getFunction(funcName);
    if (!func)
    {
        auto addrTy = llvm::IntegerType::get(_module->getContext(), 160);
        auto i64 = llvm::IntegerType::getInt64Ty(_module->getContext());
        auto fty = llvm::FunctionType::get(i64,
            {Type::EnvPtr, addrTy->getPointerTo(), Type::BytePtr, Type::Size,
                Type::BytePtr->getPointerTo(), Type::Size->getPointerTo()},
            false);
        func = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, funcName, _module);

        func->addAttribute(2, llvm::Attribute::ReadOnly);
        func->addAttribute(2, llvm::Attribute::NoAlias);
        func->addAttribute(2, llvm::Attribute::NoCapture);
        func->addAttribute(3, llvm::Attribute::ReadOnly);
        func->addAttribute(3, llvm::Attribute::NoAlias);
        func->addAttribute(3, llvm::Attribute::NoCapture);
    }
    return func;
}
llvm::Function* getTableUpdateFunc(llvm::Module* _module)
{
    static const auto funcName = "evm.table_update";
    auto func = _module->getFunction(funcName);
    if (!func)
    {
        auto addrTy = llvm::IntegerType::get(_module->getContext(), 160);
        auto i64 = llvm::IntegerType::getInt64Ty(_module->getContext());
        auto fty = llvm::FunctionType::get(i64,
            {Type::EnvPtr, addrTy->getPointerTo(), Type::BytePtr, Type::Size, Type::BytePtr,
                Type::Size, Type::BytePtr, Type::Size, Type::BytePtr->getPointerTo(),
                Type::Size->getPointerTo()},
            false);
        func = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, funcName, _module);

        func->addAttribute(2, llvm::Attribute::ReadOnly);
        func->addAttribute(2, llvm::Attribute::NoAlias);
        func->addAttribute(2, llvm::Attribute::NoCapture);
        func->addAttribute(3, llvm::Attribute::ReadOnly);
        func->addAttribute(3, llvm::Attribute::NoAlias);
        func->addAttribute(3, llvm::Attribute::NoCapture);
        func->addAttribute(5, llvm::Attribute::ReadOnly);
        func->addAttribute(5, llvm::Attribute::NoAlias);
        func->addAttribute(5, llvm::Attribute::NoCapture);
        func->addAttribute(7, llvm::Attribute::ReadOnly);
        func->addAttribute(7, llvm::Attribute::NoAlias);
        func->addAttribute(7, llvm::Attribute::NoCapture);
    }
    return func;
}
llvm::Function* getTableGrantFunc(llvm::Module* _module)
{
    static const auto funcName = "evm.table_grant";
    auto func = _module->getFunction(funcName);
    if (!func)
    {
        auto addrTy1 = llvm::IntegerType::get(_module->getContext(), 160);
        auto i64 = llvm::IntegerType::getInt64Ty(_module->getContext());
        auto fty = llvm::FunctionType::get(i64,
            {Type::EnvPtr, addrTy1->getPointerTo(), addrTy1->getPointerTo(), Type::BytePtr,
                Type::Size, Type::BytePtr, Type::Size, Type::BytePtr->getPointerTo(),
                Type::Size->getPointerTo()},
            false);
        func = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, funcName, _module);

        func->addAttribute(2, llvm::Attribute::ReadOnly);
        func->addAttribute(2, llvm::Attribute::NoAlias);
        func->addAttribute(2, llvm::Attribute::NoCapture);
        func->addAttribute(3, llvm::Attribute::ReadOnly);
        func->addAttribute(3, llvm::Attribute::NoAlias);
        func->addAttribute(3, llvm::Attribute::NoCapture);
        func->addAttribute(4, llvm::Attribute::ReadOnly);
        func->addAttribute(4, llvm::Attribute::NoAlias);
        func->addAttribute(4, llvm::Attribute::NoCapture);
        func->addAttribute(6, llvm::Attribute::ReadOnly);
        func->addAttribute(6, llvm::Attribute::NoAlias);
        func->addAttribute(6, llvm::Attribute::NoCapture);
    }
    return func;
}
llvm::Function* getTableGetHandleFunc(llvm::Module* _module)
{
    static const auto funcName = "evm.table_get_handle";
    auto func = _module->getFunction(funcName);
    if (!func)
    {
        auto addrTy = llvm::IntegerType::get(_module->getContext(), 160);
        auto fty = llvm::FunctionType::get(Type::Void,
            {Type::EnvPtr, addrTy->getPointerTo(), Type::BytePtr, Type::Size, Type::BytePtr,
                Type::Size, Type::WordPtr},
            false);
        func = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, funcName, _module);

        func->addAttribute(2, llvm::Attribute::ReadOnly);
        func->addAttribute(2, llvm::Attribute::NoAlias);
        func->addAttribute(2, llvm::Attribute::NoCapture);
        func->addAttribute(3, llvm::Attribute::ReadOnly);
        func->addAttribute(3, llvm::Attribute::NoAlias);
        func->addAttribute(3, llvm::Attribute::NoCapture);
        func->addAttribute(5, llvm::Attribute::ReadOnly);
        func->addAttribute(5, llvm::Attribute::NoAlias);
        func->addAttribute(5, llvm::Attribute::NoCapture);
    }
    return func;
}
llvm::Function* getTableGetLinesFunc(llvm::Module* _module)
{
    static const auto funcName = "evm.table_get_lines";
    auto func = _module->getFunction(funcName);
    if (!func)
    {
        auto fty = llvm::FunctionType::get(
            Type::Void, {Type::EnvPtr, Type::WordPtr, Type::WordPtr}, false);
        func = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, funcName, _module);

        func->addAttribute(2, llvm::Attribute::ReadOnly);
        func->addAttribute(2, llvm::Attribute::NoAlias);
        func->addAttribute(2, llvm::Attribute::NoCapture);
    }
    return func;
}
llvm::Function* getTableGetColumnsFunc(llvm::Module* _module)
{
    static const auto funcName = "evm.table_get_columns";
    auto func = _module->getFunction(funcName);
    if (!func)
    {
        auto fty = llvm::FunctionType::get(
            Type::Void, {Type::EnvPtr, Type::WordPtr, Type::WordPtr}, false);
        func = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, funcName, _module);

        func->addAttribute(2, llvm::Attribute::ReadOnly);
        func->addAttribute(2, llvm::Attribute::NoAlias);
        func->addAttribute(2, llvm::Attribute::NoCapture);
    }
    return func;
}

static llvm::Function* getColumnLenByNameFunc(llvm::Module* _module)
{
    static auto funcName = "evm.get_column_len_by_name";
    auto func = _module->getFunction(funcName);
    if (!func)
    {
        auto fty = llvm::FunctionType::get(Type::Size,
            {Type::EnvPtr, Type::WordPtr, Type::Size, Type::BytePtr, Type::Size, Type::WordPtr},
            false);
        func = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, funcName, _module);

        func->addAttribute(2, llvm::Attribute::ReadOnly);
        func->addAttribute(2, llvm::Attribute::NoAlias);
        func->addAttribute(2, llvm::Attribute::NoCapture);
        func->addAttribute(4, llvm::Attribute::ReadOnly);
        func->addAttribute(4, llvm::Attribute::NoAlias);
        func->addAttribute(4, llvm::Attribute::NoCapture);
    }
    return func;
}

llvm::Function* getColumnByNameFunc(llvm::Module* _module)
{
    static const auto funcName = "evm.get_column_by_name";
    auto func = _module->getFunction(funcName);
    if (!func)
    {
        auto fty = llvm::FunctionType::get(Type::Size,
            {Type::EnvPtr, Type::WordPtr, Type::Size, Type::BytePtr, Type::Size, Type::BytePtr,
                Type::Size},
            false);
        func = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, funcName, _module);

        func->addAttribute(2, llvm::Attribute::ReadOnly);
        func->addAttribute(2, llvm::Attribute::NoAlias);
        func->addAttribute(2, llvm::Attribute::NoCapture);
        func->addAttribute(4, llvm::Attribute::ReadOnly);
        func->addAttribute(4, llvm::Attribute::NoAlias);
        func->addAttribute(4, llvm::Attribute::NoCapture);
    }
    return func;
}

static llvm::Function* getColumnLenByIndexFunc(llvm::Module* _module)
{
    static const auto funcName = "evm.get_column_len_by_index";
    auto func = _module->getFunction(funcName);
    if (!func)
    {
        auto fty = llvm::FunctionType::get(Type::Size,
            {Type::EnvPtr, Type::WordPtr, Type::Size, Type::Size, Type::WordPtr}, false);
        func = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, funcName, _module);
    }
    return func;
}

llvm::Function* getColumnByIndexFunc(llvm::Module* _module)
{
    static const auto funcName = "evm.get_column_by_index";
    auto func = _module->getFunction(funcName);
    if (!func)
    {
        auto fty = llvm::FunctionType::get(Type::Size,
            {Type::EnvPtr, Type::WordPtr, Type::Size, Type::Size, Type::BytePtr, Type::Size},
            false);
        func = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, funcName, _module);

        func->addAttribute(2, llvm::Attribute::ReadOnly);
        func->addAttribute(2, llvm::Attribute::NoAlias);
        func->addAttribute(2, llvm::Attribute::NoCapture);
    }
    return func;
}

llvm::Function* getDBTransBeginiFunc(llvm::Module* _module)
{
    static const auto funcName = "evm.db_trans_begin";
    auto func = _module->getFunction(funcName);
    if (!func)
    {
        auto fty = llvm::FunctionType::get(Type::Void, {Type::EnvPtr}, false);
        func = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, funcName, _module);
    }
    return func;
}
llvm::Function* getDBTransSubmitFunc(llvm::Module* _module)
{
    static const auto funcName = "evm.db_trans_submit";
    auto func = _module->getFunction(funcName);
    if (!func)
    {
        auto i64 = llvm::IntegerType::getInt64Ty(_module->getContext());
        auto fty = llvm::FunctionType::get(
            i64, {Type::EnvPtr, Type::BytePtr->getPointerTo(), Type::Size->getPointerTo()}, false);
        func = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, funcName, _module);
    }
    return func;
}
llvm::Function* getExitFunFunc(llvm::Module* _module)
{
    static const auto funcName = "evm.exit_fun";
    auto func = _module->getFunction(funcName);
    if (!func)
    {
        auto fty = llvm::FunctionType::get(Type::Void, {Type::EnvPtr}, false);
        func = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, funcName, _module);
    }
    return func;
}

llvm::Function* getCallFunc(llvm::Module* _module)
{
    static const auto funcName = "call";
    auto func = _module->getFunction(funcName);
    if (!func)
    {
        auto i32 = llvm::IntegerType::getInt32Ty(_module->getContext());
        auto addrTy = llvm::IntegerType::get(_module->getContext(), 160);
        auto addrPtrTy = addrTy->getPointerTo();
        auto fty = llvm::FunctionType::get(Type::Gas,
            {Type::EnvPtr, i32, Type::Gas, addrPtrTy, Type::WordPtr, Type::BytePtr, Type::Size,
                Type::BytePtr, Type::Size, Type::BytePtr->getPointerTo(),
                Type::Size->getPointerTo()},
            false);
        func = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, "evm.call", _module);
        func->addAttribute(4, llvm::Attribute::ReadOnly);
        func->addAttribute(4, llvm::Attribute::NoAlias);
        func->addAttribute(4, llvm::Attribute::NoCapture);
        func->addAttribute(5, llvm::Attribute::ReadOnly);
        func->addAttribute(5, llvm::Attribute::NoAlias);
        func->addAttribute(5, llvm::Attribute::NoCapture);
        func->addAttribute(6, llvm::Attribute::ReadOnly);
        func->addAttribute(6, llvm::Attribute::NoCapture);
        func->addAttribute(8, llvm::Attribute::NoCapture);
        auto callFunc = func;

        // Create a call wrapper to handle additional checks.
        fty = llvm::FunctionType::get(Type::Gas,
            {Type::EnvPtr, i32, Type::Gas, addrPtrTy, Type::WordPtr, Type::BytePtr, Type::Size,
                Type::BytePtr, Type::Size, Type::BytePtr->getPointerTo(),
                Type::Size->getPointerTo(), addrTy, Type::Size},
            false);
        func = llvm::Function::Create(fty, llvm::Function::PrivateLinkage, funcName, _module);
        func->addAttribute(4, llvm::Attribute::ReadOnly);
        func->addAttribute(4, llvm::Attribute::NoAlias);
        func->addAttribute(4, llvm::Attribute::NoCapture);
        func->addAttribute(5, llvm::Attribute::ReadOnly);
        func->addAttribute(5, llvm::Attribute::NoAlias);
        func->addAttribute(5, llvm::Attribute::NoCapture);
        func->addAttribute(6, llvm::Attribute::ReadOnly);
        func->addAttribute(6, llvm::Attribute::NoCapture);
        func->addAttribute(8, llvm::Attribute::NoCapture);

        auto iter = func->arg_begin();
        auto& env = *iter;
        std::advance(iter, 1);
        auto& callKind = *iter;
        std::advance(iter, 1);
        auto& gas = *iter;
        std::advance(iter, 2);
        auto& valuePtr = *iter;
        std::advance(iter, 7);
        auto& addr = *iter;
        std::advance(iter, 1);
        auto& depth = *iter;

        auto& ctx = _module->getContext();
        llvm::IRBuilder<> builder(ctx);
        auto entryBB = llvm::BasicBlock::Create(ctx, "Entry", func);
        auto checkTransferBB = llvm::BasicBlock::Create(ctx, "CheckTransfer", func);
        auto checkBalanceBB = llvm::BasicBlock::Create(ctx, "CheckBalance", func);
        auto callBB = llvm::BasicBlock::Create(ctx, "Call", func);
        auto failBB = llvm::BasicBlock::Create(ctx, "Fail", func);

        builder.SetInsertPoint(entryBB);
        auto v = builder.CreateAlloca(Type::Word);
        auto addrAlloca = builder.CreateBitCast(builder.CreateAlloca(Type::Word), addrPtrTy);
        auto getBalanceFn = getGetBalanceFunc(_module);
        auto depthOk = builder.CreateICmpSLT(&depth, builder.getInt64(1024));
        builder.CreateCondBr(depthOk, checkTransferBB, failBB);

        builder.SetInsertPoint(checkTransferBB);
        auto notDelegateCall = builder.CreateICmpNE(&callKind, builder.getInt32(EVMC_DELEGATECALL));
        llvm::Value* value = builder.CreateLoad(&valuePtr);
        auto valueNonZero = builder.CreateICmpNE(value, Constant::get(0));
        auto transfer = builder.CreateAnd(notDelegateCall, valueNonZero);
        builder.CreateCondBr(transfer, checkBalanceBB, callBB);

        builder.SetInsertPoint(checkBalanceBB);
        builder.CreateStore(&addr, addrAlloca);
        builder.CreateCall(getBalanceFn, {v, &env, addrAlloca});
        llvm::Value* balance = builder.CreateLoad(v);
        balance = Endianness::toNative(builder, balance);
        value = Endianness::toNative(builder, value);
        auto balanceOk = builder.CreateICmpUGE(balance, value);
        builder.CreateCondBr(balanceOk, callBB, failBB);

        builder.SetInsertPoint(callBB);
        // Pass the first 11 args to the external call.
        llvm::Value* args[11];
        auto it = func->arg_begin();
        for (auto outIt = std::begin(args); outIt != std::end(args); ++it, ++outIt)
            *outIt = &*it;
        auto ret = builder.CreateCall(callFunc, args);
        builder.CreateRet(ret);

        builder.SetInsertPoint(failBB);
        auto failRet = builder.CreateOr(&gas, builder.getInt64(EVM_CALL_FAILURE));
        builder.CreateRet(failRet);
    }
    return func;
}

llvm::Function* getBlockHashFunc(llvm::Module* _module)
{
    static const auto funcName = "evm.blockhash";
    auto func = _module->getFunction(funcName);
    if (!func)
    {
        // TODO: Mark the function as pure to eliminate multiple calls.
        auto i64 = llvm::IntegerType::getInt64Ty(_module->getContext());
        auto fty = llvm::FunctionType::get(Type::Void, {Type::WordPtr, Type::EnvPtr, i64}, false);
        func = llvm::Function::Create(fty, llvm::Function::ExternalLinkage, funcName, _module);
        func->addAttribute(1, llvm::Attribute::NoAlias);
        func->addAttribute(1, llvm::Attribute::NoCapture);
    }
    return func;
}

}  // namespace


llvm::Value* Ext::getArgAlloca()
{
    auto& a = m_argAllocas[m_argCounter];
    if (!a)
    {
        InsertPointGuard g{m_builder};
        auto allocaIt = getMainFunction()->front().begin();
        auto allocaPtr = &(*allocaIt);
        std::advance(allocaIt, m_argCounter);  // Skip already created allocas
        m_builder.SetInsertPoint(allocaPtr);
        a = m_builder.CreateAlloca(Type::Word, nullptr, {"a.", std::to_string(m_argCounter)});
    }
    ++m_argCounter;
    return a;
}

llvm::CallInst* Ext::createCall(EnvFunc _funcId, std::initializer_list<llvm::Value*> const& _args)
{
    auto& func = m_funcs[static_cast<size_t>(_funcId)];
    if (!func)
        func = createFunc(_funcId, getModule());

    m_argCounter = 0;
    return m_builder.CreateCall(func, {_args.begin(), _args.size()});
}

llvm::Value* Ext::createCABICall(
    llvm::Function* _func, std::initializer_list<llvm::Value*> const& _args)
{
    auto args = llvm::SmallVector<llvm::Value*, 8>{_args};
    for (auto&& farg : _func->args())
    {
        if (farg.hasByValAttr() || farg.getType()->isPointerTy())
        {
            auto& arg = args[farg.getArgNo()];
            // TODO: Remove defensive check and always use it this way.
            if (!arg->getType()->isPointerTy())
            {
                auto mem = getArgAlloca();
                // TODO: The bitcast may be redundant
                mem = m_builder.CreateBitCast(mem, arg->getType()->getPointerTo());
                m_builder.CreateStore(arg, mem);
                arg = mem;
            }
        }
    }

    m_argCounter = 0;
    return m_builder.CreateCall(_func, args);
}

llvm::Value* Ext::sload(llvm::Value* _index)
{
    auto index = Endianness::toBE(m_builder, _index);
    auto addrTy = m_builder.getIntNTy(160);
    auto myAddr = Endianness::toBE(
        m_builder, m_builder.CreateTrunc(
                       Endianness::toNative(m_builder, getRuntimeManager().getAddress()), addrTy));
    auto pAddr = m_builder.CreateBitCast(getArgAlloca(), addrTy->getPointerTo());
    m_builder.CreateStore(myAddr, pAddr);
    auto func = getGetStorageFunc(getModule());
    auto pValue = getArgAlloca();
    createCABICall(func, {pValue, getRuntimeManager().getEnvPtr(), pAddr, index});
    return Endianness::toNative(m_builder, m_builder.CreateLoad(pValue));
}

void Ext::sstore(llvm::Value* _index, llvm::Value* _value)
{
    auto addrTy = m_builder.getIntNTy(160);
    auto index = Endianness::toBE(m_builder, _index);
    auto value = Endianness::toBE(m_builder, _value);
    auto myAddr = Endianness::toBE(
        m_builder, m_builder.CreateTrunc(
                       Endianness::toNative(m_builder, getRuntimeManager().getAddress()), addrTy));
    auto func = getSetStorageFunc(getModule());
    createCABICall(func, {getRuntimeManager().getEnvPtr(), myAddr, index, value});
}

void Ext::selfdestruct(llvm::Value* _beneficiary)
{
    auto addrTy = m_builder.getIntNTy(160);
    auto func = getSelfdestructFunc(getModule());
    auto b = Endianness::toBE(m_builder, m_builder.CreateTrunc(_beneficiary, addrTy));
    auto myAddr = Endianness::toBE(
        m_builder, m_builder.CreateTrunc(
                       Endianness::toNative(m_builder, getRuntimeManager().getAddress()), addrTy));
    createCABICall(func, {getRuntimeManager().getEnvPtr(), myAddr, b});
}

llvm::Value* Ext::calldataload(llvm::Value* _idx)
{
    auto ret = getArgAlloca();
    auto result = m_builder.CreateBitCast(ret, Type::BytePtr);

    auto callDataSize = getRuntimeManager().getCallDataSize();
    auto callDataSize64 = m_builder.CreateTrunc(callDataSize, Type::Size);
    auto idxValid = m_builder.CreateICmpULT(_idx, callDataSize);
    auto idx = m_builder.CreateTrunc(
        m_builder.CreateSelect(idxValid, _idx, callDataSize), Type::Size, "idx");

    auto end = m_builder.CreateNUWAdd(idx, m_builder.getInt64(32));
    end = m_builder.CreateSelect(m_builder.CreateICmpULE(end, callDataSize64), end, callDataSize64);
    auto copySize = m_builder.CreateNUWSub(end, idx);
    auto padSize = m_builder.CreateNUWSub(m_builder.getInt64(32), copySize);
    auto dataBegin = m_builder.CreateGEP(Type::Byte, getRuntimeManager().getCallData(), idx);
    m_builder.CreateMemCpy(result, dataBegin, copySize, 1);
    auto pad = m_builder.CreateGEP(Type::Byte, result, copySize);
    m_builder.CreateMemSet(pad, m_builder.getInt8(0), padSize, 1);

    m_argCounter = 0;  // Release args allocas. TODO: This is a bad design
    return Endianness::toNative(m_builder, m_builder.CreateLoad(ret));
}

llvm::Value* Ext::balance(llvm::Value* _address)
{
    auto func = getGetBalanceFunc(getModule());
    auto addrTy = m_builder.getIntNTy(160);
    auto address = Endianness::toBE(m_builder, m_builder.CreateTrunc(_address, addrTy));
    auto pResult = getArgAlloca();
    auto pAddr = m_builder.CreateBitCast(getArgAlloca(), addrTy->getPointerTo());
    m_builder.CreateStore(address, pAddr);
    createCABICall(func, {pResult, getRuntimeManager().getEnvPtr(), pAddr});
    return Endianness::toNative(m_builder, m_builder.CreateLoad(pResult));
}

llvm::Value* Ext::exists(llvm::Value* _address)
{
    auto func = getAccountExistsFunc(getModule());
    auto addrTy = m_builder.getIntNTy(160);
    auto address = Endianness::toBE(m_builder, m_builder.CreateTrunc(_address, addrTy));
    auto pAddr = m_builder.CreateBitCast(getArgAlloca(), addrTy->getPointerTo());
    m_builder.CreateStore(address, pAddr);
    auto r = createCABICall(func, {getRuntimeManager().getEnvPtr(), pAddr});
    return m_builder.CreateTrunc(r, m_builder.getInt1Ty());
}

llvm::Value* Ext::blockHash(llvm::Value* _number)
{
    auto func = getBlockHashFunc(getModule());
    auto number = m_builder.CreateTrunc(_number, m_builder.getInt64Ty());
    auto pResult = getArgAlloca();
    createCABICall(func, {pResult, getRuntimeManager().getEnvPtr(), number});
    return Endianness::toNative(m_builder, m_builder.CreateLoad(pResult));
}

llvm::Value* Ext::sha3(llvm::Value* _inOff, llvm::Value* _inSize)
{
    auto begin = m_memoryMan.getBytePtr(_inOff);
    auto size = m_builder.CreateTrunc(_inSize, Type::Size, "size");
    auto ret = getArgAlloca();
    createCall(EnvFunc::sha3, {begin, size, ret});
    llvm::Value* hash = m_builder.CreateLoad(ret);
    return Endianness::toNative(m_builder, hash);
}

MemoryRef Ext::extcode(llvm::Value* _address)
{
    auto func = getGetCodeFunc(getModule());
    auto addrTy = m_builder.getIntNTy(160);
    auto address = Endianness::toBE(m_builder, m_builder.CreateTrunc(_address, addrTy));
    auto pAddr = m_builder.CreateBitCast(getArgAlloca(), addrTy->getPointerTo());
    m_builder.CreateStore(address, pAddr);
    auto a = getArgAlloca();
    auto codePtrPtr = m_builder.CreateBitCast(a, Type::BytePtr->getPointerTo());
    auto size = createCABICall(func, {codePtrPtr, getRuntimeManager().getEnvPtr(), pAddr});
    auto code = m_builder.CreateLoad(codePtrPtr, "code");
    auto size256 = m_builder.CreateZExt(size, Type::Word);
    return {code, size256};
}

llvm::Value* Ext::extcodesize(llvm::Value* _address)
{
    auto func = getGetCodeSizeFunc(getModule());
    auto addrTy = m_builder.getIntNTy(160);
    auto address = Endianness::toBE(m_builder, m_builder.CreateTrunc(_address, addrTy));
    auto pAddr = m_builder.CreateBitCast(getArgAlloca(), addrTy->getPointerTo());
    m_builder.CreateStore(address, pAddr);
    auto size = createCABICall(func, {getRuntimeManager().getEnvPtr(), pAddr});
    return m_builder.CreateZExt(size, Type::Word);
}

void Ext::log(llvm::Value* _memIdx, llvm::Value* _numBytes, llvm::ArrayRef<llvm::Value*> _topics)
{
    if (!m_topics)
    {
        InsertPointGuard g{m_builder};
        auto& entryBB = getMainFunction()->front();
        m_builder.SetInsertPoint(&entryBB, entryBB.begin());
        m_topics = m_builder.CreateAlloca(Type::Word, m_builder.getInt32(4), "topics");
    }

    auto dataPtr = m_memoryMan.getBytePtr(_memIdx);
    auto dataSize = m_builder.CreateTrunc(_numBytes, Type::Size, "data.size");

    for (size_t i = 0; i < _topics.size(); ++i)
    {
        auto t = Endianness::toBE(m_builder, _topics[i]);
        auto p = m_builder.CreateConstGEP1_32(m_topics, static_cast<unsigned>(i));
        m_builder.CreateStore(t, p);
    }
    auto numTopics = m_builder.getInt64(_topics.size());

    auto addrTy = m_builder.getIntNTy(160);
    auto func = getLogFunc(getModule());

    auto myAddr = Endianness::toBE(
        m_builder, m_builder.CreateTrunc(
                       Endianness::toNative(m_builder, getRuntimeManager().getAddress()), addrTy));
    createCABICall(
        func, {getRuntimeManager().getEnvPtr(), myAddr, dataPtr, dataSize, m_topics, numTopics});
}

llvm::Value* Ext::call(int _kind, llvm::Value* _gas, llvm::Value* _addr, llvm::Value* _value,
    llvm::Value* _inOff, llvm::Value* _inSize, llvm::Value* _outOff, llvm::Value* _outSize)
{
    auto gas = m_builder.CreateTrunc(_gas, Type::Size);
    auto addrTy = m_builder.getIntNTy(160);
    auto addr = m_builder.CreateTrunc(_addr, addrTy);
    addr = Endianness::toBE(m_builder, addr);
    auto inData = m_memoryMan.getBytePtr(_inOff);
    auto inSize = m_builder.CreateTrunc(_inSize, Type::Size);
    auto outData = m_memoryMan.getBytePtr(_outOff);
    auto outSize = m_builder.CreateTrunc(_outSize, Type::Size);

    auto value = getArgAlloca();
    m_builder.CreateStore(Endianness::toBE(m_builder, _value), value);

    auto func = getCallFunc(getModule());
    auto myAddr = Endianness::toBE(
        m_builder, m_builder.CreateTrunc(
                       Endianness::toNative(m_builder, getRuntimeManager().getAddress()), addrTy));
    getRuntimeManager().resetReturnBuf();
    return createCABICall(func,
        {getRuntimeManager().getEnvPtr(), m_builder.getInt32(_kind), gas, addr, value, inData,
            inSize, outData, outSize, getRuntimeManager().getReturnBufDataPtr(),
            getRuntimeManager().getReturnBufSizePtr(), myAddr, getRuntimeManager().getDepth()});
}

std::tuple<llvm::Value*, llvm::Value*> Ext::create(
    llvm::Value* _gas, llvm::Value* _endowment, llvm::Value* _initOff, llvm::Value* _initSize)
{
    auto addrTy = m_builder.getIntNTy(160);
    auto value = getArgAlloca();
    m_builder.CreateStore(Endianness::toBE(m_builder, _endowment), value);
    auto inData = m_memoryMan.getBytePtr(_initOff);
    auto inSize = m_builder.CreateTrunc(_initSize, Type::Size);
    auto pAddr = m_builder.CreateBitCast(getArgAlloca(), m_builder.getInt8PtrTy());

    auto func = getCallFunc(getModule());
    auto myAddr = Endianness::toBE(
        m_builder, m_builder.CreateTrunc(
                       Endianness::toNative(m_builder, getRuntimeManager().getAddress()), addrTy));
    getRuntimeManager().resetReturnBuf();
    auto ret = createCABICall(func,
        {getRuntimeManager().getEnvPtr(), m_builder.getInt32(EVMC_CREATE), _gas,
            llvm::UndefValue::get(addrTy), value, inData, inSize, pAddr, m_builder.getInt64(20),
            getRuntimeManager().getReturnBufDataPtr(), getRuntimeManager().getReturnBufSizePtr(),
            myAddr, getRuntimeManager().getDepth()});

    pAddr = m_builder.CreateBitCast(pAddr, addrTy->getPointerTo());
    return std::tuple<llvm::Value*, llvm::Value*>{ret, pAddr};
}

llvm::Value* Ext::executeSQL(llvm::Value* _addr, int _type, llvm::Value* _name,
    llvm::Value* _nameBytes, llvm::Value* _raw, llvm::Value* _rawBytes)
{
    auto func = getExecuteSQLFunc(getModule());

    auto namePtr = m_memoryMan.getBytePtr(_name);
    auto nameSize = m_builder.CreateTrunc(_nameBytes, Type::Size, "name.size");
    auto rawPtr = m_memoryMan.getBytePtr(_raw);
    auto rawSize = m_builder.CreateTrunc(_rawBytes, Type::Size, "raw.size");

    auto addrTy = m_builder.getIntNTy(160);
    auto ownerAddr = Endianness::toBE(m_builder, m_builder.CreateTrunc(_addr, addrTy));
    auto pAddr = m_builder.CreateBitCast(getArgAlloca(), addrTy->getPointerTo());
    m_builder.CreateStore(ownerAddr, pAddr);
    auto r = createCABICall(
        func, {getRuntimeManager().getEnvPtr(), ownerAddr, m_builder.getInt8((uint8_t)_type),
                  namePtr, nameSize, rawPtr, rawSize});
    return m_builder.CreateZExt(r, Type::Word);
}

llvm::Value* Ext::table_create(llvm::Value* _addr, llvm::Value* _name, llvm::Value* _nameBytes,
    llvm::Value* _raw, llvm::Value* _rawBytes)
{
    auto func = getTableCreateFunc(getModule());

    auto namePtr = m_memoryMan.getBytePtr(_name);
    auto nameSize = m_builder.CreateTrunc(_nameBytes, Type::Size, "name.size");
    auto rawPtr = m_memoryMan.getBytePtr(_raw);
    auto rawSize = m_builder.CreateTrunc(_rawBytes, Type::Size, "raw.size");

    auto addrTy = m_builder.getIntNTy(160);
    auto ownerAddr = Endianness::toBE(m_builder, m_builder.CreateTrunc(_addr, addrTy));
    auto pAddr = m_builder.CreateBitCast(getArgAlloca(), addrTy->getPointerTo());
    m_builder.CreateStore(ownerAddr, pAddr);
    getRuntimeManager().resetReturnBuf();
   return createCABICall(func,
        {getRuntimeManager().getEnvPtr(), ownerAddr, namePtr, nameSize, rawPtr, rawSize,
            getRuntimeManager().getReturnBufDataPtr(), getRuntimeManager().getReturnBufSizePtr()});
}

llvm::Value* Ext::table_rename(llvm::Value* _addr, llvm::Value* _name, llvm::Value* _nameBytes,
    llvm::Value* _raw, llvm::Value* _rawBytes)
{
    auto func = getTableRenameFunc(getModule());

    auto namePtr = m_memoryMan.getBytePtr(_name);
    auto nameSize = m_builder.CreateTrunc(_nameBytes, Type::Size, "name.size");
    auto rawPtr = m_memoryMan.getBytePtr(_raw);
    auto rawSize = m_builder.CreateTrunc(_rawBytes, Type::Size, "raw.size");

    auto addrTy = m_builder.getIntNTy(160);
    auto ownerAddr = Endianness::toBE(m_builder, m_builder.CreateTrunc(_addr, addrTy));
    auto pAddr = m_builder.CreateBitCast(getArgAlloca(), addrTy->getPointerTo());
    m_builder.CreateStore(ownerAddr, pAddr);
    getRuntimeManager().resetReturnBuf();
    return createCABICall(func,
        {getRuntimeManager().getEnvPtr(), ownerAddr, namePtr, nameSize, rawPtr, rawSize,
            getRuntimeManager().getReturnBufDataPtr(), getRuntimeManager().getReturnBufSizePtr()});
}

llvm::Value* Ext::table_insert(llvm::Value* _addr, llvm::Value* _name, llvm::Value* _nameBytes,
    llvm::Value* _raw, llvm::Value* _rawBytes)
{
    auto func = getTableInsertFunc(getModule());

    auto namePtr = m_memoryMan.getBytePtr(_name);
    auto nameSize = m_builder.CreateTrunc(_nameBytes, Type::Size, "name.size");
    auto rawPtr = m_memoryMan.getBytePtr(_raw);
    auto rawSize = m_builder.CreateTrunc(_rawBytes, Type::Size, "raw.size");

    auto addrTy = m_builder.getIntNTy(160);
    auto ownerAddr = Endianness::toBE(m_builder, m_builder.CreateTrunc(_addr, addrTy));
    auto pAddr = m_builder.CreateBitCast(getArgAlloca(), addrTy->getPointerTo());
    m_builder.CreateStore(ownerAddr, pAddr);
    getRuntimeManager().resetReturnBuf();
    return createCABICall(func,
        {getRuntimeManager().getEnvPtr(), ownerAddr, namePtr, nameSize, rawPtr, rawSize,
            getRuntimeManager().getReturnBufDataPtr(), getRuntimeManager().getReturnBufSizePtr()});
}

llvm::Value* Ext::table_delete(llvm::Value* _addr, llvm::Value* _name, llvm::Value* _nameBytes,
    llvm::Value* _raw, llvm::Value* _rawBytes)
{
    auto func = getTableDeleteFunc(getModule());

    auto namePtr = m_memoryMan.getBytePtr(_name);
    auto nameSize = m_builder.CreateTrunc(_nameBytes, Type::Size, "name.size");
    auto rawPtr = m_memoryMan.getBytePtr(_raw);
    auto rawSize = m_builder.CreateTrunc(_rawBytes, Type::Size, "raw.size");

    auto addrTy = m_builder.getIntNTy(160);
    auto ownerAddr = Endianness::toBE(m_builder, m_builder.CreateTrunc(_addr, addrTy));
    auto pAddr = m_builder.CreateBitCast(getArgAlloca(), addrTy->getPointerTo());
    m_builder.CreateStore(ownerAddr, pAddr);
    getRuntimeManager().resetReturnBuf();
    return createCABICall(func,
        {getRuntimeManager().getEnvPtr(), ownerAddr, namePtr, nameSize, rawPtr, rawSize,
            getRuntimeManager().getReturnBufDataPtr(), getRuntimeManager().getReturnBufSizePtr()});
}

llvm::Value* Ext::table_drop(llvm::Value* _addr, llvm::Value* _name, llvm::Value* _nameBytes)
{
    auto func = getTableDropFunc(getModule());

    auto namePtr = m_memoryMan.getBytePtr(_name);
    auto nameSize = m_builder.CreateTrunc(_nameBytes, Type::Size, "name.size");

    auto addrTy = m_builder.getIntNTy(160);
    auto ownerAddr = Endianness::toBE(m_builder, m_builder.CreateTrunc(_addr, addrTy));
    auto pAddr = m_builder.CreateBitCast(getArgAlloca(), addrTy->getPointerTo());
    m_builder.CreateStore(ownerAddr, pAddr);
    getRuntimeManager().resetReturnBuf();
    return createCABICall(func,
        {getRuntimeManager().getEnvPtr(), ownerAddr, namePtr, nameSize,
            getRuntimeManager().getReturnBufDataPtr(), getRuntimeManager().getReturnBufSizePtr()});
}

llvm::Value* Ext::table_update(llvm::Value* _addr, llvm::Value* _name, llvm::Value* _nameBytes,
    llvm::Value* _raw1, llvm::Value* _rawBytes1, llvm::Value* _raw2, llvm::Value* _rawBytes2)
{
    auto func = getTableUpdateFunc(getModule());

    auto namePtr = m_memoryMan.getBytePtr(_name);
    auto nameSize = m_builder.CreateTrunc(_nameBytes, Type::Size, "name.size");
    auto rawPtr1 = m_memoryMan.getBytePtr(_raw1);
    auto rawSize1 = m_builder.CreateTrunc(_rawBytes1, Type::Size, "raw1.size");
    auto rawPtr2 = m_memoryMan.getBytePtr(_raw2);
    auto rawSize2 = m_builder.CreateTrunc(_rawBytes2, Type::Size, "raw2.size");

    auto addrTy = m_builder.getIntNTy(160);
    auto ownerAddr = Endianness::toBE(m_builder, m_builder.CreateTrunc(_addr, addrTy));
    auto pAddr = m_builder.CreateBitCast(getArgAlloca(), addrTy->getPointerTo());
    m_builder.CreateStore(ownerAddr, pAddr);
    getRuntimeManager().resetReturnBuf();
    return createCABICall(
        func, {getRuntimeManager().getEnvPtr(), ownerAddr, namePtr, nameSize, rawPtr1, rawSize1,
                  rawPtr2, rawSize2, getRuntimeManager().getReturnBufDataPtr(),
                  getRuntimeManager().getReturnBufSizePtr()});
}

llvm::Value* Ext::table_grant(llvm::Value* _addr1, llvm::Value* _addr2, llvm::Value* _name,
    llvm::Value* _nameBytes, llvm::Value* _raw, llvm::Value* _rawBytes)
{
    auto func = getTableGrantFunc(getModule());

    auto namePtr = m_memoryMan.getBytePtr(_name);
    auto nameSize = m_builder.CreateTrunc(_nameBytes, Type::Size, "name.size");
    auto rawPtr = m_memoryMan.getBytePtr(_raw);
    auto rawSize = m_builder.CreateTrunc(_rawBytes, Type::Size, "raw.size");

    auto addrTy1 = m_builder.getIntNTy(160);
    auto ownerAddr1 = Endianness::toBE(m_builder, m_builder.CreateTrunc(_addr1, addrTy1));
    auto pAddr1 = m_builder.CreateBitCast(getArgAlloca(), addrTy1->getPointerTo());
    m_builder.CreateStore(ownerAddr1, pAddr1);

    auto addrTy2 = m_builder.getIntNTy(160);
    auto ownerAddr2 = Endianness::toBE(m_builder, m_builder.CreateTrunc(_addr2, addrTy2));
    auto pAddr2 = m_builder.CreateBitCast(getArgAlloca(), addrTy2->getPointerTo());
    m_builder.CreateStore(ownerAddr2, pAddr2);
    getRuntimeManager().resetReturnBuf();
    return createCABICall(
        func, {getRuntimeManager().getEnvPtr(), ownerAddr1, ownerAddr2, namePtr, nameSize, rawPtr,
                  rawSize, getRuntimeManager().getReturnBufDataPtr(),
                  getRuntimeManager().getReturnBufSizePtr()});
}

llvm::Value* Ext::table_get_handle(llvm::Value* _addr, llvm::Value* _name, llvm::Value* _nameBytes,
    llvm::Value* _raw, llvm::Value* _rawBytes)
{
    auto func = getTableGetHandleFunc(getModule());

    auto namePtr = m_memoryMan.getBytePtr(_name);
    auto nameSize = m_builder.CreateTrunc(_nameBytes, Type::Size, "name.size");
    auto rawPtr = m_memoryMan.getBytePtr(_raw);
    auto rawSize = m_builder.CreateTrunc(_rawBytes, Type::Size, "raw.size");

    auto addrTy = m_builder.getIntNTy(160);
    auto address = Endianness::toBE(m_builder, m_builder.CreateTrunc(_addr, addrTy));
    auto pAddr = m_builder.CreateBitCast(getArgAlloca(), addrTy->getPointerTo());
    m_builder.CreateStore(address, pAddr);

    auto pResult = getArgAlloca();
    createCABICall(func,
        {getRuntimeManager().getEnvPtr(), pAddr, namePtr, nameSize, rawPtr, rawSize, pResult});
    return Endianness::toNative(m_builder, m_builder.CreateLoad(pResult));
}

llvm::Value* Ext::table_get_lines(llvm::Value* _handle)
{
    auto func = getTableGetLinesFunc(getModule());

    auto hGet = Endianness::toBE(m_builder, _handle);

    auto pValue = getArgAlloca();
    createCABICall(func, {getRuntimeManager().getEnvPtr(), hGet, pValue});
    return Endianness::toNative(m_builder, m_builder.CreateLoad(pValue));
}

llvm::Value* Ext::table_get_columns(llvm::Value* _handle)
{
    auto func = getTableGetColumnsFunc(getModule());

    auto hGet = Endianness::toBE(m_builder, _handle);

    auto pValue = getArgAlloca();
    createCABICall(func, {getRuntimeManager().getEnvPtr(), hGet, pValue});
    return Endianness::toNative(m_builder, m_builder.CreateLoad(pValue));
}

template <typename T>
static inline void PrintValueAndType(const char* desc, const T* _value)
{
    std::string value;
    llvm::raw_string_ostream os(value);
    _value->print(os);
    std::cout << desc << std::endl;
    std::cout << "type : " << _value->getType()->getTypeID() << std::endl;
    std::cout << "value: " << value << std::endl;
}

llvm::Value* Ext::get_column_len(
    llvm::Value* _handle, llvm::Value* _row, llvm::Value* _columnOff, llvm::Value* _columnSize)
{
    auto func = getColumnLenByNameFunc(getModule());

    auto handle = Endianness::toBE(m_builder, _handle);
    auto row = m_builder.CreateTrunc(_row, m_builder.getInt64Ty());
    auto columnPtr = m_memoryMan.getBytePtr(_columnOff);
    auto columnSize = m_builder.CreateTrunc(_columnSize, Type::Size);

    auto len = getArgAlloca();
    createCABICall(
        func, {getRuntimeManager().getEnvPtr(), handle, row, columnPtr, columnSize, len});
    return Endianness::toNative(m_builder, m_builder.CreateLoad(len));
}

void Ext::table_get_column(llvm::Value* _handle, llvm::Value* _row, llvm::Value* _columnOff,
    llvm::Value* _columnSize, llvm::Value* _outOff, llvm::Value* _outSize)
{
    auto func = getColumnByNameFunc(getModule());

    auto handle = Endianness::toBE(m_builder, _handle);
    auto row = m_builder.CreateTrunc(_row, m_builder.getInt64Ty());
    auto columnPtr = m_memoryMan.getBytePtr(_columnOff);
    auto columnSize = m_builder.CreateTrunc(_columnSize, Type::Size);
    auto outPtr = m_memoryMan.getBytePtr(_outOff);
    auto outSize = m_builder.CreateTrunc(_outSize, Type::Size);

    createCABICall(func,
        {getRuntimeManager().getEnvPtr(), handle, row, columnPtr, columnSize, outPtr, outSize});
}

llvm::Value* Ext::get_column_len(llvm::Value* _handle, llvm::Value* _row, llvm::Value* _column)
{
    auto func = getColumnLenByIndexFunc(getModule());

    auto handle = Endianness::toBE(m_builder, _handle);
    auto row = m_builder.CreateTrunc(_row, m_builder.getInt64Ty());
    auto column = m_builder.CreateTrunc(_column, m_builder.getInt64Ty());

    auto len = getArgAlloca();
    createCABICall(func, {getRuntimeManager().getEnvPtr(), handle, row, column, len});
    return Endianness::toNative(m_builder, m_builder.CreateLoad(len));
}

void Ext::table_get_column(llvm::Value* _handle, llvm::Value* _row, llvm::Value* _column,
    llvm::Value* _outOff, llvm::Value* _outSize)
{
    auto func = getColumnByIndexFunc(getModule());

    auto handle = Endianness::toBE(m_builder, _handle);
    auto row = m_builder.CreateTrunc(_row, m_builder.getInt64Ty());
    auto column = m_builder.CreateTrunc(_column, m_builder.getInt64Ty());
    auto outPtr = m_memoryMan.getBytePtr(_outOff);
    auto outSize = m_builder.CreateTrunc(_outSize, Type::Size);

    createCABICall(func, {getRuntimeManager().getEnvPtr(), handle, row, column, outPtr, outSize});
}

void Ext::db_trans_begin()
{
    auto func = getDBTransBeginiFunc(getModule());
    createCABICall(func, {getRuntimeManager().getEnvPtr()});
}

llvm::Value* Ext::db_trans_submit()
{
    auto func = getDBTransSubmitFunc(getModule());
    getRuntimeManager().resetReturnBuf();
    return createCABICall(
        func, {getRuntimeManager().getEnvPtr(), getRuntimeManager().getReturnBufDataPtr(),
                  getRuntimeManager().getReturnBufSizePtr()});
}

void Ext::exit_fun()
{
    auto func = getExitFunFunc(getModule());
    createCABICall(func, {getRuntimeManager().getEnvPtr()});
}

}  // namespace jit
}  // namespace eth
}  // namespace dev
