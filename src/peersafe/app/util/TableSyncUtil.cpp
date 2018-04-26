
#include <peersafe/app/util/TableSyncUtil.h>
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

std::pair<bool, STEntry*> TableSyncUtil::IsTableSLEChanged(const STArray& aTables, LedgerIndex iLastSeq, AccountID accountID, std::string sTableName, bool bStrictEqual)
{
	auto iter(aTables.end());
	bool bTableFound = false;
	iter = std::find_if(aTables.begin(), aTables.end(),
		[iLastSeq, accountID, sTableName, bStrictEqual, &bTableFound](STObject const &item) {
		uint160 uTxDBName = item.getFieldH160(sfNameInDB);
		if (to_string(uTxDBName) == sTableName) {
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
}