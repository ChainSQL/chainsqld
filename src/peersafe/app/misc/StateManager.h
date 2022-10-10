#ifndef CHAINSQL_APP_MISC_STATE_MANAGER_H_INCLUDED
#define CHAINSQL_APP_MISC_STATE_MANAGER_H_INCLUDED

#include <map>
#include <set>
#include <ripple/protocol/AccountID.h>
#include <ripple/beast/utility/Journal.h>
#include <ripple/protocol/STLedgerEntry.h>

namespace ripple {
class Schema;
class ReadView;
class StateManager
{
	struct State
	{
		uint32_t signSeq;
        uint32_t checkSeq;
        std::set<uint32_t> setFailedSeq;
	};
public:
	StateManager(Schema& app, beast::Journal j)
		: app_(app)
		, j_(j)
	{
	}

	uint32_t getAndIncSignSeq(AccountID const& id, ReadView const& view);
    uint32_t getAccountCheckSeq(AccountID const& id, ReadView const& view);
    uint32_t getAccountCheckSeq(
        AccountID const& id,
        std::shared_ptr<const SLE> const sle);

	void resetAccountSeq(AccountID const& id);

    void onTxCheckSuccess(AccountID const& id);

	void addFailedSeq(AccountID const& id, uint32_t seq);

	void clear();

private:
	Schema& app_;
	beast::Journal j_;
	std::map<AccountID, State> accountState_;
	std::mutex mutex_;
};
}

#endif

