#include <peersafe/app/misc/ContractHelper.h>
#include <peersafe/schema/Schema.h>
#include <peersafe/protocol/STMap256.h>
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
    bool
    ContractHelper::hasStorage(AccountID const& contract)
    {
        return mStateCache.find(contract) != mStateCache.end() || 
			   mDirtyCache.find(contract) != mDirtyCache.end();
    }

    boost::optional<uint256>
    ContractHelper::fetchFromCache(
        AccountID const& contract,
        uint256 const& key,
        bool bQuery /*=false*/)
    {
        if (bQuery)
            return boost::none;

        if (!hasStorage(contract))
            return boost::none;
        if (mStateCache[contract].find(key) == mStateCache[contract].end() && 
			mDirtyCache[contract].find(key) == mDirtyCache[contract].end())
            return boost::none;
        
		if (mDirtyCache[contract].find(key) != mDirtyCache[contract].end())
            return mDirtyCache[contract][key].value;

		if (mStateCache[contract].find(key) != mStateCache[contract].end())
            return mStateCache[contract][key].value;

		return boost::none;
    }

    std::shared_ptr<SHAMap>
    ContractHelper::getSHAMap(
        AccountID const& contract,
        boost::optional<uint256> const& root,
        bool bQuery /*=false*/
    )
    {
        std::shared_ptr<SHAMap> mapPtr = nullptr;
        if (mShaMapCache.find(contract) == mShaMapCache.end() || bQuery)
        {
            mapPtr = std::make_shared<SHAMap>(
                SHAMapType::CONTRACT, app_.getNodeFamily());
            if (root && !mapPtr->fetchRoot(SHAMapHash{*root}, nullptr))
            {
                JLOG(mJournal.warn()) << "Get storage root failed for contract: "
                                      << to_string(contract) << ",root hash: "<<*root;
                return nullptr;
            }
            if (!bQuery)
                mShaMapCache[contract] = mapPtr;
        }
        else
            mapPtr = mShaMapCache[contract];

        return mapPtr;
    }

    boost::optional<uint256>
    ContractHelper::fetchValue(
        AccountID const& contract,
        uint256 const& root,
        uint256 const& key,
        bool bQuery/*=false*/)
    {
        if (root == uint256(0))
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
                << to_string(contract) << " failed :" <<mn.what();
            return boost::none;
        } 
    }

    void
    ContractHelper::clearDirty()
    {
	    mDirtyCache.clear();
    }

    void
    ContractHelper::flushDirty(TER code)
    {
        if (code == TEScodes::tesSUCCESS)
        {
            auto it = mDirtyCache.begin();
            while (it != mDirtyCache.end())
            {
                auto iter = it->second.begin();
                while (iter != it->second.end())
                {
                    mStateCache[it->first][iter->first] = iter->second;
                    iter++;
                }
                it++;
            }
        }
        
        clearDirty();
	}

    void
    ContractHelper::clearCache()
    {
        mStateCache.clear();
        mShaMapCache.clear();
    }

	void
    ContractHelper::eraseStorage(
        AccountID const& contract,
        boost::optional<uint256> root,
        uint256 const& key)
    {
        if (root && (fetchFromCache(contract,key)|| fetchValue(contract,*root,key)))
        {
            ValueType value(uint256(0), ValueOpType::erase);
            mDirtyCache[contract][key] = value;
        }
    }

    void ContractHelper::setStorage(
        AccountID const& contract,
        boost::optional<uint256> root,
        uint256 const& key,
        uint256 const& value)
    {
        if (mDirtyCache.find(contract) != mDirtyCache.end() &&
            mDirtyCache[contract].find(key) != mDirtyCache[contract].end())
        {
            mDirtyCache[contract][key].value = value;
            return;		
        }
            
		if (mStateCache.find(contract) != mStateCache.end() &&
            mStateCache[contract].find(key) != mStateCache[contract].end())
        {
            mDirtyCache[contract][key].value = value;
            mDirtyCache[contract][key].type = mStateCache[contract][key].type;
            return;	
        }

        mDirtyCache[contract][key].value = value;
        if (root && fetchValue(contract,*root,key)) 
            mDirtyCache[contract][key].type = ValueOpType::modify;
        else
            mDirtyCache[contract][key].type = ValueOpType::insert;
    }

    std::shared_ptr<SHAMapItem const>
    makeSHAMapItem(uint256 const& key,uint256 const& value)
    {
        Serializer ss;
        ss.add256(value);
        return std::make_shared<SHAMapItem const>(key, std::move(ss));
    }

    void
    ContractHelper::apply(OpenView& open)
    {
        if (mStateCache.empty())
            return;
        try
        {
            for (auto it = mStateCache.begin(); it != mStateCache.end(); it++)
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
                    for (auto iter = it->second.begin();
                         iter != it->second.end();
                         iter++)
                    {
                        auto key = sha512Half(contract, iter->first);
                        switch (iter->second.type)
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

