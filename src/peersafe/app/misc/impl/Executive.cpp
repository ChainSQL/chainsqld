#include <peersafe/app/misc/Executive.h>
#include <ripple/protocol/digest.h>
#include <ripple/app/main/Application.h>
#include <peersafe/vm/VMFactory.h>

namespace ripple {

Executive::Executive(SleOps const& _s, EnvInfo const& _envInfo, unsigned int _level)
	:m_s(_s),m_envInfo(_envInfo),m_depth(_level)
{
}

void Executive::initialize(STTx const& _transaction) {
	m_t.reset(&_transaction);
}

bool Executive::finalize() {
	//Todo:deal with balance:
	//if (m_t)
	//{
	//	m_s.addBalance(m_t.sender(), m_gas * m_t.gasPrice());

	//	u256 feesEarned = (m_t.gas() - m_gas) * m_t.gasPrice();
	//	m_s.addBalance(m_envInfo.author(), feesEarned);
	//}
	return true;
}

uint256 Executive::gasUsed() const
{
	//return m_t.gas() - m_gas;
	return beast::zero
}

bool Executive::execute() {
	// Entry point for a user-executed transaction.

	// Pay...
	//clog(StateDetail) << "Paying" << formatBalance(m_gasCost) << "from sender for gas (" << m_t.gas() << "gas at" << formatBalance(m_t.gasPrice()) << ")";
	//m_s.subBalance(m_t.sender(), m_gasCost);

	//assert(m_t.gas() >= (u256)m_baseGasRequired);
	bool isCreation = m_t->getFieldU16(sfContractOpType) == 1;
	evmc_address sender = toEvmC(m_t->getAccountID(sfAccount));
	evmc_address contract_address = toEvmC(m_t->getAccountID(sfContractAddress));
	if (isCreation)
		return create(m_t.sender(), m_t.value(), m_t.gasPrice(), m_t.gas() - (u256)m_baseGasRequired, &m_t.data(), m_t.sender());
	else
		return call(m_t.receiveAddress(), m_t.sender(), m_t.value(), m_t.gasPrice(), bytesConstRef(&m_t.data()), m_t.gas() - (u256)m_baseGasRequired);
}

bool Executive::create(evmc_address const& _txSender, uint256 const& _endowment,
	uint256 const& _gasPrice, uint256 const& _gas, bytesConstRef _code, evmc_address const& _originAddress) 
{
	return createOpcode(_txSender, _endowment, _gasPrice, _gas, _code, _originAddress);
}

bool Executive::createOpcode(evmc_address const& _sender, uint256 const& _endowment,
	uint256 const& _gasPrice, uint256 const& _gas, bytesConstRef _code, evmc_address const& _originAddress)
{
	SLE::pointer pSle = m_s.getSle(_sender);
	uint32 nonce = pSle->getFieldU32(sfNonce);
	//m_newAddress = _sender + nonce;
	return executeCreate(_sender, _endowment, _gasPrice, _gas, _code, _originAddress);
}
///// @returns false iff go() must be called (and thus a VM execution in required).
//bool create2Opcode(evmc_address const& _sender, uint256 const& _endowment,
//	uint256 const& _gasPrice, uint256 const& _gas, bytesConstRef _code, evmc_address const& _originAddress, uint256 const& _salt);
/// Set up the executive for evaluating a bare CALL (message call) operation.
/// @returns false iff go() must be called (and thus a VM execution in required).
bool Executive::call(evmc_address const& _receiveAddress, evmc_address const& _senderAddress,
	uint256 const& _value, uint256 const& _gasPrice, bytesConstRef _data, int64_t const& _gas)
{
	CallParameters params{ _senderAddress, _receiveAddress, _receiveAddress, toEvmC(_value), toEvmC(_value), _gas, _data };
	return call(params, _gasPrice, _senderAddress);
}

bool Executive::call(CallParameters const& _cp, uint256 const& _gasPrice, evmc_address const& _origin)
{
	// If external transaction.
	if (m_t)
	{
		// FIXME: changelog contains unrevertable balance change that paid
		//        for the transaction.
		// Increment associated nonce for sender.
		if (_p.senderAddress != MaxAddress || m_envInfo.number() < m_sealEngine.chainParams().constantinopleForkBlock) // EIP86
			m_s.incNonce(_p.senderAddress);
	}

	//m_savepoint = m_s.savepoint();

	if (m_sealEngine.isPrecompiled(_p.codeAddress, m_envInfo.number()))
	{
		bigint g = m_sealEngine.costOfPrecompiled(_p.codeAddress, _p.data, m_envInfo.number());
		if (_p.gas < g)
		{
			m_excepted = TransactionException::OutOfGasBase;
			// Bail from exception.

			// Empty precompiled contracts need to be deleted even in case of OOG
			// because the bug in both Geth and Parity led to deleting RIPEMD precompiled in this case
			// see https://github.com/ethereum/go-ethereum/pull/3341/files#diff-2433aa143ee4772026454b8abd76b9dd
			// We mark the account as touched here, so that is can be removed among other touched empty accounts (after tx finalization)
			if (m_envInfo.number() >= m_sealEngine.chainParams().EIP158ForkBlock)
				m_s.addBalance(_p.codeAddress, 0);

			return true;	// true actually means "all finished - nothing more to be done regarding go().
		}
		else
		{
			m_gas = (u256)(_p.gas - g);
			bytes output;
			bool success;
			tie(success, output) = m_sealEngine.executePrecompiled(_p.codeAddress, _p.data, m_envInfo.number());
			size_t outputSize = output.size();
			m_output = owning_bytes_ref{ std::move(output), 0, outputSize };
			if (!success)
			{
				m_gas = 0;
				m_excepted = TransactionException::OutOfGas;
				return true;	// true means no need to run go().
			}
		}
	}
	else
	{
		m_gas = _p.gas;
		if (m_s.addressHasCode(_p.codeAddress))
		{
			bytes const& c = m_s.code(_p.codeAddress);
			h256 codeHash = m_s.codeHash(_p.codeAddress);
			m_ext = make_shared<ExtVM>(m_s, m_envInfo, m_sealEngine, _p.receiveAddress,
				_p.senderAddress, _origin, _p.apparentValue, _gasPrice, _p.data, &c, codeHash,
				m_depth, false, _p.staticCall);
		}
	}

	// Transfer ether.
	m_s.transferBalance(_p.senderAddress, _p.receiveAddress, _p.valueTransfer);
	return !m_ext;
}

bool Executive::executeCreate(evmc_address const& _sender, uint256 const& _endowment,
	uint256 const& _gasPrice, uint256 const& _gas, bytesConstRef _code, evmc_address const& _origin)
{
	// add nonce for sender
	m_s.incNonce(_sender);

	m_isCreation = true;
	m_gas = _gas;

	bool accountAlreadyExist = (m_s.addressHasCode(m_newAddress) || m_s.getNonce(m_newAddress) > 0);
	if (accountAlreadyExist)
	{
		//clog(StateSafeExceptions) << "Address already used: " << m_newAddress;
		m_gas = 0;
		//m_excepted = TransactionException::AddressAlreadyUsed;
		//revert();
		m_ext = {}; // cancel the _init execution if there are any scheduled.
		return !m_ext;
	}

	// Transfer ether before deploying the code. This will also create new
	// account if it does not exist yet.
	m_s.transferBalance(_sender, m_newAddress, _endowment);
	uint32 newNonce = m_s.requireAccountStartNonce();
	m_s.setNonce(m_newAddress, newNonce);

	// Schedule _init execution if not empty.
	if (!_code.empty())
		m_ext = std::make_shared<ExtVM>(m_s, m_envInfo, m_newAddress, _sender, _origin,
			_endowment, _gasPrice, bytesConstRef(), _code, sha512Half(_code), m_depth, true, false);

	return !m_ext;
}

bool Executive::go()
{
	auto j = m_s.ctx().app.journal("Executive");
	if (m_ext)
	{
#if ETH_TIMED_EXECUTIONS
		//Timer t;
#endif
		try
		{
			// Create VM instance. Force Interpreter if tracing requested.

			VMFace::pointer vmc = VMFactory::create(VMKind::JIT);
			if (m_isCreation)
			{
				m_s.clearStorage(m_ext->myAddress);
				auto out = vmc->exec(m_gas, *m_ext);
				//if (m_res)
				//{
				//	m_res->gasForDeposit = m_gas;
				//	m_res->depositSize = out.size();
				//}
				if (out.size() > m_ext->evmSchedule().maxCodeSize)
					BOOST_THROW_EXCEPTION(OutOfGas());
				else if (out.size() * m_ext->evmSchedule().createDataGas <= m_gas)
				{
					//if (m_res)
					//	m_res->codeDeposit = CodeDeposit::Success;
					m_gas -= out.size() * m_ext->evmSchedule().createDataGas;
				}
				else
				{
					if (m_ext->evmSchedule().exceptionalFailedCodeDeposit)
						BOOST_THROW_EXCEPTION(OutOfGas());
					else
					{
						//if (m_res)
						//	m_res->codeDeposit = CodeDeposit::Failed;
						out = {};
					}
				}
				//if (m_res)
				//	m_res->output = out.toVector(); // copy output to execution result
				m_s.setCode(m_ext->myAddress, out.toVector());
			}
			else
				m_output = vmc->exec(m_gas, *m_ext);
		}
		catch (RevertInstruction& _e)
		{
			//revert();
			m_output = _e.output();
			//m_excepted = TransactionException::RevertInstruction;
		}
		catch (VMException const& _e)
		{
			JLOG(j.warn()) << "Safe VM Exception. " << diagnostic_information(_e);
			m_gas = 0;
			//m_excepted = toTransactionException(_e);
			//revert();
		}
		catch (InternalVMError const& _e)
		{
			JLOG(j.warn()) << "Internal VM Error (" << *boost::get_error_info<errinfo_evmcStatusCode>(_e) << ")\n"
				<< diagnostic_information(_e);
			throw;
		}
		catch (Exception const& _e)
		{
			// TODO: AUDIT: check that this can never reasonably happen. Consider what to do if it does.
			JLOG(j.warn()) << "Unexpected exception in VM. There may be a bug in this implementation. " << diagnostic_information(_e);
			exit(1);
			// Another solution would be to reject this transaction, but that also
			// has drawbacks. Essentially, the amount of ram has to be increased here.
		}
		catch (std::exception const& _e)
		{
			// TODO: AUDIT: check that this can never reasonably happen. Consider what to do if it does.
			JLOG(j.warn()) << "Unexpected std::exception in VM. Not enough RAM? " << _e.what();
			exit(1);
			// Another solution would be to reject this transaction, but that also
			// has drawbacks. Essentially, the amount of ram has to be increased here.
		}

		//if (m_res && m_output)
		//	// Copy full output:
		//	m_res->output = m_output.toVector();

#if ETH_TIMED_EXECUTIONS
		JLOG(j.warn()) << "VM took:" << t.elapsed() << "; gas used: " << (sgas - m_endGas);
#endif
	}
	return true;
}
} // namespace ripple