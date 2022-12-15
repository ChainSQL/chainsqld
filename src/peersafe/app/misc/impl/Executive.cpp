#include <ripple/app/tx/impl/Transactor.h>
#include <peersafe/app/misc/Executive.h>
#include <eth/vm/VMFactory.h>
#include <peersafe/core/Tuning.h>
#include <ripple/protocol/digest.h>
#include <peersafe/schema/Schema.h>
#include <ripple/app/misc/LoadFeeTrack.h>
#include <ripple/basics/StringUtilities.h>
#include <peersafe/protocol/ContractDefines.h>
#include <peersafe/protocol/Contract.h>
#include <eth/vm/utils/keccak.h>
#include <peersafe/app/ledger/LedgerAdjust.h>
#include <peersafe/protocol/STMap256.h>
#include <ripple/protocol/Feature.h>
#include <eth/api/utils/Helpers.h>

namespace ripple {

AccountID const c_RipemdPrecompiledAddress(23);

Executive::Executive(
    SleOps& _s,
    eth::EnvInfo const& _envInfo,
    unsigned int _level)
    : m_s(_s)
    , m_envInfo(_envInfo)
    , m_PreContractFace(m_envInfo.preContractFace())
    , m_depth(_level)
{
}

double Executive::getCurGasPrice(ApplyContext& ctx)
{
    std::uint64_t scaledGasPrice = scaleGasLoad(ctx.app.getFeeTrack(), ctx.view().fees());
    
    double curGasPrice;
    curGasPrice = (double)scaledGasPrice/compressDrop;

    return curGasPrice;
}

void Executive::initGasPrice()
{
    m_gasPrice = scaleGasLoad(
		m_s.ctx().app.getFeeTrack(),
		m_s.ctx().view().fees());
}


int64_t Executive::baseGasRequired(bool isCreation, eth::bytesConstRef const& data) {
    int64_t baseGas = isCreation ? TX_CREATE_GAS : TX_GAS;
    for (auto i : data)
        baseGas += i ? TX_DATA_NON_ZERO_GAS : TX_DATA_ZERO_GAS;
    return baseGas;
}

void Executive::initialize() {
    if(m_s.ctx().view().rules().enabled(featureGasPriceCompress))
    {
        m_gasPrice = getCurGasPrice(m_s.ctx());
    }
    else
    {
        initGasPrice();
    }

	auto& tx = m_s.ctx().tx;
	auto data = tx.getFieldVL(sfContractData);
	bool isCreation = tx.getFieldU16(sfContractOpType) == ContractCreation;
    m_baseGasRequired = baseGasRequired(isCreation, &data);

	// Avoid unfordable transactions.
	int64_t gas = tx.getFieldU32(sfGas);
    double gasCost = gas * m_gasPrice;
    if(gasCost < 1) gasCost = 1;
	m_gasCost = (int64_t)gasCost;
}

bool Executive::execute() {
	auto j = getJ();
	// Entry point for a user-executed transaction.
	
	// Pay...
	JLOG(j.debug()) << "Paying " << m_gasCost << " from sender";
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
		AccountID receive_address = tx.getAccountID(sfContractAddress);
		return call(receive_address, sender, value, gasPrice, &m_input, gas - m_baseGasRequired);
	}
}

bool Executive::create(AccountID const& _txSender, uint256 const& _endowment,
	uint256 const& _gasPrice, int64_t const& _gas, eth::bytesConstRef const& _code, AccountID const& _originAddress)
{
	return createOpcode(_txSender, _endowment, _gasPrice, _gas, _code, _originAddress);
}

bool Executive::createOpcode(AccountID const& _sender, uint256 const& _endowment,
	uint256 const& _gasPrice, int64_t const& _gas, eth::bytesConstRef const& _code, AccountID const& _originAddress)
{
	bool accountAlreadyExist = false;
	uint32_t sequence = 1;
    
    CommonKey::HashType hashType = safe_cast<TxType>(m_s.getTx().getFieldU16(sfTransactionType)) == ttETH_TX ? CommonKey::sha3 : CommonKey::sha;
    
	if (m_depth == 1)
	{
		sequence = m_s.getTx().getFieldU32(sfSequence);
		m_newAddress = Contract::calcNewAddress(_sender, sequence, hashType);
	}
	else
	{
		sequence = m_s.getSequence(_sender);
		do {
			m_newAddress = Contract::calcNewAddress(_sender, sequence, hashType);
			// add sequence for sender
			//m_s.incSequence(_sender);
			sequence++;
			accountAlreadyExist = (m_s.getSle(m_newAddress) != nullptr);
		} while (accountAlreadyExist);
	}
	

	return executeCreate(_sender, _endowment, _gasPrice, _gas, _code, _originAddress);
}

bool Executive::create2Opcode(AccountID const& _sender, uint256 const& _endowment,
	uint256 const& _gasPrice, int64_t const& _gas, eth::bytesConstRef const& _code, AccountID const& _originAddress, uint256 const& _salt)
{
	eth::bytes serialData;
	serialData.push_back(0xff);
	serialData.insert(serialData.end(), _sender.begin(), _sender.end());
	serialData.insert(serialData.end(), _salt.begin(), _salt.end());
	
	uint8_t hashRet[32];
	eth::keccak(_code.data(), _code.size(), hashRet);
	serialData.insert(serialData.end(), hashRet, hashRet + 32);

	eth::keccak(serialData.data(), serialData.size(), hashRet);
	std::memcpy(m_newAddress.data(), hashRet + 12, 20);

	return executeCreate(_sender, _endowment, _gasPrice, _gas, _code, _originAddress);
}

bool Executive::call(AccountID const& _receiveAddress, AccountID const& _senderAddress,
	uint256 const& _value, uint256 const& _gasPrice, eth::bytesConstRef const& _data, int64_t const& _gas)
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

    if (m_PreContractFace.isPrecompiledOrigin(_p.codeAddress, m_envInfo.block_number()))
    {
        if (_p.receiveAddress == c_RipemdPrecompiledAddress)
            m_s.unrevertableTouch(_p.codeAddress);

        int64_t g = m_PreContractFace.costOfPrecompiled(_p.codeAddress, _p.data, m_envInfo.block_number());
        if (_p.gas < g)
        {
            m_excepted = tefGAS_INSUFFICIENT;
            // Bail from exception.
            return true;	// true actually means "all finished - nothing more to be done regarding go().
        }
        else
        {
            m_gas = (_p.gas - g);
            eth::bytes output;
            bool success;
            tie(success, output) = m_PreContractFace.executePrecompiled(_p.codeAddress, _p.data, m_envInfo.block_number());
            size_t outputSize = output.size();
            m_output = eth::owning_bytes_ref{std::move(output), 0, outputSize};
            if (!success)
            {
                m_gas = 0;
                m_excepted = tefGAS_INSUFFICIENT;
                return true;	// true means no need to run go().
            }
        }
    }
    else if (m_PreContractFace.isPrecompiledDiy(_p.codeAddress))
    {
        auto retPre = m_PreContractFace.executePreDiy(
            m_s, _p.codeAddress, _p.data, _p.senderAddress, _origin);
        m_gas = (_p.gas - get<2>(retPre));
        TER ter = get<0>(retPre);
        if (ter != tesSUCCESS)
        {
            m_excepted = ter;
            return true;
        }
        auto output = get<1>(retPre);
        if (output.size() > 0)
        {
            size_t outputSize = output.size();
            m_output = eth::owning_bytes_ref{std::move(output), 0, outputSize};
        }
	}
    else
    {
        m_gas = _p.gas;
        if (m_s.addressHasCode(_p.codeAddress))
        {
            eth::bytes const &c = m_s.code(_p.codeAddress);
            if (c.size() == 0)
            {
                m_excepted = tefCONTRACT_NOT_EXIST;
                return true;
            }

            uint256 codeHash = m_s.codeHash(_p.codeAddress);
            m_ext = std::make_shared<ExtVM>(m_s, m_envInfo, _p.receiveAddress,
                                            _p.senderAddress, _origin, _p.apparentValue, _gasPrice, _p.data, &c, codeHash,
                                            m_depth, false, _p.staticCall);
        }// if not first call,codeAddress not need to be a contract address
        else if (m_depth == 1 && !isEthTx(m_s.ctx().tx))                                                
        {
            // contract may be killed
            auto blob = strCopy(std::string("Contract does not exist,maybe destructed."));
            m_output = eth::owning_bytes_ref(std::move(blob), 0, blob.size());
            m_excepted = tefCONTRACT_NOT_EXIST;
            return true;
        }
        // Transfer zxc.
		if(_p.valueTransfer != uint256(0))
		{
			TER ret = tesSUCCESS;
			if (!_p.staticCall && m_s.getSle(_p.receiveAddress) == nullptr &&
				!m_PreContractFace.isPrecompiledOrigin(
					_p.receiveAddress, m_envInfo.block_number()) &&
				!m_PreContractFace.isPrecompiledDiy(_p.receiveAddress))
			{
				// account not exist,activate it
				ret = m_s.doPayment(
					_p.senderAddress, _p.receiveAddress, _p.valueTransfer);
			}
			else
			{
				ret = m_s.transferBalance(
					_p.senderAddress, _p.receiveAddress, _p.valueTransfer);
			}

			auto j = getJ();
			JLOG(j.info()) << "Contract invoke , address : "
						<< to_string(_p.codeAddress)
						<< ", sender :" << to_string(_p.senderAddress)
						<< ", receive :" << to_string(_p.receiveAddress)
						<< ", amount :" << to_string(_p.valueTransfer);

			if (ret != tesSUCCESS)
			{
				m_excepted = ret;
				//formatOutput(std::to_string(TERtoInt(ret)));
				formatOutput(transHuman(ret));
				return true;
			}
		}        
    }

	return !m_ext;
}

bool Executive::executeCreate(AccountID const& _sender, uint256 const& _endowment,
	uint256 const& _gasPrice, int64_t const& _gas, eth::bytesConstRef const& _code, AccountID const& _origin)
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
	LedgerAdjust::updateContractCount(m_s.ctx().app, m_s.ctx().view(),CONTRACT_CREATE);

    JLOG(j.info()) << "Contract create , address : "
                   << to_string(m_newAddress) << ", from sender :" << to_string(_sender);

	// Schedule _init execution if not empty.
	Blob data;
    if (!_code.empty())
        m_ext = std::make_shared<ExtVM>(m_s, m_envInfo, m_newAddress, _sender, _origin,
            value, _gasPrice, &data, _code, sha512Half(makeSlice(_code.toBytes())), m_depth, true, false);
	
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
			eth::VMFace::pointer vmc = eth::VMFactory::create(eth::VMKind::Interpreter);
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
					BOOST_THROW_EXCEPTION(eth::OutOfGas());
				else if (out.size() * CREATE_DATA_GAS <= m_gas)
				{
					//if (m_res)
					//	m_res->codeDeposit = CodeDeposit::Success;
					m_gas -= out.size() * CREATE_DATA_GAS;
				}
				else
				{
					BOOST_THROW_EXCEPTION(eth::OutOfGas());
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
		catch (eth::RevertInstruction& _e)
		{
			//revert();
            if (m_depth == INITIAL_DEPTH)
            {
                m_revertOri = _e.output();
                formatOutput(_e.output());
            }
            else
                m_output = _e.output();
            
			m_excepted = tefCONTRACT_REVERT_INSTRUCTION;
		}
		catch (eth::RevertDiyInstruction& _e)
		{
			auto str = _e.output().toString();
			int n = atoi(str.c_str());
			m_excepted = TER::fromInt(n);
		}
		catch (eth::VMException const& _e)
		{
			//JLOG(j.warn()) << "Safe VM Exception. " << diagnostic_information(_e);
            formatOutput(_e.what());
			m_gas = 0;
            m_excepted = exceptionToTerCode(_e);
			//revert();
		}
		catch (eth::InternalVMError const& _e)
		{
			JLOG(j.warn()) << "Internal VM Error (" << *boost::get_error_info<eth::errinfo_evmcStatusCode>(_e) << ")\n"
				<< diagnostic_information(_e);
			formatOutput(_e.what());
			m_excepted = tefCONTRACT_EXEC_EXCEPTION;
			throw;
		}
		catch (eth::Exception const& _e)
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
		m_ext->sub.refunds += SUICIDE_REFUND_GAS * m_ext->sub.selfdestruct.size();

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
    if (m_ext)
    {
        for (auto a : m_ext->sub.selfdestruct)
        {
            if (auto ter = Transactor::cleanUpDirOnDeleteAccount(m_s.ctx(), a);
                ter != tesSUCCESS)
            {
                return ter;
            }

            m_s.kill(a);
            LedgerAdjust::updateContractCount(
                m_s.ctx().app, m_s.ctx().view(), CONTRACT_DESTORY);
        }
    }

    return m_excepted;
}

void Executive::accrueSubState(eth::SubState& _parentContext)
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
	m_output = eth::owning_bytes_ref(std::move(blob), 0, blob.size());
}

void Executive::formatOutput(eth::owning_bytes_ref output)
{
	if (output.empty())
	{
		m_output = eth::owning_bytes_ref();
		return;
	}

	auto str = output.toString();
	std::string funSig = output.size() >= 4 ? strHex(output.data(), output.data()+4) : std::string("");
 	Blob blob;

	//self-define exception in go()
	if (funSig == SELFDEFFUNSIG)
	{
		blob = strCopy(str.substr(4,str.size() - 4));
	}
	else if (funSig == ERRFUNSIG)
	{
		uint256 offset = uint256(strCopy(str.substr(4, 32)));
		uint256 length = uint256(strCopy(str.substr(4 + 32, 32)));
		blob = strCopy(str.substr(4 + 32 + fromUint256(offset), fromUint256(length)));
	}
	else if (funSig == REVERTFUNSIG)
	{
		int64_t errCode = fromUint256(uint256(Blob(output.data() + 4, output.data() + output.size())));
		blob = strCopy(getRevertErr(errCode));
	}
	else
	{
		blob = strCopy(str);
	}
	m_output = eth::owning_bytes_ref(std::move(blob), 0, blob.size());
}

std::string Executive::getRevertErr(int64_t errCode)
{
	switch(errCode)
	{
	case 0x00:
		return std::string("Generic compiler inserted panics.");
	case 0x01:
		return std::string("The assert with an argument that evaluates to false.");
	case 0x11:
		return std::string("An arithmetic operation results in underflow or overflow.");
	case 0x12:
		return std::string("Divide or modulo by zero.");
	case 0x21:
		return std::string("Convert a value that is too big or negative into an enum type.");
	case 0x22:
		return std::string("Access a storage byte array that is incorrectly encoded.");
	case 0x31:
		return std::string("Call .pop() on an empty array.");
	case 0x32:
		return std::string("An out-of-bounds or negative index for array.");
	case 0x41:
		return std::string("Allocate too much memory or create an array that is too large.");
	case 0x51:
		return std::string("Call a zero-initialized variable of internal function type.");
	default:
		return std::string("Unkown errCode for assert");
	}
}

TER
Executive::exceptionToTerCode(eth::VMException const& _e)
{
    // VM execution exceptions
    if (!!dynamic_cast<eth::OutOfGas const*>(&_e))
        return tefGAS_INSUFFICIENT;
    return tefCONTRACT_EXEC_EXCEPTION;
}

} // namespace ripple
