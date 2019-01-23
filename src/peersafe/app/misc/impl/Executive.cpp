#include <peersafe/app/misc/Executive.h>
#include <peersafe/vm/VMFactory.h>
#include <peersafe/core/Tuning.h>
#include <ripple/protocol/digest.h>
#include <ripple/app/main/Application.h>
#include <ripple/app/misc/LoadFeeTrack.h>
#include <ripple/basics/StringUtilities.h>
#include <peersafe/protocol/ContractDefines.h>
#include <peersafe/protocol/Contract.h>

namespace ripple {

Executive::Executive(SleOps & _s, EnvInfo const& _envInfo, unsigned int _level)
	:m_s(_s),m_envInfo(_envInfo),m_depth(_level)
{
}

void Executive::initGasPrice()
{
	m_gasPrice = scaleGasLoad(GAS_PRICE, m_s.ctx().app.getFeeTrack(),
		m_s.ctx().view().fees());
}

void Executive::initialize() {
	initGasPrice();

	auto& tx = m_s.ctx().tx;
	auto data = tx.getFieldVL(sfContractData);
	bool isCreation = tx.getFieldU16(sfContractOpType) == ContractCreation;
	int g = isCreation ? TX_CREATE_GAS : TX_GAS;
	for (auto i : data)
		g += i ? TX_DATA_NON_ZERO_GAS : TX_DATA_ZERO_GAS;
	m_baseGasRequired = g;

	// Avoid unaffordable transactions.
	int64_t gas = tx.getFieldU32(sfGas);
	int64_t gasCost = int64_t(gas * m_gasPrice);
	m_gasCost = gasCost;
}

bool Executive::execute() {
	auto j = getJ();
	// Entry point for a user-executed transaction.
	
	// Pay...
	JLOG(j.info()) << "Paying" << m_gasCost << "from sender";
	auto& tx = m_s.ctx().tx;
	auto sender = tx.getAccountID(sfAccount);
	auto ter = m_s.subBalance(sender, m_gasCost);
	if (ter != tesSUCCESS)
	{
		m_excepted = ter;
		return true;
	}

	int64_t gas = tx.getFieldU32(sfGas);
	if (uint256(gas) < (uint256)m_baseGasRequired)
	{
		m_excepted = tefGAS_INSUFFICIENT;
		return true;
	}
	bool isCreation = tx.getFieldU16(sfContractOpType) == ContractCreation;
	m_input = tx.getFieldVL(sfContractData);
	uint256 value = uint256(tx.getFieldAmount(sfContractValue).zxc().drops());
	uint256 gasPrice = uint256(m_gasPrice);
	if (isCreation)
	{
		return create(sender, value, gasPrice, gas - m_baseGasRequired, &m_input, sender);
	}
	else
	{
		AccountID contract_address = tx.getAccountID(sfContractAddress);
		return call(contract_address, sender, value, gasPrice, &m_input, gas - m_baseGasRequired);
	}
}

bool Executive::create(AccountID const& _txSender, uint256 const& _endowment,
	uint256 const& _gasPrice, int64_t const& _gas, bytesConstRef const& _code, AccountID const& _originAddress)
{
	return createOpcode(_txSender, _endowment, _gasPrice, _gas, _code, _originAddress);
}

bool Executive::createOpcode(AccountID const& _sender, uint256 const& _endowment,
	uint256 const& _gasPrice, int64_t const& _gas, bytesConstRef const& _code, AccountID const& _originAddress)
{
	bool accountAlreadyExist = false;
	do {
		uint32 sequence = 1;
		if (m_depth == 1)
		{
			sequence = m_s.getTx().getFieldU32(sfSequence);
		}
		else
		{
			sequence = m_s.getSequence(_sender);
		}

		m_newAddress = Contract::calcNewAddress(_sender, sequence);
		// add sequence for sender
		m_s.incSequence(_sender);

		accountAlreadyExist = (m_s.getSle(m_newAddress) != nullptr);
	} while (accountAlreadyExist);

	return executeCreate(_sender, _endowment, _gasPrice, _gas, _code, _originAddress);
}

bool Executive::call(AccountID const& _receiveAddress, AccountID const& _senderAddress,
	uint256 const& _value, uint256 const& _gasPrice, bytesConstRef const& _data, int64_t const& _gas)
{
	CallParametersR params{ _senderAddress, _receiveAddress, _receiveAddress, _value, _value, _gas, _data };
	return call(params, _gasPrice, _senderAddress);
}

bool Executive::call(CallParametersR const& _p, uint256 const& _gasPrice, AccountID const& _origin)
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
		if (c.size() == 0)
		{
			m_excepted = tefCONTRACT_NOT_EXIST;
			return true;
		}
			
		uint256 codeHash = m_s.codeHash(_p.codeAddress);
		m_ext = std::make_shared<ExtVM>(m_s, m_envInfo, _p.receiveAddress,
			_p.senderAddress, _origin, _p.apparentValue, _gasPrice, _p.data, &c, codeHash,
			m_depth, false, _p.staticCall);
	}
	else if(m_depth == 1) //if not first call,codeAddress not need to be a contract address
	{
		// contract may be killed
		auto blob = strCopy(std::string("Contract does not exist,maybe destructed."));
		m_output = owning_bytes_ref(std::move(blob), 0, blob.size());
		m_excepted = tefCONTRACT_NOT_EXIST;
		return true;
	}

	// Transfer zxc.
	TER ret = tesSUCCESS;
	if (m_s.getSle(_p.receiveAddress) == nullptr)
	{
		//account not exist,activate it
		ret = m_s.doPayment(_p.senderAddress, _p.receiveAddress, _p.valueTransfer);
	}
	else
	{
		ret = m_s.transferBalance(_p.senderAddress, _p.receiveAddress, _p.valueTransfer);
	}

	if (ret != tesSUCCESS)
	{
		m_excepted = ret;
		return true;
	}

	return !m_ext;
}

bool Executive::executeCreate(AccountID const& _sender, uint256 const& _endowment,
	uint256 const& _gasPrice, int64_t const& _gas, bytesConstRef const& _code, AccountID const& _origin)
{
	auto j = getJ();

	m_isCreation = true;
	m_gas = _gas;

	// Transfer zxc before deploying the code. This will also create new
	// account if it does not exist yet.
	auto value = _endowment;

	TER ret = m_s.createContractAccount(_sender, m_newAddress, value);
	if (ret != tesSUCCESS)
	{
		m_excepted = ret;
		return true;
	}

	// Schedule _init execution if not empty.
	Blob data;
	if (!_code.empty())
		m_ext = std::make_shared<ExtVM>(m_s, m_envInfo, m_newAddress, _sender, _origin,
			value, _gasPrice, &data, _code, sha512Half(_code.toBytes()), m_depth, true, false);

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
				m_s.clearStorage(m_ext->contractAddress());
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
				m_s.setCode(m_ext->contractAddress(), out.toVector());
			}
			else
			{
				m_output = vmc->exec(m_gas, *m_ext);
			}
		}
		catch (RevertInstruction& _e)
		{
			//revert();
			formatOutput(_e.output());
			m_excepted = tefCONTRACT_REVERT_INSTRUCTION;
		}
		catch (RevertDiyInstruction& _e)
		{
			auto str = _e.output().toString();
			int n = atoi(str.c_str());
			m_excepted = TER(n);
		}
		catch (VMException const& _e)
		{
			JLOG(j.warn()) << "Safe VM Exception. " << diagnostic_information(_e);
			formatOutput(_e.what());
			m_gas = 0;
			m_excepted = tefCONTRACT_EXEC_EXCEPTION;
			//revert();
		}
		catch (InternalVMError const& _e)
		{
			JLOG(j.warn()) << "Internal VM Error (" << *boost::get_error_info<errinfo_evmcStatusCode>(_e) << ")\n"
				<< diagnostic_information(_e);
			formatOutput(_e.what());
			m_excepted = tefCONTRACT_EXEC_EXCEPTION;
			throw;
		}
		catch (Exception const& _e)
		{
			// TODO: AUDIT: check that this can never reasonably happen. Consider what to do if it does.
			JLOG(j.warn()) << "Unexpected exception in VM. There may be a bug in this implementation. " << diagnostic_information(_e);
			m_excepted = tefCONTRACT_EXEC_EXCEPTION;
			formatOutput(_e.what());
			// Another solution would be to reject this transaction, but that also
			// has drawbacks. Essentially, the amount of ram has to be increased here.
		}
		catch (std::exception const& _e)
		{
			// TODO: AUDIT: check that this can never reasonably happen. Consider what to do if it does.
			JLOG(j.warn()) << "Unexpected std::exception in VM. Not enough RAM? " << _e.what();
			m_excepted = tefCONTRACT_EXEC_EXCEPTION;
			formatOutput(_e.what());
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


int64_t Executive::gasUsed() const
{
	auto& tx = m_s.ctx().tx;
	int64_t gas = tx.getFieldU32(sfGas);
	return gas - m_gas;
}

TER Executive::finalize() {

	// Accumulate refunds for suicides.
	if (m_ext)
		m_ext->sub.refunds += SUICIDE_REFUND_GAS * m_ext->sub.suicides.size();

	auto& tx = m_s.ctx().tx;
	int64_t gas = tx.getFieldU32(sfGas);

	if (m_ext)
	{
		m_refunded = (gas - m_gas) / 2 > m_ext->sub.refunds ? m_ext->sub.refunds : (gas - m_gas) / 2;
	}
	else
	{
		m_refunded = 0;
	}
	m_gas += m_refunded;

	auto sender = tx.getAccountID(sfAccount);
	m_s.addBalance(sender, m_gas * m_gasPrice);

	// Suicides...
	if (m_ext) for (auto a : m_ext->sub.suicides) m_s.kill(a);

	return m_excepted;
}

void Executive::accrueSubState(SubState& _parentContext)
{
    if (m_ext)
        _parentContext += m_ext->sub;
}

beast::Journal Executive::getJ()
{
	return m_s.ctx().app.journal("Executive");
}

void Executive::formatOutput(std::string msg)
{
	auto blob = strCopy(msg);
	m_output = owning_bytes_ref(std::move(blob), 0, blob.size());
}

void Executive::formatOutput(owning_bytes_ref output)
{
	if (output.empty())
	{
		m_output = owning_bytes_ref();
		return;
	}

	auto str = output.toString();
	Blob blob;

	//self-define exception in go()
	if (str.substr(0, 4) == "\0\0\0\0")
	{
		blob = strCopy(str.substr(4,str.size() - 4));
	}
	else
	{
		uint256 offset = uint256(strCopy(str.substr(4, 32)));
		uint256 length = uint256(strCopy(str.substr(4 + 32, 32)));
		blob = strCopy(str.substr(4 + 32 + fromUint256(offset), fromUint256(length)));
	}
	m_output = owning_bytes_ref(std::move(blob), 0, blob.size());
}

} // namespace ripple