#ifndef CHAINSQL_APP_MISC_STATE_MANAGER_H_INCLUDED
#define CHAINSQL_APP_MISC_STATE_MANAGER_H_INCLUDED

#include <map>
#include <ripple/protocol/AccountID.h>
#include <ripple/beast/utility/Journal.h>
#include <ripple/protocol/STLedgerEntry.h>

namespace ripple {
class Schema;

class StateManager
{
	struct State
	{
		uint32_t sequence;
	};
public:
	StateManager(Schema& app, beast::Journal j)
		: app_(app)
		, j_(j)
	{
	}

	uint32_t getAccountSeq(AccountID const& id);
    uint32_t getAccountSeq(AccountID const& id,std::shared_ptr<const SLE> const sle);

	void resetAccountSeq(AccountID const& id);

	void incrementSeq(AccountID const& id);

	void clear();

private:
	Schema& app_;
	beast::Journal j_;
	std::map<AccountID, State> accountState_;
	std::mutex mutex_;
};
}

#endif

