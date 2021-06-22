#include <peersafe/precompiled/TableOpPrecompiled.h>
#include <peersafe/precompiled/Utils.h>
#include <peersafe/precompiled/ABI.h>
#include <ripple/basics/StringUtilities.h>

namespace ripple {

const char* const TABLE_METHOD_INSERT_STR =
    "insert(address,string,string)";
const char* const TABLE_METHOD_INSERT_HASH_STR =
    "insertWithHash(address,string,string,string)";

TableOpPrecompiled::TableOpPrecompiled()
{
    name2Selector_[TABLE_METHOD_INSERT_STR] =
        getFuncSelector(TABLE_METHOD_INSERT_STR);
    name2Selector_[TABLE_METHOD_INSERT_HASH_STR] =
        getFuncSelector(TABLE_METHOD_INSERT_HASH_STR);
}

std::string
TableOpPrecompiled::toString()
{
    return "TableOperation";
}

std::tuple<TER, eth::bytes, int64_t>
TableOpPrecompiled::execute(
    SleOps& _s,
    eth::bytesConstRef _in,
    AccountID const& caller,
    AccountID const& origin)
{
    uint32_t func = getParamFunc(_in);
    bytesConstRef data = getParamData(_in);
    ContractABI abi;
    if (func == name2Selector_[TABLE_METHOD_INSERT_STR] ||
        func == name2Selector_[TABLE_METHOD_INSERT_HASH_STR])
    {
        AccountID owner;
        std::string tableName, raw, autoFillField;
        int64_t ret, runGas;
        if (func == name2Selector_[TABLE_METHOD_INSERT_STR])
        {
            abi.abiOut(data, owner, tableName, raw);
            ret = _s.insertData(origin, owner, tableName, raw);
        }
        else
        {
            abi.abiOut(data, owner, tableName, raw, autoFillField);
            ret =
                _s.insertData(origin, owner, tableName, raw, autoFillField);
        }
        runGas = _s.ctx().view().fees().drops_per_byte * raw.size();
        return std::make_tuple(
            TER::fromInt(ret), strCopy(""), runGas);
    }
    else
    {
        return std::make_tuple(tesSUCCESS, strCopy(""),0);
    }
}
}  // namespace ripple