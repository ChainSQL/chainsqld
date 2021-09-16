
#include <peersafe/app/util/TableSyncUtil.h>
#include <ripple/beast/clock/abstract_clock.h>
#include <ripple/beast/clock/basic_seconds_clock.h>
#include <ripple/beast/clock/manual_clock.h>
#include <chrono>
namespace ripple{

uint256 TableSyncUtil::GetChainId(const ReadView * pView)
{
	//read chainId
	uint256 chainId(0);
	if (pView == nullptr)
		return chainId;
	auto chainIdSle = pView->read(keylet::chainId());
	if (chainIdSle)
		chainId = chainIdSle->getFieldH256(sfChainId);
	return chainId;
}

std::pair<bool, STEntry*> TableSyncUtil::IsTableSLEChanged(const STArray& aTables, LedgerIndex iLastSeq, std::string sNameInDB, bool bStrictEqual)
{
	auto iter(aTables.end());
	bool bTableFound = false;
	uint160 nameInDB = from_hex_text<uint160>(sNameInDB);
	iter = std::find_if(aTables.begin(), aTables.end(),
		[iLastSeq, nameInDB, bStrictEqual, &bTableFound](STObject const &item) {
		if (item.getFieldH160(sfNameInDB) == nameInDB) {
			bTableFound = true;
			return (bStrictEqual ?
				item.getFieldU32(sfPreviousTxnLgrSeq) == iLastSeq :
				item.getFieldU32(sfPreviousTxnLgrSeq) >= iLastSeq);
		}
		return false;
	});
	if (iter == aTables.end())
		return std::make_pair(bTableFound, nullptr);
	else
		return std::make_pair(bTableFound, (STEntry*)(&(*iter)));
}

//----------------------------------------------------------------------------------
SyncParam::SyncParam(std::string const& operationRule) 
	: ledgerSeq(0), rules(operationRule), ledgerTime("")
{
}

SyncParam::SyncParam(
    std::uint32_t seq,
    std::string const& operationRule,
    std::uint32_t closetime)
    : ledgerSeq(seq), rules(operationRule)
{
    using time_point = NetClock::time_point;
    using duration = NetClock::duration;
	using namespace std::chrono;
    auto time = time_point{duration{closetime}};
    auto sysTime = system_clock::time_point{time.time_since_epoch() + 946684800s};
    ledgerTime = date::format("%Y-%m-%d %H:%M:%S", sysTime);
}
}