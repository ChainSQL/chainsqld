#include <peersafe/precompiled/TableOpPrecompiled.h>
#include <peersafe/precompiled/Utils.h>
#include <peersafe/precompiled/ABI.h>
#include <ripple/basics/StringUtilities.h>

namespace ripple {
const char* const TABLE_METHOD_CREATE_TABLE_STR =
    "createTable(string,string)";
const char* const TABLE_METHOD_CREATE_BY_CONTRACT_STR =
    "createByContract(string,string)";
const char* const TABLE_METHOD_DROP_TABLE_STR =
    "dropTable(string)";
const char* const TABLE_METHOD_DROP_TABLE_BY_CONTRACT_STR = 
    "dropTableByContract(string)";
const char* const TABLE_METHOD_GRANT_STR =
    "grant(address,string,string)";
const char* const TABLE_METHOD_GRANT_BY_CONTRACT_STR = 
    "grantByContract(address,string,string)";
const char* const TABLE_METHOD_RENAME_TABLE_STR =
    "renameTable(string,string)";
const char* const TABLE_METHOD_RENAME_TABLE_BY_CONTRACT_STR =
    "renameTableByContract(string,string)";
const char* const TABLE_METHOD_INSERT_STR =
    "insert(address,string,string)";
const char* const TABLE_METHOD_INSERT_BY_CONTRACT_STR =
    "insertByContract(address,string,string)";
const char* const TABLE_METHOD_INSERT_HASH_STR =
    "insertWithHash(address,string,string,string)";
const char* const TABLE_METHOD_INSERT_HASH_BY_CONTTRACT_STR =
    "insertWithHashByContract(address,string,string,string)";
const char* const TABLE_METHOD_UPDATE_GETRAW_STR =
    "update(address,string,string,string)";
const char* const TABLE_METHOD_UPDATE_GETRAW_BY_CONTTRACT_STR = 
     "updateByContract(address,string,string,string)";
const char* const TABLE_METHOD_UPDATE_STR =
    "update(address,string,string)";
const char* const TABLE_METHOD_UPDATE_BY_CONTTRACT_STR = 
     "updateByContract(address,string,string)";
const char* const TABLE_METHOD_DELETE_DATA_STR =
    "deleteData(address,string,string)";
const char* const TABLE_METHOD_DELETE_BY_CONTRACT_STR =
    "deleteByContract(address,string,string)";
const char* const TABLE_METHOD_ADD_FIELDS_STR =
    "addFields(string,string)";
const char* const TABLE_METHOD_ADD_FIELDS_BY_CONTRACT_STR =
    "addFieldsByContract(string,string)";
const char* const TABLE_METHOD_DELETE_FIELDS_STR =
    "deleteFields(string,string)";
const char* const TABLE_METHOD_DELETE_FIELDS_BY_CONTRACT_STR =
    "deleteFieldsByContract(string,string)";
const char* const TABLE_METHOD_MODIFY_FIELDS_STR =
    "modifyFields(string,string)";
const char* const TABLE_METHOD_MODIFY_FIELDS_BY_CONTRACT_STR =
    "modifyFieldsByContract(string,string)";
const char* const TABLE_METHOD_CREATE_INDEX_STR =
    "createIndex(string,string)";
const char* const TABLE_METHOD_CREATE_INDEX_BY_CONTRACT_STR =
    "createIndexByContract(string,string)";
const char* const TABLE_METHOD_DELETE_INDEX_STR =
    "deleteIndex(string,string)";
const char* const TABLE_METHOD_DELETE_INDEX_BY_CONTRACT_STR =
    "deleteIndexByContract(string,string)";
const char* const TABLE_METHOD_GET_DATA_HANDLE_STR =
    "getDataHandle(address,string,string)";
const char* const TABLE_METHOD_GET_DATA_HANDLE_BY_CONTRACT_STR =
    "getDataHandleByContract(address,string,string)";

TableOpPrecompiled::TableOpPrecompiled()
{
    name2Selector_[TABLE_METHOD_CREATE_TABLE_STR] =
        getFuncSelector(TABLE_METHOD_CREATE_TABLE_STR);
    name2Selector_[TABLE_METHOD_CREATE_BY_CONTRACT_STR] =
        getFuncSelector(TABLE_METHOD_CREATE_BY_CONTRACT_STR);
    name2Selector_[TABLE_METHOD_DROP_TABLE_STR] =
        getFuncSelector(TABLE_METHOD_DROP_TABLE_STR);
    name2Selector_[TABLE_METHOD_DROP_TABLE_BY_CONTRACT_STR] =
        getFuncSelector(TABLE_METHOD_DROP_TABLE_BY_CONTRACT_STR);
    name2Selector_[TABLE_METHOD_GRANT_STR] =
        getFuncSelector(TABLE_METHOD_GRANT_STR);
    name2Selector_[TABLE_METHOD_GRANT_BY_CONTRACT_STR] =
        getFuncSelector(TABLE_METHOD_GRANT_BY_CONTRACT_STR);
    name2Selector_[TABLE_METHOD_RENAME_TABLE_STR] =
        getFuncSelector(TABLE_METHOD_RENAME_TABLE_STR);
    name2Selector_[TABLE_METHOD_RENAME_TABLE_BY_CONTRACT_STR] =
        getFuncSelector(TABLE_METHOD_RENAME_TABLE_BY_CONTRACT_STR);
    name2Selector_[TABLE_METHOD_INSERT_STR] =
        getFuncSelector(TABLE_METHOD_INSERT_STR);
    name2Selector_[TABLE_METHOD_INSERT_HASH_STR] =
        getFuncSelector(TABLE_METHOD_INSERT_HASH_STR);
    name2Selector_[TABLE_METHOD_INSERT_HASH_BY_CONTTRACT_STR] =
        getFuncSelector(TABLE_METHOD_INSERT_HASH_BY_CONTTRACT_STR);
    name2Selector_[TABLE_METHOD_INSERT_BY_CONTRACT_STR] =
        getFuncSelector(TABLE_METHOD_INSERT_BY_CONTRACT_STR);
    name2Selector_[TABLE_METHOD_UPDATE_GETRAW_STR] =
        getFuncSelector(TABLE_METHOD_UPDATE_GETRAW_STR);
    name2Selector_[TABLE_METHOD_UPDATE_STR] =
        getFuncSelector(TABLE_METHOD_UPDATE_STR);
    name2Selector_[TABLE_METHOD_UPDATE_BY_CONTTRACT_STR] =
        getFuncSelector(TABLE_METHOD_UPDATE_BY_CONTTRACT_STR);
    name2Selector_[TABLE_METHOD_UPDATE_GETRAW_BY_CONTTRACT_STR] =
        getFuncSelector(TABLE_METHOD_UPDATE_GETRAW_BY_CONTTRACT_STR);
    name2Selector_[TABLE_METHOD_DELETE_DATA_STR] =
        getFuncSelector(TABLE_METHOD_DELETE_DATA_STR);
    name2Selector_[TABLE_METHOD_DELETE_BY_CONTRACT_STR] =
        getFuncSelector(TABLE_METHOD_DELETE_BY_CONTRACT_STR);
    name2Selector_[TABLE_METHOD_ADD_FIELDS_STR] =
        getFuncSelector(TABLE_METHOD_ADD_FIELDS_STR);
    name2Selector_[TABLE_METHOD_ADD_FIELDS_BY_CONTRACT_STR] =
        getFuncSelector(TABLE_METHOD_ADD_FIELDS_BY_CONTRACT_STR);
    name2Selector_[TABLE_METHOD_DELETE_FIELDS_STR] =
        getFuncSelector(TABLE_METHOD_DELETE_FIELDS_STR);
    name2Selector_[TABLE_METHOD_DELETE_FIELDS_BY_CONTRACT_STR] =
        getFuncSelector(TABLE_METHOD_DELETE_FIELDS_BY_CONTRACT_STR);
    name2Selector_[TABLE_METHOD_MODIFY_FIELDS_STR] =
        getFuncSelector(TABLE_METHOD_MODIFY_FIELDS_STR);
    name2Selector_[TABLE_METHOD_MODIFY_FIELDS_BY_CONTRACT_STR] =
        getFuncSelector(TABLE_METHOD_MODIFY_FIELDS_BY_CONTRACT_STR);
    name2Selector_[TABLE_METHOD_CREATE_INDEX_STR] =
        getFuncSelector(TABLE_METHOD_CREATE_INDEX_STR);
    name2Selector_[TABLE_METHOD_CREATE_INDEX_BY_CONTRACT_STR] =
        getFuncSelector(TABLE_METHOD_CREATE_INDEX_BY_CONTRACT_STR);
    name2Selector_[TABLE_METHOD_DELETE_INDEX_STR] =
        getFuncSelector(TABLE_METHOD_DELETE_INDEX_STR);
    name2Selector_[TABLE_METHOD_DELETE_INDEX_BY_CONTRACT_STR] =
        getFuncSelector(TABLE_METHOD_DELETE_INDEX_BY_CONTRACT_STR);
    name2Selector_[TABLE_METHOD_GET_DATA_HANDLE_STR] =
        getFuncSelector(TABLE_METHOD_GET_DATA_HANDLE_STR);
    name2Selector_[TABLE_METHOD_GET_DATA_HANDLE_BY_CONTRACT_STR] =
        getFuncSelector(TABLE_METHOD_GET_DATA_HANDLE_BY_CONTRACT_STR);
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
    if (func)
    {
        AccountID owner, destAddr;
        std::string tableName(""), tableNewName(""), raw(""), rawUpdate(""),
            rawGet(""), autoFillField("");
        int64_t ret(0), runGas(0);
        uint256 getRes(0);
        if (func == name2Selector_[TABLE_METHOD_CREATE_TABLE_STR])
        {
            abi.abiOut(data, tableName, raw);
            ret = _s.createTable(origin, tableName, raw);
        }
        if (func == name2Selector_[TABLE_METHOD_CREATE_BY_CONTRACT_STR])
        {
            abi.abiOut(data, tableName, raw);
            ret = _s.createTable(caller, tableName, raw);
        }
        if (func == name2Selector_[TABLE_METHOD_DROP_TABLE_STR])
        {
            abi.abiOut(data, tableName);
            ret = _s.dropTable(origin, tableName);
        }
        if (func == name2Selector_[TABLE_METHOD_DROP_TABLE_BY_CONTRACT_STR])
        {
            abi.abiOut(data, tableName);
            ret = _s.dropTable(caller, tableName);
        }
        if (func == name2Selector_[TABLE_METHOD_GRANT_STR])
        {
            abi.abiOut(data, destAddr, tableName, raw); 
            ret = _s.grantTable(origin, destAddr, tableName, raw);
        }
        if (func == name2Selector_[TABLE_METHOD_GRANT_BY_CONTRACT_STR])
        {
            abi.abiOut(data, destAddr, tableName, raw);
            ret = _s.grantTable(caller, destAddr, tableName, raw);
        }
        if (func == name2Selector_[TABLE_METHOD_RENAME_TABLE_STR])
        {
            abi.abiOut(data, tableName, tableNewName);
            ret = _s.renameTable(origin, tableName, tableNewName);
        }
        if (func == name2Selector_[TABLE_METHOD_RENAME_TABLE_BY_CONTRACT_STR])
        {
            abi.abiOut(data, tableName, tableNewName);
            ret = _s.renameTable(caller, tableName, tableNewName);
        }
        if (func == name2Selector_[TABLE_METHOD_INSERT_STR])
        {
            abi.abiOut(data, owner, tableName, raw);
            ret = _s.insertData(origin, owner, tableName, raw);
        }
        if (func == name2Selector_[TABLE_METHOD_INSERT_HASH_STR])
        {
            abi.abiOut(data, owner, tableName, raw, autoFillField);
            ret = _s.insertData(origin, owner, tableName, raw, autoFillField);
        }
        if (func == name2Selector_[TABLE_METHOD_INSERT_HASH_BY_CONTTRACT_STR])
        {
            abi.abiOut(data, owner, tableName, raw, autoFillField);
            ret = _s.insertData(caller, owner, tableName, raw, autoFillField);
        }
        if (func == name2Selector_[TABLE_METHOD_INSERT_BY_CONTRACT_STR])
        {
            abi.abiOut(data, owner, tableName, raw);
            ret = _s.insertData(caller, owner, tableName, raw);
        }
        if (func == name2Selector_[TABLE_METHOD_UPDATE_GETRAW_STR])
        {
            abi.abiOut(data, owner, tableName, rawUpdate, rawGet);
            ret = _s.updateData(origin, owner, tableName, rawGet, rawUpdate);
        }
        if (func == name2Selector_[TABLE_METHOD_UPDATE_STR])
        {
            abi.abiOut(data, owner, tableName, raw);
            ret = _s.updateData(origin, owner, tableName, raw);
        }
        if (func == name2Selector_[TABLE_METHOD_UPDATE_GETRAW_BY_CONTTRACT_STR])
        {
            abi.abiOut(data, owner, tableName, rawUpdate, rawGet);
            ret = _s.updateData(caller, owner, tableName, rawGet, rawUpdate);
        }
        if (func == name2Selector_[TABLE_METHOD_UPDATE_BY_CONTTRACT_STR])
        {
            abi.abiOut(data, owner, tableName, raw);
            ret = _s.updateData(caller, owner, tableName, raw);
        }
        if (func == name2Selector_[TABLE_METHOD_DELETE_DATA_STR])
        {
            abi.abiOut(data, owner, tableName, raw);
            ret = _s.deleteData(origin, owner, tableName, raw);
        }
        if (func == name2Selector_[TABLE_METHOD_DELETE_BY_CONTRACT_STR])
        {
            abi.abiOut(data, owner, tableName, raw);
            ret = _s.deleteData(caller, owner, tableName, raw);
        }
        if (func == name2Selector_[TABLE_METHOD_ADD_FIELDS_STR])
        {
            abi.abiOut(data, tableName, raw);
            TableOpType eType = T_ADD_FIELDS;
            ret = _s.updateFieldsTable(origin, eType, tableName, raw);
        }    
        if (func == name2Selector_[TABLE_METHOD_ADD_FIELDS_BY_CONTRACT_STR])
        {
            abi.abiOut(data, tableName, raw);
            TableOpType eType = T_ADD_FIELDS;
            ret = _s.updateFieldsTable(caller, eType, tableName, raw);
        }
        if (func == name2Selector_[TABLE_METHOD_DELETE_FIELDS_STR])
        {
            abi.abiOut(data, tableName, raw);
            TableOpType eType = T_DELETE_FIELDS;
            ret = _s.updateFieldsTable(origin, eType, tableName, raw);
        }
        if (func == name2Selector_[TABLE_METHOD_DELETE_FIELDS_BY_CONTRACT_STR])
        {
            abi.abiOut(data, tableName, raw);
            TableOpType eType = T_DELETE_FIELDS;
            ret = _s.updateFieldsTable(caller, eType, tableName, raw);
        }
        if (func == name2Selector_[TABLE_METHOD_MODIFY_FIELDS_STR])
        {
            abi.abiOut(data, tableName, raw);
            TableOpType eType = T_MODIFY_FIELDS;
            ret = _s.updateFieldsTable(origin, eType, tableName, raw);
        }
        if (func == name2Selector_[TABLE_METHOD_MODIFY_FIELDS_BY_CONTRACT_STR])
        {
            abi.abiOut(data, tableName, raw);
            TableOpType eType = T_MODIFY_FIELDS;
            ret = _s.updateFieldsTable(caller, eType, tableName, raw);
        }
        if (func == name2Selector_[TABLE_METHOD_CREATE_INDEX_STR])
        {
            abi.abiOut(data, tableName, raw);
            TableOpType eType = T_CREATE_INDEX;
            ret = _s.updateFieldsTable(origin, eType, tableName, raw);
        }
        if (func == name2Selector_[TABLE_METHOD_CREATE_INDEX_BY_CONTRACT_STR])
        {
            abi.abiOut(data, tableName, raw);
            TableOpType eType = T_CREATE_INDEX;
            ret = _s.updateFieldsTable(caller, eType, tableName, raw);
        }
        if (func == name2Selector_[TABLE_METHOD_DELETE_INDEX_STR])
        {
            abi.abiOut(data, tableName, raw);
            TableOpType eType = T_DELETE_INDEX;
            ret = _s.updateFieldsTable(origin, eType, tableName, raw);
        }
        if (func == name2Selector_[TABLE_METHOD_DELETE_INDEX_BY_CONTRACT_STR])
        {
            abi.abiOut(data, tableName, raw);
            TableOpType eType = T_DELETE_INDEX;
            ret = _s.updateFieldsTable(caller, eType, tableName, raw);
        }
        if (func == name2Selector_[TABLE_METHOD_GET_DATA_HANDLE_STR])
        {
            abi.abiOut(data, owner, tableName, raw);
            getRes = _s.getDataHandle(origin, owner, tableName, raw);
        }
        if (func == name2Selector_[TABLE_METHOD_GET_DATA_HANDLE_BY_CONTRACT_STR])
        {
            abi.abiOut(data, owner, tableName, raw);
            getRes = _s.getDataHandle(caller, owner, tableName, raw);
        }
        runGas = _s.ctx().view().fees().drops_per_byte * raw.size();
        
        return std::make_tuple(TER::fromInt(ret), Blob(getRes.begin(),getRes.end()), runGas);
    }
    else
    {
        return std::make_tuple(temUNKNOWN, strCopy(""),0);
    }
}
}  // namespace ripple