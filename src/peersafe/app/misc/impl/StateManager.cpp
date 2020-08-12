
#include <peersafe/app/misc/StateManager.h>
#include <ripple/app/ledger/OpenLedger.h>
#include <ripple/app/main/Application.h>

namespace ripple {

uint32 StateManager::getAccountSeq(AccountID const& id, bool refresh)
{
    State state;

    bool cached = accountState_.retrieve(id, state);

    if (refresh || !cached)
    {
        auto sle = app_.openLedger().current()->read(keylet::account(id));
        if (sle)
        {
            uint32 curSequence = sle->getFieldU32(sfSequence);
            if (curSequence > state.sequence)
            {
                state.sequence = curSequence;
                auto newState = std::make_shared<State>(state);
                accountState_.canonicalize(id, newState, refresh);
            }
        }
    }

	return state.sequence;
}

void StateManager::incrementSeq(AccountID const& id)
{
    auto state = accountState_.fetch(id);
    if (state)
    {
        state->sequence++;
    }
}

void StateManager::sweep()
{
    accountState_.sweep();
}

}