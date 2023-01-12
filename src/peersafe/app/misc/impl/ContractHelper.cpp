#include <peersafe/app/misc/ContractHelper.h>
#include <peersafe/schema/Schema.h>
#include <peersafe/protocol/STMap256.h>
#include <ripple/app/tx/impl/ApplyContext.h>
#include <ripple/shamap/SHAMap.h>
#include <ripple/ledger/OpenView.h>
#include <ripple/protocol/digest.h>

namespace ripple {

    ContractHelper::ContractHelper(Schema& app)
        : app_(app)
        , mTxCache(
              "ContractHelperTxCache",
              100,
              std::chrono::seconds{60},
              stopwatch(),
              app.journal("ContractHelper"))
        , mRecordCache(
              "ContractHelperTxCache",
              100,
              std::chrono::seconds{60},
              stopwatch(),
              app.journal("ContractHelper"))
        , mJournal(app_.journal("ContractHelper"))
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
			mTxCache.canonicalize_replace_client(txHash, p);
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
		mRecordCache.canonicalize_replace_client(handle, p);
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

	/////////////////////////////////////////////////////////////////////
	// Contract state storage related
    boost::optional<uint256>
    ContractHelper::fetchFromCache(
        ApplyContext& ctx,
        AccountID const& contract,
        uint256 const& key,
        bool bQuery /*=false*/)
    {
        if (auto value = ctx.fetchFromDirty(contract, key); value != boost::none)
            return value;

        if (bQuery)
            return boost::none;

        if (auto data = ctx.fetchFromStateCache(contract, key);
            data != boost::none)
            return data->value;
        
		return boost::none;
    }

    std::shared_ptr<SHAMap>
    ContractHelper::getSHAMap(
        AccountID const& contract,
        boost::optional<uint256> const& root,
        bool bQuery /*=false*/
    )
    {
        auto funMakeShamap = [&, this](){
            auto mapPtr = std::make_shared<SHAMap>(SHAMapType::CONTRACT, app_.getNodeFamily());
            if (root && !mapPtr->fetchRoot(SHAMapHash{*root}, nullptr))
            {
                JLOG(mJournal.warn()) << "Get storage root failed for contract: "
                                      << to_string(contract) << ",root hash: "<<*root;
                mapPtr = nullptr;
            }
            return mapPtr;
        };
        
        std::shared_ptr<SHAMap> mapPtr = nullptr;
        //!!!!! potential threat, please consider the thread conflict about mShaMapCache
        if(bQuery)
        {
            mapPtr = funMakeShamap();
        }
        else
        {
            if(mShaMapCache.find(contract) == mShaMapCache.end())
            {
                mShaMapCache[contract] = funMakeShamap();
            }
            mapPtr = mShaMapCache[contract];
        }

        return mapPtr;
    }

    boost::optional<uint256>
    ContractHelper::fetchFromDB(
        AccountID const& contract,
        boost::optional<uint256> const& root,
        uint256 const& key,
        bool bQuery /*=false*/)
    {
        if (!root || *root == uint256(0))
            return boost::none;

        std::shared_ptr<SHAMap> mapPtr = getSHAMap(contract, root, bQuery);
        if (mapPtr == nullptr)
            return boost::none;
        try
        {
            auto realKey = sha512Half(contract, key);
            auto const& item = mapPtr->peekItem(realKey);
            if (!item)
                return boost::none;
            uint256 ret;
            std::memcpy(&ret, item->data(), item->size());
            return ret;
        }
        catch (SHAMapMissingNode const& mn)
        {
            JLOG(mJournal.warn())
                << "Fetch item for key:" << to_string(key) << " of contract "
                << to_string(contract) << " failed :" << mn.what();
            return boost::none;
        } 
    }

    boost::optional<uint256>
    ContractHelper::fetchValue(
        ApplyContext& ctx,
        AccountID const& contract,
        boost::optional<uint256> const& root,
        uint256 const& key,
        bool bQuery/*=false*/)
    {
        auto ret = fetchFromCache(ctx,contract, key, bQuery);
        if (ret)
            return ret;

        return fetchFromDB(contract, root, key, bQuery);
    }

    void
    ContractHelper::clearCache()
    {
        mShaMapCache.clear();
    }

    void
    ContractHelper::setStorage(
        ApplyContext& ctx,
        AccountID const& contract,
        boost::optional<uint256> root,
        uint256 const& key,
        uint256 const& value)
    {
        if (ctx.fetchFromDirty(contract,key) != boost::none)
        {
            ctx.setDirtyValue(contract, key, value);
            return;
        }
        
        if (auto data = ctx.fetchFromStateCache(contract,key); 
            data != boost::none)
        {
            ctx.setDirtyValue(contract, key, value);
            ctx.setDirtyExistInDB(contract, key, data->existInDB);
            return;
        }

        ctx.setDirtyValue(contract, key, value);
        if (fetchFromDB(contract,root,key)) 
            ctx.setDirtyExistInDB(contract, key, true);
        else
            ctx.setDirtyExistInDB(contract, key, false);
    }

    std::shared_ptr<SHAMapItem const>
    makeSHAMapItem(uint256 const& key,uint256 const& value)
    {
        Serializer ss;
        ss.add256(value);
        return std::make_shared<SHAMapItem const>(key, std::move(ss));
    }

    ContractHelper::ValueOpType
    ContractHelper::getOpType(ContractValueType const& value)
    {
        auto type = ValueOpType::invalid;
        if (value.value == uint256(0))
        {
            if (value.existInDB)
                type = ValueOpType::erase;
        }
        else
        {
            if (value.existInDB)
                type = ValueOpType::modify;
            else
                type = ValueOpType::insert;
        }
        return type;
    }

    void
    ContractHelper::apply(OpenView& open)
    {
        auto& stateCache = open.getStateCache();
        if (stateCache.empty())
            return;
        try
        {
            for (auto it = stateCache.begin(); it != stateCache.end(); it++)
            {
                AccountID const& contract = it->first;
                auto const k = keylet::account(contract);
                auto pSle = open.read(k);
                if (pSle != nullptr)
                {
                    // For a contract SLE
                    auto newSle = std::make_shared<SLE>(*pSle);
                    auto& mapStore = newSle->peekFieldM256(sfStorageOverlay);
                    std::shared_ptr<SHAMap> mapPtr =
                        getSHAMap(contract, mapStore.rootHash());
                    if (mapPtr == nullptr)
                        continue;

                    // Modify SHAMap
                    auto iter = it->second.begin();
                    while (iter != it->second.end())
                    {
                        auto key = sha512Half(contract, iter->first);
                        auto type = getOpType(iter->second);
                        switch (type)
                        {
                            case ValueOpType::insert: {
                                auto item =
                                    makeSHAMapItem(key, iter->second.value);
                                mapPtr->addGiveItem(item, false, false);
                                break;
                            }
                            case ValueOpType::modify: {
                                auto item =
                                    makeSHAMapItem(key, iter->second.value);
                                mapPtr->updateGiveItem(item, false, false);
                                break;
                            }
                            case ValueOpType::erase:
                                mapPtr->delItem(key);
                                break;
                            default:
                                break;
                        }
                        iter++;
                    }
                    // Store to disk
                    mapPtr->flushDirty(hotACCOUNT_NODE, open.seq());
                    mapStore.updateRoot(mapPtr->getHash().as_uint256());

                    // Update SLE
                    newSle->setFieldM256(sfStorageOverlay, mapStore);
                    open.rawReplace(newSle);
                }
            }
        }
        catch (SHAMapMissingNode const& mn)
        {
            JLOG(mJournal.warn()) << "ContractHelper::apply failed:" << mn.what();
        }        
    }
}

