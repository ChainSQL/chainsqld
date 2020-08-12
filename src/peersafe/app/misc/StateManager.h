#ifndef CHAINSQL_APP_MISC_STATE_MANAGER_H_INCLUDED
#define CHAINSQL_APP_MISC_STATE_MANAGER_H_INCLUDED


#include <ripple/basics/TaggedCache.h>
#include <ripple/basics/chrono.h>
#include <ripple/protocol/AccountID.h>
#include <ripple/beast/utility/Journal.h>


namespace ripple {


class Application;


class StateManager
{
public:
	struct State
	{
		uint32 sequence;

        State() : sequence(0) {}
        State(uint32 seq_) : sequence(seq_) {}
	};

    StateManager(Application& app, Stopwatch& stopwatch, beast::Journal j)
        : app_(app)
        , j_(j)
        , accountState_("StateManager", 1024000, 30, stopwatch, j)
	{
	}

	uint32 getAccountSeq(AccountID const& id, bool refresh);

	void incrementSeq(AccountID const& id);

	void sweep();

private:
	Application& app_;
	beast::Journal j_;

	TaggedCache<AccountID, State> accountState_;
};


}

#endif

