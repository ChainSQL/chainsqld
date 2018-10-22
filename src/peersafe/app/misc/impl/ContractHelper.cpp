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

	void releaseHandle_(ContractHelper *contractHelper, boost::asio::deadline_timer*t, uint256 handle)
	{
		contractHelper->releaseHandle(handle);
		t->cancel();
	}

	void ContractHelper::addRecord(uint256 const& handle, Json::Value const& result)
	{
		auto p = std::make_shared<Json::Value>(result);
		mRecordCache.canonicalize(handle, p);
		mHandleFlag.emplace(handle);
		boost::asio::io_service io;
		boost::asio::deadline_timer t(io, boost::posix_time::seconds(60));
		int count = 0;
		t.async_wait(boost::bind(releaseHandle_, &app_.getContractHelper(), &t, handle));
		io.run();
	}

	Json::Value ContractHelper::getRecord(uint256 const& handle)
	{
		Json::Value* ret = mRecordCache.fetch(handle).get();
		if (ret == nullptr)
			return Json::Value();
		return *ret;
	}

	void ContractHelper::releaseHandle(uint256 const& handle)
	{
		mRecordCache.del(handle, false);
		mHandleFlag.erase(handle);
	}

	uint256 ContractHelper::genRandomUniqueHandle()
	{
		int num = 0;
		do {
			srand(time(0));
			num = rand();
		} while (mHandleFlag.find(uint256(num)) != mHandleFlag.end());
		//
		return uint256(num);
	}
}

