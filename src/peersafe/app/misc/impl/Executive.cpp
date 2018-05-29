#include <peersafe/app/misc/Executive.h>


namespace ripple {

Executive::Executive(SleOps& _s, EnvInfo& _envInfo, unsigned int _level)
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

bool Executive::execute() {
	// Entry point for a user-executed transaction.

	// Pay...
	//clog(StateDetail) << "Paying" << formatBalance(m_gasCost) << "from sender for gas (" << m_t.gas() << "gas at" << formatBalance(m_t.gasPrice()) << ")";
	//m_s.subBalance(m_t.sender(), m_gasCost);

	//assert(m_t.gas() >= (u256)m_baseGasRequired);
	//if (m_t.isCreation())
	//	return create(m_t.sender(), m_t.value(), m_t.gasPrice(), m_t.gas() - (u256)m_baseGasRequired, &m_t.data(), m_t.sender());
	//else
	//	return call(m_t.receiveAddress(), m_t.sender(), m_t.value(), m_t.gasPrice(), bytesConstRef(&m_t.data()), m_t.gas() - (u256)m_baseGasRequired);
}

bool Executive::create(evmc_address const& _txSender, uint256 const& _endowment,
	uint256 const& _gasPrice, uint256 const& _gas, bytesConstRef _code, evmc_address const& _originAddress) {

	return false;
}

bool Executive::createOpcode(evmc_address const& _sender, uint256 const& _endowment,
	uint256 const& _gasPrice, uint256 const& _gas, bytesConstRef _code, evmc_address const& _originAddress)
{
	return true;
}
///// @returns false iff go() must be called (and thus a VM execution in required).
//bool create2Opcode(evmc_address const& _sender, uint256 const& _endowment,
//	uint256 const& _gasPrice, uint256 const& _gas, bytesConstRef _code, evmc_address const& _originAddress, uint256 const& _salt);
/// Set up the executive for evaluating a bare CALL (message call) operation.
/// @returns false iff go() must be called (and thus a VM execution in required).
bool Executive::call(evmc_address const& _receiveAddress, evmc_address const& _txSender,
	uint256 const& _txValue, uint256 const& _gasPrice, bytesConstRef _txData, uint256 const& _gas)
{
	return true;
}

bool Executive::call(CallParameters const& _cp, uint256 const& _gasPrice, evmc_address const& _origin)
{
	return true;
}

bool Executive::go()
{
	return true;
}
} // namespace ripple