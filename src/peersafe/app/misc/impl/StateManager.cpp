
#include <peersafe/app/misc/StateManager.h>
#include <ripple/app/ledger/OpenLedger.h>
#include <ripple/app/main/Application.h>

namespace ripple {

uint32 StateManager::getAccountSeq(AccountID const& id)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (accountState_.find(id) != accountState_.end())
	{
		return accountState_[id].sequence;
	}

	assert(app_.openLedger().current());
	
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

void StateManager::resetAccountSeq(AccountID const& id)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (accountState_.find(id) != accountState_.end())
	{
		accountState_.erase(id);
	}	
}

void StateManager::incrementSeq(AccountID const& id)
{
	std::lock_guard<std::mutex> lock(mutex_);
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

void StateManager::clear()
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (accountState_.size() > 0)
	{
		accountState_.clear();
	}
}

}