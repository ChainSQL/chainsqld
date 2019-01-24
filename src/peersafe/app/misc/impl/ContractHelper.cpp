#include <peersafe/app/misc/ContractHelper.h>

namespace ripple {
	ContractHelper::ContractHelper(Application& app)
		:app_(app),
		 mTxCache("ContractHelperTxCache", 100, 60, stopwatch(), app.journal("ContractHelper")),
		 mRecordCache("ContractHelperTxCache", 100, 60, stopwatch(), app.journal("ContractHelper"))
	{
	}

	void ContractHelper::addTx(uint256 const& txHash, STTx const& tx)
	{
		auto pTxs = mTxCache.fetch(txHash).get();
		if (pTxs == nullptr)
		{
			std::vector<STTx> vecTxs;
			vecTxs.push_back(tx);
			auto p = std::make_shared<std::vector<STTx>>(vecTxs);
			mTxCache.canonicalize(txHash, p);
		}
		else
		{
			pTxs->push_back(tx);
		}
	}

	std::vector<STTx> ContractHelper::getTxsByHash(uint256 const& txHash)
	{
		auto pTxs = mTxCache.fetch(txHash).get();
		if (pTxs == nullptr)
		{
			return std::vector<STTx>();
		}
		else
		{
			auto ret = *pTxs;
			mTxCache.del(txHash, false);
			return ret;
		}
	}

	void ContractHelper::addRecord(uint256 const& handle, std::vector<std::vector<Json::Value>> const& result)
	{
		auto p = std::make_shared<std::vector<std::vector<Json::Value>>>(result);
		mRecordCache.canonicalize(handle, p);
	}

	std::vector<std::vector<Json::Value>>const& ContractHelper::getRecord(uint256 const& handle)
	{
		std::vector<std::vector<Json::Value>>* ret = mRecordCache.fetch(handle).get();
		if (ret == nullptr)
			return *std::make_shared<std::vector<std::vector<Json::Value>>>();
		return *ret;
	}

	void ContractHelper::releaseHandle(uint256 const& handle)
	{
		mRecordCache.del(handle, false);
	}

	uint256 ContractHelper::genRandomUniqueHandle()
	{
		int num = 0;
		do {
			srand(time(0));
			num = rand();
		} while (mRecordCache.fetch(uint256(num)).get() != nullptr);
		//
		return uint256(num);
	}
}

