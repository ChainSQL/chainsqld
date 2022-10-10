// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.
#pragma once

#include "VMConfig.h"

#include <eth/vm/VMFace.h>
#include <intx/include/intx/intx.hpp>

#include <eth/evmc/include/evmc/evmc.h>
#include <eth/evmc/include/evmc/instructions.h>

#include <boost/optional.hpp>

//todo: the version should be in build info.
#define INTERPRETER_VERSION "0.0.1"

namespace eth
{

struct VMSchedule
{
    static constexpr int64_t stackLimit = 1024;
    static constexpr int64_t stepGas0 = 0;
    static constexpr int64_t stepGas1 = 2;
    static constexpr int64_t stepGas2 = 3;
    static constexpr int64_t stepGas3 = 5;
    static constexpr int64_t stepGas4 = 8;
    static constexpr int64_t stepGas5 = 10;
    static constexpr int64_t stepGas6 = 20;
    static constexpr int64_t sha3Gas = 30;
    static constexpr int64_t sha3WordGas = 6;
    static constexpr int64_t sstoreSetGas = 20000;
    static constexpr int64_t sstoreResetGas = 5000;
    static constexpr int64_t jumpdestGas = 1;
    static constexpr int64_t logGas = 375;
    static constexpr int64_t logDataGas = 8;
    static constexpr int64_t logTopicGas = 375;
    static constexpr int64_t createGas = 32000;
    static constexpr int64_t memoryGas = 3;
    static constexpr int64_t quadCoeffDiv = 512;
    static constexpr int64_t copyGas = 3;
    static constexpr int64_t valueTransferGas = 9000;
    static constexpr int64_t callStipend = 2300;
    static constexpr int64_t callNewAccount = 25000;
    static constexpr int64_t callSelfGas = 40;
    static constexpr int64_t sqlGas = 100;
    static constexpr int64_t sqlDataGas = 100;
    static constexpr int64_t tokenGas = 100;
    static uint64_t  dropsPerByte;
};

class VM
{
public:
    static bool initMetrics();

    VM() = default;

    owning_bytes_ref exec(const evmc_host_interface* _host, evmc_host_context* _context,
        evmc_revision _rev, const evmc_message* _msg, uint8_t const* _code, size_t _codeSize);

    uint64_t m_io_gas = 0;
    int m_exception = 0;
private:
    const evmc_host_interface* m_host = nullptr;
    evmc_host_context* m_context = nullptr;
    evmc_revision m_rev = EVMC_FRONTIER;
    std::array<evmc_instruction_metrics, 256>* m_metrics = nullptr;
    evmc_message const* m_message = nullptr;
    boost::optional<evmc_tx_context> m_tx_context;
    static std::array<std::array<evmc_instruction_metrics, 256>, EVMC_MAX_REVISION + 1> s_metrics;
    void copyCode(int);
    typedef void (VM::*MemFnPtr)();
    MemFnPtr m_bounce = nullptr;
    uint64_t m_nSteps = 0;
    // return bytes
    owning_bytes_ref m_output;

    // space for memory
    bytes m_mem;

    uint8_t const* m_pCode = nullptr;
    size_t m_codeSize = 0;
    // space for code
    bytes m_code;

    /// RETURNDATA buffer for memory returned from direct subcalls.
    bytes m_returnData;

    // space for data stack, grows towards smaller addresses from the end
	intx::uint256 m_stack[VMSchedule::stackLimit];
	intx::uint256 *m_stackEnd = &m_stack[VMSchedule::stackLimit];
    size_t stackSize() { return m_stackEnd - m_SP; }
    
    // constant pool
    std::vector<intx::uint256> m_pool;

    // interpreter state
    Instruction m_OP;         // current operation
    uint64_t m_PC = 0;        // program counter
	intx::uint256* m_SP = m_stackEnd;  // stack pointer
	intx::uint256* m_SPP = m_SP;       // stack pointer prime (next SP)

    // metering and memory state
    uint64_t m_runGas = 0;
    uint64_t m_newMemSize = 0;
    uint64_t m_copyMemSize = 0;

    // initialize interpreter
    void initEntry();
    void optimize();

    // interpreter loop & switch
    void interpretCases();

    // interpreter cases that call out
    void caseCreate();
    bool caseCallSetup(evmc_message& _msg, bytesRef& o_output);
    void caseCall();

    void ter2ReturnData(int64_t);
    void copyDataToMemory(bytesConstRef _data, intx::uint256*_sp);
    uint64_t memNeed(intx::uint256 const& _offset, intx::uint256 const& _size);

    const evmc_tx_context& getTxContext();

    void throwOutOfGas();
    void throwInvalidInstruction();
    void throwBadInstruction();
    void throwBadJumpDestination();
    void throwBadStack(int _removed, int _added);
    void throwRevertInstruction(owning_bytes_ref&& _output);
    void throwRevertDiyInstruction(owning_bytes_ref&& _output);
    void throwDisallowedStateChange();
    void throwBufferOverrun(intx::uint512 const& _enfOfAccess);

    std::vector<uint64_t> m_beginSubs;
    std::vector<uint64_t> m_jumpDests;
    int64_t verifyJumpDest(intx::uint256 const& _dest, bool _throw = true);

    void onOperation() {}
    void adjustStack(int _removed, int _added);
    uint64_t gasForMem(intx::uint512 const& _size);
    void updateIOGas();
    void updateGas();
    void updateMem(uint64_t _newMem);
    void logGasMem();
    void tableGasMem(intx::uint256 memBegin, intx::uint256 byteLens);
    void int64ToIntxUint256(int64_t num);
    void fetchInstruction();
    
    uint64_t decodeJumpDest(const byte* const _code, uint64_t& _pc);
    uint64_t decodeJumpvDest(const byte* const _code, uint64_t& _pc, byte _voff);

    template<class T> uint64_t toInt63(T v)
    {
        // check for overflow
        if (v > 0x7FFFFFFFFFFFFFFF)
            throwOutOfGas();
        uint64_t w = uint64_t(v);
        return w;
    }
    
    template<class T> uint64_t toInt15(T v)
    {
        // check for overflow
        if (v > 0x7FFF)
            throwOutOfGas();
        uint64_t w = uint64_t(v);
        return w;
    }
};

}
