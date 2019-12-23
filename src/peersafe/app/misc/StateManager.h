#ifndef CHAINSQL_APP_MISC_STATE_MANAGER_H_INCLUDED
#define CHAINSQL_APP_MISC_STATE_MANAGER_H_INCLUDED

#include <map>
#include <ripple/protocol/AccountID.h>
#include <ripple/beast/utility/Journal.h>

namespace ripple {
class Application;

class StateManager
{
	struct State
	{
		uint32 sequence;
	};
public:
	StateManager(Application& app, beast::Journal j)
		: app_(app)
		, j_(j)
	{
	}

	uint32 getAccountSeq(AccountID const& id);
	void resetAccountSeq(AccountID const& id);

	void incrementSeq(AccountID const& id);

	void clear();

private:
	Application& app_;
	beast::Journal j_;
	std::map<AccountID, State> accountState_;
	std::mutex mutex_;
};
}

#endif

