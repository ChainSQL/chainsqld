
#include <peersafe/app/misc/StateManager.h>
#include <ripple/app/ledger/OpenLedger.h>
#include <peersafe/schema/Schema.h>

namespace ripple {

uint32_t
StateManager::getAndIncSignSeq(AccountID const& id, ReadView const& view)
{
    std::lock_guard lock(mutex_);
    auto sle = view.read(keylet::account(id));
    if (sle)
    {
        auto seq =
            std::max(sle->getFieldU32(sfSequence), accountState_[id].checkSeq);
        if (accountState_.find(id) != accountState_.end() &&
            accountState_[id].signSeq >= seq)
        {
            if (!accountState_[id].setFailedSeq.empty())
            {
                auto ret = *accountState_[id].setFailedSeq.begin();
                JLOG(j_.info()) << "getAndIncSignSeq: use failed seq " << ret;
                accountState_[id].setFailedSeq.erase(ret);
                return ret;
            }
            ++accountState_[id].signSeq;
            return accountState_[id].signSeq - 1;
        }
        else
        {
            // Maybe client sign and submit tx first,and then let chain-node
            // sign and submit
            JLOG(j_.info()) << "getAndIncSignSeq: use sle seq " << seq;
            accountState_[id].signSeq = seq + 1;
            accountState_[id].checkSeq = seq;
            return seq;
        }
    }
    else
    {
        return 0;
    }
}

uint32_t
StateManager::getAccountCheckSeq(AccountID const& id, ReadView const& view)
{
    std::lock_guard lock(mutex_);
    auto sle = view.read(keylet::account(id));
    if (sle)
    {
        auto seq = sle->getFieldU32(sfSequence);
        if (accountState_.find(id) != accountState_.end() &&
            accountState_[id].checkSeq >= seq)
        {
            return accountState_[id].checkSeq;
        }
        else
        {
            JLOG(j_.info())
                << "getAccountCheckSeq: set signSeq and checkSeq to " << seq
                << " by sle in view";
            accountState_[id].signSeq = seq;
            accountState_[id].checkSeq = seq;
            return seq;
        }
    }
    else
    {
        return 0;
    }
}

uint32_t
StateManager::getAccountCheckSeq(AccountID const& id,std::shared_ptr<const SLE> const sle)
{
    std::lock_guard lock(mutex_);
    auto seq = sle->getFieldU32(sfSequence);
    if (accountState_.find(id) != accountState_.end() && 
		accountState_[id].checkSeq >= seq)
    {
		return accountState_[id].checkSeq;
    }
    else
    {
        JLOG(j_.info()) << "getAccountCheckSeq: set signSeq and checkSeq to "
                        << seq << " by sle";
        accountState_[id].signSeq = seq;
        accountState_[id].checkSeq = seq;
        return seq;
    }
}

void
StateManager::resetAccountSeq(AccountID const& id)
{
    std::lock_guard lock(mutex_);
    if (accountState_.find(id) != accountState_.end())
    {
        JLOG(j_.info()) << "resetAccountSeq " << id;
        accountState_.erase(id);
    }
}

void
StateManager::onTxCheckSuccess(AccountID const& id)
{
    std::lock_guard lock(mutex_);
    if (accountState_.find(id) != accountState_.end())
    {
        auto seq = accountState_[id].checkSeq;
        if (accountState_[id].setFailedSeq.find(seq) !=
            accountState_[id].setFailedSeq.end())
        {
            JLOG(j_.info())
                << "onTxCheckSuccess erase " << id << " failed seq " << seq;
            accountState_[id].setFailedSeq.erase(seq);
        }
        ++accountState_[id].checkSeq;
        return;
    }
}

void
StateManager::addFailedSeq(AccountID const& id, uint32_t seq)
{
    std::lock_guard lock(mutex_);
    JLOG(j_.info()) << "addFailedSeq " << id << " " << seq;
    accountState_[id].setFailedSeq.insert(seq);
}

void
StateManager::clear()
{
    std::lock_guard lock(mutex_);
    if (accountState_.size() > 0)
    {
        JLOG(j_.info()) << "clear all account states";
        accountState_.clear();
    }
}

}