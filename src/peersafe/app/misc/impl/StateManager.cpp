
#include <peersafe/app/misc/StateManager.h>
#include <ripple/app/ledger/OpenLedger.h>
#include <ripple/app/main/Application.h>

namespace ripple {

uint32 StateManager::getAccountSeq(AccountID const& id)
{
	if (accountState_.find(id) != accountState_.end())
	{
		return accountState_[id].sequence;
	}

	auto sle = app_.openLedger().current()->read(keylet::account(id));
	if (sle)
	{
		accountState_[id].sequence = sle->getFieldU32(sfSequence);
		return sle->getFieldU32(sfSequence);
	}
	else
	{
		return 0;
	}
}

void StateManager::incrementSeq(AccountID const& id)
{
	if (accountState_.find(id) != accountState_.end())
	{
		++accountState_[id].sequence;
		return;
	}
	auto sle = app_.openLedger().current()->read(keylet::account(id));
	if (sle)
	{
		accountState_[id].sequence = sle->getFieldU32(sfSequence) + 1;
	}
}

}