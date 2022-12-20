#include <assert.h>
#include <iostream>
#include <cstdlib>

#include "ExtVMFace.h"
#include "Common.h"
#include <eth/evmc/include/evmc/evmc.h>
#include <eth/evmc/include/evmc/evmc.hpp>
#include <peersafe/core/Tuning.h>

//evmc_context_fn_table const fnTable = {
//    accountExists,
//    getStorage,
//    setStorage,
//    getBalance,
//    getCodeSize,
//    copyCode,
//    selfdestruct,
//    call,
//    getTxContext,
//    getBlockHash,
//    log,
//    evm_executeSQL,
//
//    table_create,
//    table_rename,
//    table_insert,
//    table_delete,
//    table_drop,
//    table_update,
//    table_grant,
//
//    table_get_handle,
//    table_get_lines,
//    table_get_columns,
//    get_column_by_name,
//    get_column_by_index,
//
//    db_trans_begin,
//    db_trans_submit,
//
//    release_resource,
//
//    get_column_len_by_name,
//    get_column_len_by_index,
//
//    account_set,
//    transfer_fee_set,
//    trust_set,
//    trust_limit,
//    gateway_balance,
//    pay
//};

namespace eth {
    bool EvmCHost::account_exists(evmc::address const& _addr) const noexcept
    {
        return m_extVM.exists(_addr);
    }

    evmc::bytes32 EvmCHost::get_storage(evmc::address const& _addr, evmc::bytes32 const& _key) const
        noexcept
    {
        (void)_addr;
        assert(_addr == m_extVM.myAddress);
        return m_extVM.store(_key);
    }

    evmc_storage_status EvmCHost::set_storage(
        evmc::address const& _addr, evmc::bytes32 const& _key, evmc::bytes32 const& _value) noexcept
    {
        (void)_addr;
        assert(_addr == m_extVM.myAddress);
        ripple::uint256 uNewValue = ripple::fromEvmC(_value);
        ripple::uint256 uOldValue = ripple::fromEvmC(m_extVM.store(_key));

        if (uNewValue == uOldValue)
            return EVMC_STORAGE_UNCHANGED;

        auto status = EVMC_STORAGE_MODIFIED;
        if (uOldValue == 0)
        {
            status = EVMC_STORAGE_ADDED;
        }
        else if (uNewValue == 0 && uOldValue != 0)       // If delete
        {
            status = EVMC_STORAGE_DELETED;
            m_extVM.sub.refunds += ripple::STORE_REFUND_GAS;    // Increase refund counter
        }

        m_extVM.setStore(_key, _value);
        return status;
    }

    evmc::uint256be EvmCHost::get_balance(evmc::address const& _addr) const noexcept
    {
        return m_extVM.balance(_addr);
    }

    size_t EvmCHost::get_code_size(evmc::address const& _addr) const noexcept
    {
        return m_extVM.codeSizeAt(_addr);
    }

    evmc::bytes32 EvmCHost::get_code_hash(evmc::address const& _addr) const noexcept
    {
        return m_extVM.codeHashAt(_addr);
    }

    size_t EvmCHost::copy_code(evmc::address const& _addr, size_t _codeOffset, byte* _bufferData,
        size_t _bufferSize) const noexcept
    {
        bytes const& code = m_extVM.codeAt(_addr);

        // Handle "big offset" edge case.
        if (_codeOffset >= code.size())
            return 0;

        size_t maxToCopy = code.size() - _codeOffset;
        size_t numToCopy = std::min(maxToCopy, _bufferSize);
        std::copy_n(&code[_codeOffset], numToCopy, _bufferData);
        return numToCopy;
    }

    void EvmCHost::selfdestruct(evmc::address const& _addr, evmc::address const& _beneficiary) noexcept
    {
        (void)_addr;
        assert(_addr == m_extVM.myAddress);
        m_extVM.selfdestruct(_beneficiary);
    }


    void EvmCHost::emit_log(evmc::address const& _addr, uint8_t const* _data, size_t _dataSize,
        evmc::bytes32 const _topics[], size_t _numTopics) noexcept
    {
        (void)_addr;
        assert(_addr == m_extVM.myAddress);
        m_extVM.log(_topics, _numTopics, bytesConstRef{ _data, _dataSize });
    }

    evmc_tx_context EvmCHost::get_tx_context() const noexcept
    {
        evmc_tx_context result = {};

        result.tx_gas_price = m_extVM.gasPrice;
        result.tx_origin = m_extVM.origin;
        result.block_coinbase = m_extVM.envInfo().coin_base();
        result.block_number = m_extVM.envInfo().block_number();
        result.block_timestamp = m_extVM.envInfo().block_timestamp();
        result.block_gas_limit = m_extVM.envInfo().gasLimit();
        result.block_difficulty = evmc_uint256be{};
        result.chain_id = ripple::toEvmC(ripple::uint256(m_extVM.envInfo().chainID()));
        result.eth_tx = m_extVM.envInfo().eth_tx();

        return result;
    }

    evmc::bytes32 EvmCHost::get_block_hash(int64_t _number) const noexcept
    {
        return m_extVM.blockHash(_number);
    }

    evmc::result EvmCHost::create(evmc_message const& _msg) noexcept
    {
        int64_t gas = _msg.gas;
        evmc_uint256be value = _msg.value;
        bytesConstRef init = { _msg.input_data, _msg.input_size };
        // ExtVM::create takes the sender address from .myAddress.
        assert(std::memcmp(_msg.sender.bytes, m_extVM.myAddress.bytes, sizeof(m_extVM.myAddress)) == 0);

        Instruction opcode = _msg.kind == EVMC_CREATE ? Instruction::CREATE : Instruction::CREATE2;

        CreateResult result = m_extVM.create(value, gas, init, opcode, _msg.create2_salt);
        evmc_result evmcResult = {};
        evmcResult.status_code = result.status;
        evmcResult.gas_left = static_cast<int64_t>(gas);
        evmcResult.release = nullptr;
        evmcResult.exception = result.exception;

        if (result.status == EVMC_SUCCESS)
        {
            evmcResult.create_address = result.address;
            evmcResult.output_data = nullptr;
            evmcResult.output_size = 0;
        }
        else
        {
            // Pass the output to the EVM without a copy. The EVM will delete it
            // when finished with it.

            // First assign reference. References are not invalidated when vector
            // of bytes is moved. See `.takeBytes()` below.
            evmcResult.output_data = result.output.data();
            evmcResult.output_size = result.output.size();
 
            // Place a new vector of bytes containing output in result's reserved memory.
            auto* data = evmc_get_optional_storage(&evmcResult);
            //static_assert(sizeof(bytes) <= sizeof(*data), "Vector is too big");
            new(data) bytes(result.output.takeBytes());
            // Set the destructor to delete the vector.
            evmcResult.release = [](evmc_result const* _result)
            {
                auto* data = evmc_get_const_optional_storage(_result);
                auto& output = reinterpret_cast<bytes const&>(*data);
                // Explicitly call vector's destructor to release its data.
                // This is normal pattern when placement new operator is used.
                output.~bytes();
            };
        }
        return evmc::result{ evmcResult };
    }

    evmc::result EvmCHost::call(evmc_message const& _msg) noexcept
    {
        assert(_msg.gas >= 0 && "Invalid gas value");
        assert(_msg.depth == static_cast<int>(m_extVM.depth) + 1);
        
        // Handle CREATE separately.
        if (_msg.kind == EVMC_CREATE || _msg.kind == EVMC_CREATE2)
            return create(_msg);

        CallParameters params;
        params.gas = _msg.gas;
        params.apparentValue = _msg.value;
        if (_msg.kind == EVMC_DELEGATECALL)
            params.valueTransfer = { { 0 } };
        else
            params.valueTransfer = params.apparentValue;
        params.senderAddress = _msg.sender;
        params.codeAddress = _msg.destination;
        params.receiveAddress =
            _msg.kind == EVMC_CALL ? params.codeAddress : m_extVM.myAddress;
        params.data = { _msg.input_data, _msg.input_size };
        params.staticCall = (_msg.flags & EVMC_STATIC) != 0;
        params.onOp = {};

        CallResult result = m_extVM.call(params);
        evmc_result evmcResult = {};
        evmcResult.status_code = result.status;
        evmcResult.gas_left = params.gas;
        evmcResult.release = nullptr;
        evmcResult.exception = result.exception;
        // Pass the output to the EVM without a copy. The EVM will delete it
        // when finished with it.

        // First assign reference. References are not invalidated when vector
        // of bytes is moved. See `.takeBytes()` below.
        evmcResult.output_data = result.output.data();
        evmcResult.output_size = result.output.size();

        // Place a new vector of bytes containing output in result's reserved memory.
        auto* data = evmc_get_optional_storage(&evmcResult);
        //static_assert(sizeof(bytes) <= sizeof(*data), "Vector is too big");
        new(data) bytes(result.output.takeBytes());
        // Set the destructor to delete the vector.
        //o_result->release = [](evmc_result const* _result)
        evmcResult.release = [](evmc_result const* _result)
        {
            auto* data = evmc_get_const_optional_storage(_result);
            auto& output = reinterpret_cast<bytes const&>(*data);
            // Explicitly call vector's destructor to release its data.
            // This is normal pattern when placement new operator is used.
            output.~bytes();
        };
        return evmc::result{ evmcResult };
    }

    int64_t EvmCHost::evm_executeSQL(
        evmc_address const* _addr,
        uint8_t _type,
        uint8_t const* _name,
        size_t _nameSize,
        uint8_t const* _raw,
        size_t _rawSize
    ) noexcept
    {
        return m_extVM.executeSQL(_addr, _type, bytesConstRef{ _name, _nameSize }, bytesConstRef{ _raw, _rawSize });
    }

    int64_t EvmCHost::table_create(
        const struct evmc_address* address,
        uint8_t const* _name,
        size_t _nameSize,
        uint8_t const* _raw,
        size_t _rawSize) 
    {
        return m_extVM.table_create(address, bytesConstRef{ _name, _nameSize },
            bytesConstRef{ _raw, _rawSize });
    }

    int64_t EvmCHost::table_rename(
        const struct evmc_address* address,
        uint8_t const* _name,
        size_t _nameSize,
        uint8_t const* _raw,
        size_t _rawSize)
    {
        return m_extVM.table_rename(address, bytesConstRef{ _name, _nameSize }, bytesConstRef{ _raw, _rawSize });
    }
    int64_t EvmCHost::table_insert(
        const struct evmc_address* address,
        uint8_t const* _name,
        size_t _nameSize,
        uint8_t const* _raw,
        size_t _rawSize)
    {
        return m_extVM.table_insert(address, bytesConstRef{ _name, _nameSize }, bytesConstRef{ _raw, _rawSize });
    }

    int64_t EvmCHost::table_delete(
        const struct evmc_address* address,
        uint8_t const* _name,
        size_t _nameSize,
        uint8_t const* _raw,
        size_t _rawSize)
    {
        return m_extVM.table_delete(address, bytesConstRef{ _name, _nameSize }, bytesConstRef{ _raw, _rawSize });
    }
    int64_t EvmCHost::table_drop(
        const struct evmc_address* address,
        uint8_t const* _name,
        size_t _nameSize)
    {
        return m_extVM.table_drop(address, bytesConstRef{ _name, _nameSize });
    }
    int64_t EvmCHost::table_update(
        const struct evmc_address* address,
        uint8_t const* _name,
        size_t _nameSize,
        uint8_t const* _raw1,
        size_t _rawSize1,
        uint8_t const* _raw2,
        size_t _rawSize2)
    {
        return m_extVM.table_update(address, bytesConstRef{ _name, _nameSize }, bytesConstRef{ _raw1, _rawSize1 }, bytesConstRef{ _raw2, _rawSize2 });
    }

    int64_t EvmCHost::table_grant(
        const struct evmc_address* address1,
        const struct evmc_address* address2,
        uint8_t const* _name,
        size_t _nameSize,
        uint8_t const* _row,
        size_t _rowSize) 
    {
        return m_extVM.table_grant(address1, address2,
            bytesConstRef{ _name, _nameSize }, bytesConstRef{ _row, _rowSize });
    }

    evmc_uint256be EvmCHost::table_get_handle(
        const struct evmc_address* address,
        uint8_t const* _name,
        size_t _nameSize,
        uint8_t const* _raw,
        size_t _rawSize) 
    {
        return m_extVM.table_get_handle(address,
            bytesConstRef{ _name, _nameSize }, bytesConstRef{ _raw, _rawSize });
    }

    evmc_uint256be EvmCHost::table_get_lines(
        const struct evmc_uint256be* handle) 
    {
        return m_extVM.table_get_lines(handle);
    }

    evmc_uint256be EvmCHost::table_get_columns(
        const struct evmc_uint256be* handle) 
    {
        return m_extVM.table_get_columns(handle);
    }

    // TODO: Remove the copy to outBuf
    size_t EvmCHost::get_column_by_name(
        const evmc_uint256be* _handle,
        size_t _row,
        uint8_t const* _column,
        size_t _columnSize,
        uint8_t* _outBuf,
        size_t _outSize) 
    {
        size_t size = m_extVM.table_get_by_key(_handle, _row,
            bytesConstRef{ _column, _columnSize }, _outBuf + 32, _outSize - 32);
        memcpy(_outBuf, ripple::toEvmC((ripple::uint256)size).bytes, 32);
        return size;
    }

    size_t EvmCHost::get_column_by_index(
        const evmc_uint256be *_handle,
        size_t _row,
        size_t _column,
        uint8_t *_outBuf,
        size_t _outSize) 
    {
        size_t size = m_extVM.table_get_by_index(_handle, _row, _column,
            _outBuf + 32, _outSize - 32);
        memcpy(_outBuf, ripple::toEvmC((ripple::uint256)size).bytes, 32);
        return size;
    }

    void EvmCHost::db_trans_begin()
    {
        m_extVM.db_trans_begin();
    }
    int64_t EvmCHost::db_trans_submit()
    {
        return m_extVM.db_trans_submit();
    }

    void EvmCHost::release_resource()
    {
        m_extVM.release_resource();
    }

    evmc_uint256be EvmCHost::get_column_len_by_name(
        const evmc_uint256be *_handle,
        size_t _row,
        const uint8_t *_column,
        size_t _size) 
    {
        return m_extVM.get_column_len(_handle, _row, bytesConstRef{ _column, _size });
    }

    evmc_uint256be EvmCHost::get_column_len_by_index(
        const evmc_uint256be *_handle,
        size_t _row,
        size_t _column) 
    {
        return m_extVM.get_column_len(_handle, _row, _column);
    }

    int64_t EvmCHost::account_set(
        const struct evmc_address *_address,
        uint32_t _uFlag,
        bool _bSet
    ) noexcept
    {
        return m_extVM.account_set(_address, _uFlag, _bSet);
    }

    int64_t EvmCHost::transfer_fee_set(
        const struct evmc_address *address,
        uint8_t const *_pRate, size_t _rateLen,
        uint8_t const *_pMin, size_t _minLen,
        uint8_t const *_pMax, size_t _maxLen
    ) noexcept
    {
        return m_extVM.transfer_fee_set(address, bytesConstRef{ _pRate, _rateLen }, bytesConstRef{ _pMin, _minLen }, bytesConstRef{ _pMax, _maxLen });
    }

    int64_t EvmCHost::trust_set(
        const struct evmc_address *address,
        uint8_t const *_pValue, size_t _valueLen,
        uint8_t const *_pCurrency, size_t _currencyLen,
        const struct evmc_address *gateWay
    ) noexcept
    {
        return m_extVM.trust_set(address, bytesConstRef{ _pValue, _valueLen }, bytesConstRef{ _pCurrency, _currencyLen }, gateWay);
    }

    int64_t EvmCHost::trust_limit(
        /* evmc_uint256be* o_result, */
        const struct evmc_address *address,
        uint8_t const *_pCurrency, size_t _currencyLen,
        uint64_t _power,
        const struct evmc_address *gateWay
    ) noexcept
    {
        //*o_result = env.trust_limit(address, bytesConstRef{ _pCurrency, _currencyLen }, gateWay);
        return m_extVM.trust_limit(address, bytesConstRef{ _pCurrency, _currencyLen }, _power, gateWay);
    }

    int64_t EvmCHost::gateway_balance(
        /* evmc_uint256be* o_result, */
        const struct evmc_address *address,
        uint8_t const *_pCurrency, size_t _currencyLen,
        uint64_t _power,
        const struct evmc_address *gateWay
    ) noexcept
    {
        //*o_result = env.gateway_balance(address, bytesConstRef{ _pCurrency, _currencyLen }, gateWay);
        return m_extVM.gateway_balance(address, bytesConstRef{ _pCurrency, _currencyLen }, _power, gateWay);
    }

    int64_t EvmCHost::pay(
        const struct evmc_address *address,
        const struct evmc_address *receiver,
        uint8_t const *_pValue, size_t _valueLen,
        uint8_t const *_pSendMax, size_t _sendMaxLen,
        uint8_t const *_pCurrency, size_t _currencyLen,
        const struct evmc_address *gateWay
    ) noexcept
    {
        return m_extVM.pay(address, receiver, bytesConstRef{ _pValue, _valueLen },
            bytesConstRef{ _pSendMax, _sendMaxLen }, bytesConstRef{ _pCurrency, _currencyLen }, gateWay);
    }

    ExtVMFace::ExtVMFace(
        EnvInfo const& envInfo,
        evmc_address _myAddress,
        evmc_address _caller,
        evmc_address _origin,
        evmc_uint256be _value,
        evmc_uint256be _gasPrice,
        bytesConstRef _data,
        bytes _code,
        evmc_uint256be _codeHash,
        int32_t _depth,
        bool _isCreate,
        bool _staticCall)
        : myAddress(_myAddress)
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
        , dropsPerByte(1000)
        , envInfo_(envInfo)
    {
    }

} // namespace eth
