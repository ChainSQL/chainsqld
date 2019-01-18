#include "Compiler.h"

#include <fstream>
#include <chrono>
#include <sstream>
#include <iostream>
#include <string>

#include "preprocessor/llvm_includes_start.h"
#include <llvm/IR/CFG.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/Support/raw_ostream.h>
#include "preprocessor/llvm_includes_end.h"

#include "JIT.h"
#include "Instruction.h"
#include "Type.h"
#include "Memory.h"
#include "Ext.h"
#include "GasMeter.h"
#include "Utils.h"
#include "Endianness.h"
#include "Arith256.h"
#include "RuntimeManager.h"

#ifndef __has_cpp_attribute
  #define __has_cpp_attribute(x) 0
#endif

#if __has_cpp_attribute(fallthrough)
  #define FALLTHROUGH [[fallthrough]]
#elif __has_cpp_attribute(clang::fallthrough)
  #define FALLTHROUGH [[clang::fallthrough]]
#elif __has_cpp_attribute(gnu::fallthrough)
  #define FALLTHROUGH [[gnu::fallthrough]]
#else
  #define FALLTHROUGH
#endif

namespace dev
{
namespace eth
{
namespace jit
{

static const auto c_destIdxLabel = "destIdx";

Compiler::Compiler(Options const& _options, evmc_revision _rev, bool _staticCall, llvm::LLVMContext& _llvmContext):
	m_options(_options),
	m_rev(_rev),
	m_staticCall(_staticCall),
	m_builder(_llvmContext)
{
	Type::init(m_builder.getContext());
}

std::vector<BasicBlock> Compiler::createBasicBlocks(code_iterator _codeBegin, code_iterator _codeEnd)
{
	/// Helper function that skips push data and finds next iterator (can be the end)
	auto skipPushDataAndGetNext = [](code_iterator _curr, code_iterator _end)
	{
		static const auto push1  = static_cast<size_t>(Instruction::PUSH1);
		static const auto push32 = static_cast<size_t>(Instruction::PUSH32);
		size_t offset = 1;
		if (*_curr >= push1 && *_curr <= push32)
			offset += std::min<size_t>(*_curr - push1 + 1, (_end - _curr) - 1);
		return _curr + offset;
	};

	std::vector<BasicBlock> blocks;

	bool isDead = false;
	auto begin = _codeBegin; // begin of current block
	for (auto curr = begin, next = begin; curr != _codeEnd; curr = next)
	{
		next = skipPushDataAndGetNext(curr, _codeEnd);

		if (isDead)
		{
			if (Instruction(*curr) == Instruction::JUMPDEST)
			{
				isDead = false;
				begin = curr;
			}
			else
				continue;
		}

		bool isEnd = false;
		switch (Instruction(*curr))
		{
		case Instruction::JUMP:
		case Instruction::RETURN:
		case Instruction::REVERT:
        case Instruction::REVERTDIY:
		case Instruction::STOP:
		case Instruction::SUICIDE:
			isDead = true;
			FALLTHROUGH;
		case Instruction::JUMPI:
			isEnd = true;
			break;

		default:
			break;
		}

		assert(next <= _codeEnd);
		if (next == _codeEnd || Instruction(*next) == Instruction::JUMPDEST)
			isEnd = true;

		if (isEnd)
		{
			auto beginIdx = begin - _codeBegin;
			blocks.emplace_back(beginIdx, begin, next, m_mainFunc);
			begin = next;
		}
	}

	return blocks;
}

void Compiler::resolveJumps()
{
	auto jumpTable = llvm::cast<llvm::SwitchInst>(m_jumpTableBB->getTerminator());
	auto jumpTableInput = llvm::cast<llvm::PHINode>(m_jumpTableBB->begin());

	// Iterate through all EVM instructions blocks (skip first one and last 4 - special blocks).
	for (auto it = std::next(m_mainFunc->begin()), end = std::prev(m_mainFunc->end(), 4); it != end; ++it)
	{
		auto nextBlockIter = it;
		++nextBlockIter; // If the last code block, that will be "stop" block.
		auto currentBlockPtr = &(*it);
		auto nextBlockPtr = &(*nextBlockIter);
		
		auto term = it->getTerminator();
		llvm::BranchInst* jump = nullptr;

		if (!term) // Block may have no terminator if the next instruction is a jump destination.
			IRBuilder{currentBlockPtr}.CreateBr(nextBlockPtr);
		else if ((jump = llvm::dyn_cast<llvm::BranchInst>(term)) && jump->getSuccessor(0) == m_jumpTableBB)
		{
			auto destIdx = llvm::cast<llvm::ValueAsMetadata>(jump->getMetadata(c_destIdxLabel)->getOperand(0))->getValue();
			if (auto constant = llvm::dyn_cast<llvm::ConstantInt>(destIdx))
			{
				// If destination index is a constant do direct jump to the destination block.
				auto bb = jumpTable->findCaseValue(constant)->getCaseSuccessor();
				jump->setSuccessor(0, bb);
			}
			else
				jumpTableInput->addIncoming(destIdx, currentBlockPtr); // Fill up PHI node

			if (jump->isConditional())
				jump->setSuccessor(1, &(*nextBlockIter)); // Set next block for conditional jumps
		}
	}

	auto simplifiedInput = jumpTableInput->getNumIncomingValues() == 0 ?
			llvm::UndefValue::get(jumpTableInput->getType()) :
			jumpTableInput->hasConstantValue();
	if (simplifiedInput)
	{
		jumpTableInput->replaceAllUsesWith(simplifiedInput);
		jumpTableInput->eraseFromParent();
	}
}

std::unique_ptr<llvm::Module> Compiler::compile(code_iterator _begin, code_iterator _end, std::string const& _id)
{
	auto module = llvm::make_unique<llvm::Module>(_id, m_builder.getContext()); // TODO: Provide native DataLayout

	// Create main function
	auto mainFuncType = llvm::FunctionType::get(Type::MainReturn, Type::RuntimePtr, false);
	m_mainFunc = llvm::Function::Create(mainFuncType, llvm::Function::ExternalLinkage, _id, module.get());
	m_mainFunc->args().begin()->setName("rt");

	// Create entry basic block
	auto entryBB = llvm::BasicBlock::Create(m_builder.getContext(), "Entry", m_mainFunc);

	auto blocks = createBasicBlocks(_begin, _end);

 	// Special "Stop" block. Guarantees that there exists a next block after the code blocks (also when there are no code blocks).
	auto stopBB = llvm::BasicBlock::Create(m_mainFunc->getContext(), "Stop", m_mainFunc);
	m_jumpTableBB = llvm::BasicBlock::Create(m_mainFunc->getContext(), "JumpTable", m_mainFunc);
	auto abortBB = llvm::BasicBlock::Create(m_mainFunc->getContext(), "Abort", m_mainFunc);

	m_builder.SetInsertPoint(m_jumpTableBB); // Must be before basic blocks compilation
	auto target = m_builder.CreatePHI(Type::Word, 16, "target");
	m_builder.CreateSwitch(target, abortBB);

	m_builder.SetInsertPoint(entryBB);


	// Init runtime structures.
	RuntimeManager runtimeManager(m_builder, _begin, _end);
	GasMeter gasMeter(m_builder, runtimeManager, m_rev);
	Memory memory(runtimeManager, gasMeter);
	Ext ext(runtimeManager, memory);
	Arith256 arith(m_builder);

	auto jmpBufWords = m_builder.CreateAlloca(Type::BytePtr, m_builder.getInt64(3), "jmpBuf.words");
	auto frameaddress = llvm::Intrinsic::getDeclaration(module.get(), llvm::Intrinsic::frameaddress);
	auto fp = m_builder.CreateCall(frameaddress, m_builder.getInt32(0), "fp");
	m_builder.CreateStore(fp, jmpBufWords);
	auto stacksave = llvm::Intrinsic::getDeclaration(module.get(), llvm::Intrinsic::stacksave);
	auto sp = m_builder.CreateCall(stacksave, {}, "sp");
	auto jmpBufSp = m_builder.CreateConstInBoundsGEP1_64(jmpBufWords, 2, "jmpBuf.sp");
	m_builder.CreateStore(sp, jmpBufSp);
	auto setjmp = llvm::Intrinsic::getDeclaration(module.get(), llvm::Intrinsic::eh_sjlj_setjmp);
	auto jmpBuf = m_builder.CreateBitCast(jmpBufWords, Type::BytePtr, "jmpBuf");
	auto r = m_builder.CreateCall(setjmp, jmpBuf);
	auto normalFlow = m_builder.CreateICmpEQ(r, m_builder.getInt32(0));
	runtimeManager.setJmpBuf(jmpBuf);
	m_builder.CreateCondBr(normalFlow, entryBB->getNextNode(), abortBB, Type::expectTrue);

	for (auto& block: blocks)
		compileBasicBlock(block, runtimeManager, arith, memory, ext, gasMeter);

	// Code for special blocks:
	m_builder.SetInsertPoint(stopBB);
	runtimeManager.exit(ReturnCode::Stop);

	m_builder.SetInsertPoint(abortBB);
	runtimeManager.exit(ReturnCode::OutOfGas);

	resolveJumps();

	return module;
}


void Compiler::compileBasicBlock(BasicBlock& _basicBlock, RuntimeManager& _runtimeManager,
								 Arith256& _arith, Memory& _memory, Ext& _ext, GasMeter& _gasMeter)
{
	m_builder.SetInsertPoint(_basicBlock.llvm());
	LocalStack stack{m_builder, _runtimeManager};

	for (auto it = _basicBlock.begin(); it != _basicBlock.end(); ++it)
	{
		auto inst = Instruction(*it);

		_gasMeter.count(inst);

		switch (inst)
		{

		case Instruction::ADD:
		{
			auto lhs = stack.pop();
			auto rhs = stack.pop();
			auto result = m_builder.CreateAdd(lhs, rhs);
			stack.push(result);
			break;
		}

		case Instruction::SUB:
		{
			auto lhs = stack.pop();
			auto rhs = stack.pop();
			auto result = m_builder.CreateSub(lhs, rhs);
			stack.push(result);
			break;
		}

		case Instruction::MUL:
		{
			auto lhs = stack.pop();
			auto rhs = stack.pop();
			auto res = m_builder.CreateMul(lhs, rhs);
			stack.push(res);
			break;
		}

		case Instruction::DIV:
		{
			auto d = stack.pop();
			auto n = stack.pop();
			auto divByZero = m_builder.CreateICmpEQ(n, Constant::get(0));
			n = m_builder.CreateSelect(divByZero, Constant::get(1), n); // protect against hardware signal
			auto r = m_builder.CreateUDiv(d, n);
			r = m_builder.CreateSelect(divByZero, Constant::get(0), r);
			stack.push(r);
			break;
		}

		case Instruction::SDIV:
		{
			auto d = stack.pop();
			auto n = stack.pop();
			auto divByZero = m_builder.CreateICmpEQ(n, Constant::get(0));
			auto divByMinusOne = m_builder.CreateICmpEQ(n, Constant::get(-1));
			n = m_builder.CreateSelect(divByZero, Constant::get(1), n); // protect against hardware signal
			auto r = m_builder.CreateSDiv(d, n);
			r = m_builder.CreateSelect(divByZero, Constant::get(0), r);
			auto dNeg = m_builder.CreateSub(Constant::get(0), d);
			r = m_builder.CreateSelect(divByMinusOne, dNeg, r); // protect against undef i256.min / -1
			stack.push(r);
			break;
		}

		case Instruction::MOD:
		{
			auto d = stack.pop();
			auto n = stack.pop();
			auto divByZero = m_builder.CreateICmpEQ(n, Constant::get(0));
			n = m_builder.CreateSelect(divByZero, Constant::get(1), n); // protect against hardware signal
			auto r = m_builder.CreateURem(d, n);
			r = m_builder.CreateSelect(divByZero, Constant::get(0), r);
			stack.push(r);
			break;
		}

		case Instruction::SMOD:
		{
			auto d = stack.pop();
			auto n = stack.pop();
			auto divByZero = m_builder.CreateICmpEQ(n, Constant::get(0));
			auto divByMinusOne = m_builder.CreateICmpEQ(n, Constant::get(-1));
			n = m_builder.CreateSelect(divByZero, Constant::get(1), n); // protect against hardware signal
			auto r = m_builder.CreateSRem(d, n);
			r = m_builder.CreateSelect(divByZero, Constant::get(0), r);
			r = m_builder.CreateSelect(divByMinusOne, Constant::get(0), r); // protect against undef i256.min / -1
			stack.push(r);
			break;
		}

		case Instruction::ADDMOD:
		{
			auto i512Ty = m_builder.getIntNTy(512);
			auto a = stack.pop();
			auto b = stack.pop();
			auto m = stack.pop();
			auto divByZero = m_builder.CreateICmpEQ(m, Constant::get(0));
			a = m_builder.CreateZExt(a, i512Ty);
			b = m_builder.CreateZExt(b, i512Ty);
			m = m_builder.CreateZExt(m, i512Ty);
			auto s = m_builder.CreateNUWAdd(a, b);
			s = m_builder.CreateURem(s, m);
			s = m_builder.CreateTrunc(s, Type::Word);
			s = m_builder.CreateSelect(divByZero, Constant::get(0), s);
			stack.push(s);
			break;
		}

		case Instruction::MULMOD:
		{
			auto i512Ty = m_builder.getIntNTy(512);
			auto a = stack.pop();
			auto b = stack.pop();
			auto m = stack.pop();
			auto divByZero = m_builder.CreateICmpEQ(m, Constant::get(0));
			a = m_builder.CreateZExt(a, i512Ty);
			b = m_builder.CreateZExt(b, i512Ty);
			m = m_builder.CreateZExt(m, i512Ty);
			auto p = m_builder.CreateNUWMul(a, b);
			p = m_builder.CreateURem(p, m);
			p = m_builder.CreateTrunc(p, Type::Word);
			p = m_builder.CreateSelect(divByZero, Constant::get(0), p);
			stack.push(p);
			break;
		}

		case Instruction::EXP:
		{
			auto base = stack.pop();
			auto exponent = stack.pop();
			_gasMeter.countExp(exponent);
			auto ret = _arith.exp(base, exponent);
			stack.push(ret);
			break;
		}

		case Instruction::NOT:
		{
			auto value = stack.pop();
			auto ret = m_builder.CreateXor(value, Constant::get(-1), "bnot");
			stack.push(ret);
			break;
		}

		case Instruction::LT:
		{
			auto lhs = stack.pop();
			auto rhs = stack.pop();
			auto res1 = m_builder.CreateICmpULT(lhs, rhs);
			auto res256 = m_builder.CreateZExt(res1, Type::Word);
			stack.push(res256);
			break;
		}

		case Instruction::GT:
		{
			auto lhs = stack.pop();
			auto rhs = stack.pop();
			auto res1 = m_builder.CreateICmpUGT(lhs, rhs);
			auto res256 = m_builder.CreateZExt(res1, Type::Word);
			stack.push(res256);
			break;
		}

		case Instruction::SLT:
		{
			auto lhs = stack.pop();
			auto rhs = stack.pop();
			auto res1 = m_builder.CreateICmpSLT(lhs, rhs);
			auto res256 = m_builder.CreateZExt(res1, Type::Word);
			stack.push(res256);
			break;
		}

		case Instruction::SGT:
		{
			auto lhs = stack.pop();
			auto rhs = stack.pop();
			auto res1 = m_builder.CreateICmpSGT(lhs, rhs);
			auto res256 = m_builder.CreateZExt(res1, Type::Word);
			stack.push(res256);
			break;
		}

		case Instruction::EQ:
		{
			auto lhs = stack.pop();
			auto rhs = stack.pop();
			auto res1 = m_builder.CreateICmpEQ(lhs, rhs);
			auto res256 = m_builder.CreateZExt(res1, Type::Word);
			stack.push(res256);
			break;
		}

		case Instruction::ISZERO:
		{
			auto top = stack.pop();
			auto iszero = m_builder.CreateICmpEQ(top, Constant::get(0), "iszero");
			auto result = m_builder.CreateZExt(iszero, Type::Word);
			stack.push(result);
			break;
		}

		case Instruction::AND:
		{
			auto lhs = stack.pop();
			auto rhs = stack.pop();
			auto res = m_builder.CreateAnd(lhs, rhs);
			stack.push(res);
			break;
		}

		case Instruction::OR:
		{
			auto lhs = stack.pop();
			auto rhs = stack.pop();
			auto res = m_builder.CreateOr(lhs, rhs);
			stack.push(res);
			break;
		}

		case Instruction::XOR:
		{
			auto lhs = stack.pop();
			auto rhs = stack.pop();
			auto res = m_builder.CreateXor(lhs, rhs);
			stack.push(res);
			break;
		}

		case Instruction::BYTE:
		{
			const auto idx = stack.pop();
			auto value = Endianness::toBE(m_builder, stack.pop());

			auto idxValid = m_builder.CreateICmpULT(idx, Constant::get(32), "idxValid");
			auto bytes = m_builder.CreateBitCast(value, llvm::VectorType::get(Type::Byte, 32), "bytes");
			// TODO: Workaround for LLVM bug. Using big value of index causes invalid memory access.
			auto safeIdx = m_builder.CreateTrunc(idx, m_builder.getIntNTy(5));
			// TODO: Workaround for LLVM bug. DAG Builder used sext on index instead of zext
			safeIdx = m_builder.CreateZExt(safeIdx, Type::Size);
			auto byte = m_builder.CreateExtractElement(bytes, safeIdx, "byte");
			value = m_builder.CreateZExt(byte, Type::Word);
			value = m_builder.CreateSelect(idxValid, value, Constant::get(0));
			stack.push(value);
			break;
		}

		case Instruction::SIGNEXTEND:
		{
			auto idx = stack.pop();
			auto word = stack.pop();

			auto k32_ = m_builder.CreateTrunc(idx, m_builder.getIntNTy(5), "k_32");
			auto k32 = m_builder.CreateZExt(k32_, Type::Size);
			auto k32x8 = m_builder.CreateMul(k32, m_builder.getInt64(8), "kx8");

			// test for word >> (k * 8 + 7)
			auto bitpos = m_builder.CreateAdd(k32x8, m_builder.getInt64(7), "bitpos");
			auto bitposEx = m_builder.CreateZExt(bitpos, Type::Word);
			auto bitval = m_builder.CreateLShr(word, bitposEx, "bitval");
			auto bittest = m_builder.CreateTrunc(bitval, Type::Bool, "bittest");

			auto mask_ = m_builder.CreateShl(Constant::get(1), bitposEx);
			auto mask = m_builder.CreateSub(mask_, Constant::get(1), "mask");

			auto negmask = m_builder.CreateXor(mask, llvm::ConstantInt::getAllOnesValue(Type::Word), "negmask");
			auto val1 = m_builder.CreateOr(word, negmask);
			auto val0 = m_builder.CreateAnd(word, mask);

			auto kInRange = m_builder.CreateICmpULE(idx, llvm::ConstantInt::get(Type::Word, 30));
			auto result = m_builder.CreateSelect(kInRange,
												 m_builder.CreateSelect(bittest, val1, val0),
												 word);
			stack.push(result);
			break;
		}

		case Instruction::SHA3:
		{
			auto inOff = stack.pop();
			auto inSize = stack.pop();
			_memory.require(inOff, inSize);
			_gasMeter.countSha3Data(inSize);
			auto hash = _ext.sha3(inOff, inSize);
			stack.push(hash);
			break;
		}

		case Instruction::POP:
		{
			stack.pop();
			break;
		}

		case Instruction::ANY_PUSH:
		{
			auto value = readPushData(it, _basicBlock.end());
			stack.push(Constant::get(value));
			break;
		}

		case Instruction::ANY_DUP:
		{
			auto index = static_cast<size_t>(inst) - static_cast<size_t>(Instruction::DUP1);
			stack.dup(index);
			break;
		}

		case Instruction::ANY_SWAP:
		{
			auto index = static_cast<size_t>(inst) - static_cast<size_t>(Instruction::SWAP1) + 1;
			stack.swap(index);
			break;
		}

		case Instruction::MLOAD:
		{
			auto addr = stack.pop();
			auto word = _memory.loadWord(addr);
			stack.push(word);
			break;
		}

		case Instruction::MSTORE:
		{
			auto addr = stack.pop();
			auto word = stack.pop();
			_memory.storeWord(addr, word);
			break;
		}

		case Instruction::MSTORE8:
		{
			auto addr = stack.pop();
			auto word = stack.pop();
			_memory.storeByte(addr, word);
			break;
		}

		case Instruction::MSIZE:
		{
			auto word = _memory.getSize();
			stack.push(word);
			break;
		}

		case Instruction::SLOAD:
		{
			auto index = stack.pop();
			auto value = _ext.sload(index);
			stack.push(value);
			break;
		}

		case Instruction::SSTORE:
		{
			if (m_staticCall)
				goto invalidInstruction;

			auto index = stack.pop();
			auto value = stack.pop();
			_gasMeter.countSStore(_ext, index, value);
			_ext.sstore(index, value);
			break;
		}

		case Instruction::JUMP:
		case Instruction::JUMPI:
		{
			auto destIdx = llvm::MDNode::get(m_builder.getContext(), llvm::ValueAsMetadata::get(stack.pop()));

			// Create branch instruction, initially to jump table.
			// Destination will be optimized with direct jump during jump resolving if destination index is a constant.
			auto jumpInst = (inst == Instruction::JUMP) ?
					m_builder.CreateBr(m_jumpTableBB) :
					m_builder.CreateCondBr(m_builder.CreateICmpNE(stack.pop(), Constant::get(0), "jump.check"), m_jumpTableBB, nullptr);

			// Attach medatada to branch instruction with information about destination index.
			jumpInst->setMetadata(c_destIdxLabel, destIdx);
			break;
		}

		case Instruction::JUMPDEST:
		{
			// Add the basic block to the jump table.
			assert(it == _basicBlock.begin() && "JUMPDEST must be the first instruction of a basic block");
			auto jumpTable = llvm::cast<llvm::SwitchInst>(m_jumpTableBB->getTerminator());
			jumpTable->addCase(Constant::get(_basicBlock.firstInstrIdx()), _basicBlock.llvm());
			break;
		}

		case Instruction::PC:
		{
			auto value = Constant::get(it - _basicBlock.begin() + _basicBlock.firstInstrIdx());
			stack.push(value);
			break;
		}

		case Instruction::GAS:
		{
			_gasMeter.commitCostBlock();
			stack.push(m_builder.CreateZExt(_runtimeManager.getGas(), Type::Word));
			break;
		}

		case Instruction::ADDRESS:
			stack.push(Endianness::toNative(m_builder, _runtimeManager.getAddress()));
			break;
		case Instruction::CALLER:
			stack.push(Endianness::toNative(m_builder, _runtimeManager.getSender()));
			break;
		case Instruction::ORIGIN:
			stack.push(m_builder.CreateZExt(Endianness::toNative(m_builder, _runtimeManager.getTxContextItem(1)), Type::Word));
			break;
		case Instruction::COINBASE:
			stack.push(m_builder.CreateZExt(Endianness::toNative(m_builder, _runtimeManager.getTxContextItem(2)), Type::Word));
			break;

		case Instruction::GASPRICE:
			stack.push(Endianness::toNative(m_builder, _runtimeManager.getTxContextItem(0)));
			break;

		case Instruction::DIFFICULTY:
			stack.push(Endianness::toNative(m_builder, _runtimeManager.getTxContextItem(6)));
			break;

		case Instruction::GASLIMIT:
			stack.push(m_builder.CreateZExt(_runtimeManager.getTxContextItem(5), Type::Word));
			break;

		case Instruction::NUMBER:
			stack.push(m_builder.CreateZExt(_runtimeManager.getTxContextItem(3), Type::Word));
			break;

		case Instruction::TIMESTAMP:
			stack.push(m_builder.CreateZExt(_runtimeManager.getTxContextItem(4), Type::Word));
			break;

		case Instruction::CALLVALUE:
		{
			auto beValue = _runtimeManager.getValue();
			stack.push(Endianness::toNative(m_builder, beValue));
			break;
		}

		case Instruction::CODESIZE:
			stack.push(_runtimeManager.getCodeSize());
			break;

		case Instruction::CALLDATASIZE:
			stack.push(_runtimeManager.getCallDataSize());
			break;

		case Instruction::RETURNDATASIZE:
		{
			if (m_rev < EVMC_BYZANTIUM)
				goto invalidInstruction;

			auto returnBufSizePtr = _runtimeManager.getReturnBufSizePtr();
			auto returnBufSize = m_builder.CreateLoad(returnBufSizePtr);
			stack.push(m_builder.CreateZExt(returnBufSize, Type::Word));
			break;
		}

		case Instruction::BLOCKHASH:
		{
			auto number = stack.pop();
			// If number bigger than int64 assume the result is 0.
			auto limitC = m_builder.getInt64(std::numeric_limits<int64_t>::max());
			auto limit = m_builder.CreateZExt(limitC, Type::Word);
			auto isBigNumber = m_builder.CreateICmpUGT(number, limit);
			auto hash = _ext.blockHash(number);
			// TODO: Try to eliminate the call if the number is invalid.
			hash = m_builder.CreateSelect(isBigNumber, Constant::get(0), hash);
			stack.push(hash);
			break;
		}

		case Instruction::BALANCE:
		{
			auto address = stack.pop();
			auto value = _ext.balance(address);
			stack.push(value);
			break;
		}

		case Instruction::EXTCODESIZE:
		{
			auto addr = stack.pop();
			auto codesize = _ext.extcodesize(addr);
			stack.push(codesize);
			break;
		}

		case Instruction::CALLDATACOPY:
		{
			auto destMemIdx = stack.pop();
			auto srcIdx = stack.pop();
			auto reqBytes = stack.pop();

			auto srcPtr = _runtimeManager.getCallData();
			auto srcSize = _runtimeManager.getCallDataSize();

			_memory.copyBytes(srcPtr, srcSize, srcIdx, destMemIdx, reqBytes);
			break;
		}

		case Instruction::RETURNDATACOPY:
		{
			if (m_rev < EVMC_BYZANTIUM)
				goto invalidInstruction;

			auto destMemIdx = stack.pop();
			auto srcIdx = stack.pop();
			auto reqBytes = stack.pop();

			auto srcPtr = m_builder.CreateLoad(_runtimeManager.getReturnBufDataPtr());
			auto srcSize = m_builder.CreateLoad(_runtimeManager.getReturnBufSizePtr());

			_memory.copyBytesNoPadding(srcPtr, srcSize, srcIdx, destMemIdx, reqBytes);
			break;
		}

		case Instruction::CODECOPY:
		{
			auto destMemIdx = stack.pop();
			auto srcIdx = stack.pop();
			auto reqBytes = stack.pop();

			auto srcPtr = _runtimeManager.getCode();    // TODO: Code & its size are constants, feature #80814234
			auto srcSize = _runtimeManager.getCodeSize();

			_memory.copyBytes(srcPtr, srcSize, srcIdx, destMemIdx, reqBytes);
			break;
		}

		case Instruction::EXTCODECOPY:
		{
			auto addr = stack.pop();
			auto destMemIdx = stack.pop();
			auto srcIdx = stack.pop();
			auto reqBytes = stack.pop();

			auto codeRef = _ext.extcode(addr);

			_memory.copyBytes(codeRef.ptr, codeRef.size, srcIdx, destMemIdx, reqBytes);
			break;
		}

		case Instruction::CALLDATALOAD:
		{
			auto idx = stack.pop();
			auto value = _ext.calldataload(idx);
			stack.push(value);
			break;
		}

		case Instruction::CREATE:
		{
			if (m_staticCall)
				goto invalidInstruction;

			auto endowment = stack.pop();
			auto initOff = stack.pop();
			auto initSize = stack.pop();
			_memory.require(initOff, initSize);

			_gasMeter.commitCostBlock();
			auto gas = _runtimeManager.getGas();
			llvm::Value* gasKept = (m_rev >= EVMC_TANGERINE_WHISTLE) ?
			                       m_builder.CreateLShr(gas, 6) :
			                       m_builder.getInt64(0);
			auto createGas = m_builder.CreateSub(gas, gasKept, "create.gas", true, true);
			llvm::Value* r = nullptr;
			llvm::Value* pAddr = nullptr;
			std::tie(r, pAddr) = _ext.create(createGas, endowment, initOff, initSize);

			auto ret =
				m_builder.CreateICmpSGE(r, m_builder.getInt64(0), "create.ret");
			auto rmagic = m_builder.CreateSelect(
				ret, m_builder.getInt64(0), m_builder.getInt64(EVM_CALL_FAILURE),
				"call.rmagic");
			// TODO: optimize
			auto gasLeft = m_builder.CreateSub(r, rmagic, "create.gasleft");
			gas = m_builder.CreateAdd(gasLeft, gasKept);
			_runtimeManager.setGas(gas);

			llvm::Value* addr = m_builder.CreateLoad(pAddr);
			addr = Endianness::toNative(m_builder, addr);
			addr = m_builder.CreateZExt(addr, Type::Word);
			addr = m_builder.CreateSelect(ret, addr, Constant::get(0));
			stack.push(addr);
			break;
		}

		case Instruction::CALL:
		case Instruction::CALLCODE:
		case Instruction::DELEGATECALL:
		case Instruction::STATICCALL:
		{
			// Handle invalid instructions.
			if (inst == Instruction::DELEGATECALL && m_rev < EVMC_HOMESTEAD)
				goto invalidInstruction;

			if (inst == Instruction::STATICCALL && m_rev < EVMC_BYZANTIUM)
				goto invalidInstruction;

			auto callGas = stack.pop();
			auto address = stack.pop();
			bool hasValue = inst == Instruction::CALL || inst == Instruction::CALLCODE;
			auto value = hasValue ? stack.pop() : Constant::get(0);

			auto inOff = stack.pop();
			auto inSize = stack.pop();
			auto outOff = stack.pop();
			auto outSize = stack.pop();

			_gasMeter.commitCostBlock();

			// Require memory for in and out buffers
			_memory.require(outOff, outSize);  // Out buffer first as we guess
											   // it will be after the in one
			_memory.require(inOff, inSize);

			auto noTransfer = m_builder.CreateICmpEQ(value, Constant::get(0));

			// For static call mode, select infinite penalty for CALL with
			// value transfer.
			auto const transferGas = (inst == Instruction::CALL && m_staticCall) ?
				std::numeric_limits<int64_t>::max() :
				JITSchedule::valueTransferGas::value;

			auto transferCost = m_builder.CreateSelect(
					noTransfer, m_builder.getInt64(0),
					m_builder.getInt64(transferGas));
			_gasMeter.count(transferCost, _runtimeManager.getJmpBuf(),
							_runtimeManager.getGasPtr());

			if (inst == Instruction::CALL)
			{
				auto accountExists = _ext.exists(address);
				auto noPenaltyCond = accountExists;
				if (m_rev >= EVMC_SPURIOUS_DRAGON)
					noPenaltyCond = m_builder.CreateOr(accountExists, noTransfer);
				auto penalty = m_builder.CreateSelect(noPenaltyCond,
				                                      m_builder.getInt64(0),
				                                      m_builder.getInt64(JITSchedule::callNewAccount::value));
				_gasMeter.count(penalty, _runtimeManager.getJmpBuf(),
				                _runtimeManager.getGasPtr());
			}

			if (m_rev >= EVMC_TANGERINE_WHISTLE)
			{
				auto gas = _runtimeManager.getGas();
				auto gas64th = m_builder.CreateLShr(gas, 6);
				auto gasMaxAllowed = m_builder.CreateZExt(
						m_builder.CreateSub(gas, gas64th, "gas.maxallowed",
						                    true, true), Type::Word);
				auto cmp = m_builder.CreateICmpUGT(callGas, gasMaxAllowed);
				callGas = m_builder.CreateSelect(cmp, gasMaxAllowed, callGas);
			}

			_gasMeter.count(callGas, _runtimeManager.getJmpBuf(),
							_runtimeManager.getGasPtr());
			auto stipend = m_builder.CreateSelect(
					noTransfer, m_builder.getInt64(0),
					m_builder.getInt64(JITSchedule::callStipend::value));
			auto gas = m_builder.CreateTrunc(callGas, Type::Gas, "call.gas.declared");
			gas = m_builder.CreateAdd(gas, stipend, "call.gas", true, true);
			int kind = [inst]() -> int
			{
				switch (inst)
				{
				case Instruction::CALL: return EVMC_CALL;
				case Instruction::CALLCODE: return EVMC_CALLCODE;
				case Instruction::DELEGATECALL: return EVMC_DELEGATECALL;
				case Instruction::STATICCALL: return EVM_STATICCALL;
				default: LLVM_BUILTIN_UNREACHABLE;
				}
			}();
			auto r = _ext.call(kind, gas, address, value, inOff, inSize, outOff,
							   outSize);
			auto ret =
				m_builder.CreateICmpSGE(r, m_builder.getInt64(0), "call.ret");
			auto rmagic = m_builder.CreateSelect(
				ret, m_builder.getInt64(0), m_builder.getInt64(EVM_CALL_FAILURE),
				"call.rmagic");
			// TODO: optimize
			auto finalGas = m_builder.CreateSub(r, rmagic, "call.finalgas");
			_gasMeter.giveBack(finalGas);
			auto xx = m_builder.CreateZExt(ret, Type::Word);
			stack.push(xx);
			break;
		}

		case Instruction::RETURN:
		case Instruction::REVERT:
        case Instruction::REVERTDIY:
        {
            auto const isRevert = (inst == Instruction::REVERT || inst == Instruction::REVERTDIY);
            if (isRevert && m_rev < EVMC_BYZANTIUM)
                goto invalidInstruction;

            auto index = stack.pop();
            auto size = stack.pop();

            _memory.require(index, size);
            _runtimeManager.registerReturnData(index, size);
            switch (inst)
            {
            case Instruction::RETURN:
                _runtimeManager.exit(ReturnCode::Return);
                break;
            case Instruction::REVERT:
                _runtimeManager.exit(ReturnCode::Revert);
                break;
            case Instruction::REVERTDIY:
                _runtimeManager.exit(ReturnCode::RevertDiy);
                break;
            }
            break;
        }

		case Instruction::SUICIDE:
		{
			if (m_staticCall)
				goto invalidInstruction;

			auto dest = stack.pop();
			if (m_rev >= EVMC_TANGERINE_WHISTLE)
			{
				auto destExists = _ext.exists(dest);
				auto noPenaltyCond = destExists;
				if (m_rev >= EVMC_SPURIOUS_DRAGON)
				{
					auto addr = Endianness::toNative(m_builder, _runtimeManager.getAddress());
					auto balance = _ext.balance(addr);
					auto noTransfer = m_builder.CreateICmpEQ(balance,
					                                         Constant::get(0));
					noPenaltyCond = m_builder.CreateOr(destExists, noTransfer);
				}
				auto penalty = m_builder.CreateSelect(
						noPenaltyCond, m_builder.getInt64(0),
						m_builder.getInt64(JITSchedule::callNewAccount::value));
				_gasMeter.count(penalty, _runtimeManager.getJmpBuf(),
				                _runtimeManager.getGasPtr());
			}
			_ext.selfdestruct(dest);
		}
			FALLTHROUGH;
		case Instruction::STOP:
			_runtimeManager.exit(ReturnCode::Stop);
			break;

		case Instruction::LOG0:
		case Instruction::LOG1:
		case Instruction::LOG2:
		case Instruction::LOG3:
		case Instruction::LOG4:
		{
			if (m_staticCall)
				goto invalidInstruction;

			auto beginIdx = stack.pop();
			auto numBytes = stack.pop();
			_memory.require(beginIdx, numBytes);

			// This will commit the current cost block
			_gasMeter.countLogData(numBytes);

			llvm::SmallVector<llvm::Value*, 4> topics;
			auto numTopics = static_cast<size_t>(inst) - static_cast<size_t>(Instruction::LOG0);
			for (size_t i = 0; i < numTopics; ++i)
				topics.emplace_back(stack.pop());

			_ext.log(beginIdx, numBytes, topics);
			break;
		}
        /*
		case Instruction::CREATETABLE:
		{
            auto address   = stack.pop();
            auto nameIdx   = stack.pop();
            auto nameBytes = stack.pop();
            auto rawIdx    = stack.pop();
            auto rawBytes  = stack.pop();

             _memory.require(nameIdx, nameBytes);
            _memory.require(rawIdx, rawBytes);

            // This will commit the current cost block
            //_gasMeter.countLogData(numBytes);
            
            
            

            auto type = (uint8_t)inst - (uint8_t)Instruction::CREATETABLE;

            auto r = _ext.executeSQL(address, type, nameIdx, nameBytes, rawIdx, rawBytes);            
            stack.push(r);
            break;           
		}
        */
        case Instruction::CREATETABLE:
        {
            auto address = stack.pop();
            auto nameIdx = stack.pop();
            auto nameBytes = stack.pop();
            auto rawIdx = stack.pop();
            auto rawBytes = stack.pop();

            _memory.require(nameIdx, nameBytes);
            _memory.require(rawIdx, rawBytes);

            _gasMeter.countSqlData(rawBytes);

            auto r = _ext.table_create(address, nameIdx, nameBytes, rawIdx, rawBytes);

			auto ret = m_builder.CreateICmpEQ(r, m_builder.getInt64(0), "createtable.ret");
            auto xx = m_builder.CreateZExt(ret, Type::Word);

            stack.push(xx);
            break;
        }
        case Instruction::EXDROPTABLE:
        {
            auto address = stack.pop();
            auto nameIdx = stack.pop();
            auto nameBytes = stack.pop();

            _memory.require(nameIdx, nameBytes);

            auto r = _ext.table_drop(address, nameIdx, nameBytes);

            auto ret = m_builder.CreateICmpEQ(r, m_builder.getInt64(0), "createtable.ret");
            auto xx = m_builder.CreateZExt(ret, Type::Word);

            stack.push(xx);
            break;
        }
        case Instruction::EXRENAMETABLE:
        {
            auto address = stack.pop();
            auto nameIdx = stack.pop();
            auto nameBytes = stack.pop();
            auto rawIdx = stack.pop();
            auto rawBytes = stack.pop();

            _memory.require(nameIdx, nameBytes);
            _memory.require(rawIdx, rawBytes);

            _gasMeter.countSqlData(rawBytes);

            auto r = _ext.table_rename(address, nameIdx, nameBytes, rawIdx, rawBytes);

            auto ret = m_builder.CreateICmpEQ(r, m_builder.getInt64(0), "createtable.ret");
            auto xx = m_builder.CreateZExt(ret, Type::Word);

            stack.push(xx);
            break;
        }
        case Instruction::EXINSERTSQL:
        {
            auto address = stack.pop();
            auto nameIdx = stack.pop();
            auto nameBytes = stack.pop();
            auto rawIdx = stack.pop();
            auto rawBytes = stack.pop();

            _memory.require(nameIdx, nameBytes);
            _memory.require(rawIdx, rawBytes);

            _gasMeter.countSqlData(rawBytes);

            auto r = _ext.table_insert(address, nameIdx, nameBytes, rawIdx, rawBytes);

            auto ret = m_builder.CreateICmpEQ(r, m_builder.getInt64(0), "createtable.ret");
            auto xx = m_builder.CreateZExt(ret, Type::Word);

            stack.push(xx);
            break;
        }
        case Instruction::EXDELETESQL:
        {
            auto address = stack.pop();
            auto nameIdx = stack.pop();
            auto nameBytes = stack.pop();
            auto rawIdx = stack.pop();
            auto rawBytes = stack.pop();

            _memory.require(nameIdx, nameBytes);
            _memory.require(rawIdx, rawBytes);

            _gasMeter.countSqlData(rawBytes);

            auto r = _ext.table_delete(address, nameIdx, nameBytes, rawIdx, rawBytes);

            auto ret = m_builder.CreateICmpEQ(r, m_builder.getInt64(0), "createtable.ret");
            auto xx = m_builder.CreateZExt(ret, Type::Word);

            stack.push(xx);
            break;
        }
        case Instruction::EXUPDATESQL:
        {
            auto address   = stack.pop();
            auto nameIdx   = stack.pop();
            auto nameBytes = stack.pop();
            auto rawIdx1   = stack.pop();
            auto rawBytes1 = stack.pop();
            auto rawIdx2   = stack.pop();
            auto rawBytes2 = stack.pop();

            _memory.require(nameIdx, nameBytes);
            _memory.require(rawIdx1, rawBytes1);
            _memory.require(rawIdx2, rawBytes2);

            _gasMeter.countSqlData(rawBytes1);
            _gasMeter.countSqlData(rawBytes2);
            

            auto r = _ext.table_update(address, nameIdx, nameBytes, rawIdx1, rawBytes1, rawIdx2, rawBytes2);

            auto ret = m_builder.CreateICmpEQ(r, m_builder.getInt64(0), "createtable.ret");
            auto xx = m_builder.CreateZExt(ret, Type::Word);

            stack.push(xx);
            break;
        }
        case Instruction::EXSELECTSQL:
        {
            auto address = stack.pop();
            auto nameIdx = stack.pop();
            auto nameBytes = stack.pop();
            auto rawIdx = stack.pop();
            auto rawBytes = stack.pop();

            _memory.require(nameIdx, nameBytes);
            _memory.require(rawIdx, rawBytes);

            _gasMeter.countSqlData(rawBytes);

            auto r = _ext.table_get_handle(address, nameIdx, nameBytes, rawIdx, rawBytes);
            stack.push(r);
            break;
        }
        case Instruction::EXGRANTSQL:
        {
            auto addOwner  = stack.pop();
            auto addDest   = stack.pop();
            auto nameIdx   = stack.pop();
            auto nameBytes = stack.pop();
            auto rawIdx    = stack.pop();
            auto rawBytes  = stack.pop();

            _memory.require(nameIdx, nameBytes);
            _memory.require(rawIdx, rawBytes);

            _gasMeter.countSqlData(rawBytes);

            auto r = _ext.table_grant(addOwner, addDest, 
                    nameIdx, nameBytes, rawIdx, rawBytes);

            auto ret = m_builder.CreateICmpEQ(r, m_builder.getInt64(0), "createtable.ret");
            auto xx = m_builder.CreateZExt(ret, Type::Word);

            stack.push(xx);
            break;
        }
        case Instruction::EXTRANSBEGIN:
        {
            _ext.db_trans_begin();
            break;
        }
        case Instruction::EXTRANSCOMMIT:
        {
            auto r = _ext.db_trans_submit();

            auto ret = m_builder.CreateICmpEQ(r, m_builder.getInt64(0), "createtable.ret");
            auto xx = m_builder.CreateZExt(ret, Type::Word);

            stack.push(xx);
            break;
        }
        case Instruction::EXGETROWSIZE:
        {
            auto handle = stack.pop();

            auto r = _ext.table_get_lines(handle);
            stack.push(r);
            break;
        }
        case Instruction::EXGETCOLSIZE:
        {
            auto handle = stack.pop();

            auto r = _ext.table_get_columns(handle);
            stack.push(r);
            break;
        }
        case Instruction::EXGETVALUEBYKEY:
        {
            auto handle = stack.pop();
            auto row = stack.pop();
            auto columnOff = stack.pop();
            auto columnSize = stack.pop();
            auto outOff = stack.pop();
            auto outSize = stack.pop();

            _memory.require(columnOff, columnSize);
            _memory.require(outOff, outSize);

            _ext.table_get_column(handle, row, columnOff, columnSize, 
                    outOff, outSize);
            stack.push(m_builder.CreateZExt(outOff, Type::Word));

            break;
        }
        case Instruction::EXGETVALUEBYINDEX:
        {
            auto handle = stack.pop();
            auto row = stack.pop();
            auto colomn = stack.pop();
            auto outOff = stack.pop();
            auto outSize = stack.pop();

            _memory.require(outOff, outSize);
            _ext.table_get_column(handle, row, colomn, outOff, outSize);
            stack.push(m_builder.CreateZExt(outOff, Type::Word));

            break;
        }
        case Instruction::EXEXITFUNC:
        {
            _ext.exit_fun();
            break;
        }
        case Instruction::EXGETLENBYKEY:
        {
            auto handle = stack.pop();
            auto row = stack.pop();
            auto columnOff = stack.pop();
            auto columnSize = stack.pop();

            _memory.require(columnOff, columnSize);
            auto r = _ext.get_column_len(handle, row, columnOff, columnSize);
            stack.push(r);

            break; 
        }
        case Instruction::EXGETLENBYINDEX:
        {
            auto handle = stack.pop();
            auto row = stack.pop();
            auto column = stack.pop();

            auto r = _ext.get_column_len(handle, row, column);
            stack.push(r);

            break;
        }

		invalidInstruction:
		default: // Invalid instruction - abort
			_runtimeManager.exit(ReturnCode::OutOfGas);
			it = _basicBlock.end() - 1; // finish block compilation
		}
	}

	_gasMeter.commitCostBlock();

	stack.finalize();
}


}
}
}
