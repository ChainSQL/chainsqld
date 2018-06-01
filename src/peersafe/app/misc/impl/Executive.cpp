#include <peersafe/app/misc/Executive.h>
#include <ripple/protocol/digest.h>
#include <ripple/app/main/Application.h>
#include <peersafe/core/Tuning.h>
#include <peersafe/vm/VMFactory.h>

namespace ripple {

Executive::Executive(SleOps & _s, EnvInfo const& _envInfo, unsigned int _level)
	:m_s(_s),m_envInfo(_envInfo),m_depth(_level)
{
}

void Executive::initialize() {
	auto& tx = m_s.ctx().tx;
	auto data = tx.getFieldVL(sfContractData);
	bool isCreation = tx.getFieldU16(sfContractOpType) == ContractCreation;
	int g = isCreation ? TX_CREATE_GAS : TX_GAS;
	for (auto i : data)
		g += i ? TX_DATA_NON_ZERO_GAS : TX_DATA_ZERO_GAS;
	m_baseGasRequired = g;

	// Avoid unaffordable transactions.
	int64_t gas = tx.getFieldU32(sfGas);
	int64_t gasPrice = tx.getFieldU32(sfGasPrice);
	int64_t gasCost = int64_t(gas * gasPrice);
	m_gasCost = gasCost;
}

TER Executive::finalize() {
	auto& tx = m_s.ctx().tx;
	auto sender = toEvmC(tx.getAccountID(sfAccount));
	int64_t gasPrice = tx.getFieldU32(sfGasPrice);
	m_s.addBalance(sender, m_gas * gasPrice);

	//to coinbase
	//u256 feesEarned = (m_t.gas() - m_gas) * m_t.gasPrice();
	//m_s.addBalance(m_envInfo.author(), feesEarned);
	return m_excepted;
}

int64_t Executive::gasUsed() const
{
	auto& tx = m_s.ctx().tx;
	int64_t gas = tx.getFieldU32(sfGas);
	return gas - m_gas;
}

bool Executive::execute() {
	auto j = getJ();
	// Entry point for a user-executed transaction.

	// Pay...
	JLOG(j.warn()) << "Paying" << 0/*formatBalance(m_gasCost)*/ << "from sender for gas (" << 0/*m_t.gas()*/ << "gas at" << 0/*formatBalance(m_t.gasPrice())*/ << ")";
	auto& tx = m_s.ctx().tx;
	auto sender = toEvmC(tx.getAccountID(sfAccount));
	m_s.subBalance(sender, m_gasCost);

	if (uint256(tx.getFieldU32(sfGas)) < (uint256)m_baseGasRequired)
	{
		m_excepted = tefGAS_INSUFFICIENT;
		return true;
	}
	bool isCreation = tx.getFieldU16(sfContractOpType) == ContractCreation;
	bytes data = tx.getFieldVL(sfContractData);
	auto value = toEvmC(uint256(tx.getFieldAmount(sfContractValue).zxc().drops()));
	evmc_uint256be gasPrice = toEvmC(uint256());
	int64_t gas = tx.getFieldU32(sfGas);
	if (isCreation)
	{
		return create(sender, value, gasPrice, gas - m_baseGasRequired, &data, sender);
	}
	else
	{
		evmc_address contract_address = toEvmC(tx.getAccountID(sfContractAddress));
		return call(contract_address, sender, value, gasPrice, bytesConstRef(&data), gas - m_baseGasRequired);
	}
}

bool Executive::create(evmc_address const& _txSender, evmc_uint256be const& _endowment,
	evmc_uint256be const& _gasPrice, int64_t const& _gas, bytesConstRef _code, evmc_address const& _originAddress)
{
	return createOpcode(_txSender, _endowment, _gasPrice, _gas, _code, _originAddress);
}

bool Executive::createOpcode(evmc_address const& _sender, evmc_uint256be const& _endowment,
	evmc_uint256be const& _gasPrice, int64_t const& _gas, bytesConstRef _code, evmc_address const& _originAddress)
{
	SLE::pointer pSle = m_s.getSle(_sender);
	uint32 nonce = pSle->getFieldU32(sfNonce);
	m_newAddress = m_s.calcNewAddress(_sender, nonce);
	// add nonce for sender
	m_s.incNonce(_sender);
	return executeCreate(_sender, _endowment, _gasPrice, _gas, _code, _originAddress);
}


bool Executive::call(evmc_address const& _receiveAddress, evmc_address const& _senderAddress,
	evmc_uint256be const& _value, evmc_uint256be const& _gasPrice, bytesConstRef _data, int64_t const& _gas)
{
	CallParameters params{ _senderAddress, _receiveAddress, _receiveAddress, _value, _value, _gas, _data };
	return call(params, _gasPrice, _senderAddress);
}

bool Executive::call(CallParameters const& _p, evmc_uint256be const& _gasPrice, evmc_address const& _origin)
{
	//// If external transaction.
	//if (m_t)
	//{
	//	// FIXME: changelog contains unrevertable balance change that paid
	//	//        for the transaction.
	//	// Increment associated nonce for sender.
	//	//if (_p.senderAddress != MaxAddress || m_envInfo.number() < m_sealEngine.chainParams().constantinopleForkBlock) // EIP86
	//	m_s.incNonce(_p.senderAddress);
	//}

	//m_savepoint = m_s.savepoint();

	m_gas = _p.gas;
	if (m_s.addressHasCode(_p.codeAddress))
	{
		bytes const& c = m_s.code(_p.codeAddress);
		uint256 codeHash = m_s.codeHash(_p.codeAddress);
		m_ext = std::make_shared<ExtVM>(m_s, m_envInfo, _p.receiveAddress,
			_p.senderAddress, _origin, _p.apparentValue, _gasPrice, _p.data, &c, toEvmC(codeHash),
			m_depth, false, _p.staticCall);
	}

	// Transfer zxc.
	m_s.transferBalance(_p.senderAddress, _p.receiveAddress, _p.valueTransfer);
	return !m_ext;
}

bool Executive::executeCreate(evmc_address const& _sender, evmc_uint256be const& _endowment,
	evmc_uint256be const& _gasPrice, int64_t const& _gas, bytesConstRef _code, evmc_address const& _origin)
{
	auto j = getJ();

	m_isCreation = true;
	m_gas = _gas;

	bool accountAlreadyExist = (m_s.addressHasCode(m_newAddress) || m_s.getNonce(m_newAddress) > 0);
	if (accountAlreadyExist)
	{
		JLOG(j.warn()) << "Address already used: " << to_string(fromEvmC(m_newAddress));
		m_gas = 0;
		m_excepted = tefADDRESS_AREADY_USED;
		//revert();
		m_ext = {}; // cancel the _init execution if there are any scheduled.
		return !m_ext;
	}

	// Transfer ether before deploying the code. This will also create new
	// account if it does not exist yet.
	TER ret = m_s.activateContract(_sender, m_newAddress, _endowment);
	if (ret != tesSUCCESS)
	{
		return true;
	}
	//m_s.transferBalance(_sender, m_newAddress, _endowment);
	uint32 newNonce = m_s.requireAccountStartNonce();
	m_s.setNonce(m_newAddress, newNonce);

	// Schedule _init execution if not empty.
	if (!_code.empty())
		m_ext = std::make_shared<ExtVM>(m_s, m_envInfo, m_newAddress, _sender, _origin,
			_endowment, _gasPrice, bytesConstRef(), _code, toEvmC(sha512Half(_code.data())), m_depth, true, false);

	return !m_ext;
}

bool Executive::go()
{
	auto j = getJ();
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
				if (out.size() > MAX_CODE_SIZE)
					BOOST_THROW_EXCEPTION(OutOfGas());
				else if (out.size() * CREATE_DATA_GAS <= m_gas)
				{
					//if (m_res)
					//	m_res->codeDeposit = CodeDeposit::Success;
					m_gas -= out.size() * CREATE_DATA_GAS;
				}
				else
				{
					BOOST_THROW_EXCEPTION(OutOfGas());
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
			m_excepted = tefCONTRACT_EXEC_EXCEPTION;
			//m_excepted = TransactionException::RevertInstruction;
		}
		catch (VMException const& _e)
		{
			JLOG(j.warn()) << "Safe VM Exception. " << diagnostic_information(_e);
			m_gas = 0;
			m_excepted = tefCONTRACT_EXEC_EXCEPTION;
			//revert();
		}
		catch (InternalVMError const& _e)
		{
			JLOG(j.warn()) << "Internal VM Error (" << *boost::get_error_info<errinfo_evmcStatusCode>(_e) << ")\n"
				<< diagnostic_information(_e);
			m_excepted = tefCONTRACT_EXEC_EXCEPTION;
			throw;
		}
		catch (Exception const& _e)
		{
			// TODO: AUDIT: check that this can never reasonably happen. Consider what to do if it does.
			JLOG(j.warn()) << "Unexpected exception in VM. There may be a bug in this implementation. " << diagnostic_information(_e);
			m_excepted = tefCONTRACT_EXEC_EXCEPTION;
			// Another solution would be to reject this transaction, but that also
			// has drawbacks. Essentially, the amount of ram has to be increased here.
		}
		catch (std::exception const& _e)
		{
			// TODO: AUDIT: check that this can never reasonably happen. Consider what to do if it does.
			JLOG(j.warn()) << "Unexpected std::exception in VM. Not enough RAM? " << _e.what();
			m_excepted = tefCONTRACT_EXEC_EXCEPTION;
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

beast::Journal Executive::getJ()
{
	return m_s.ctx().app.journal("Executive");
}
} // namespace ripple