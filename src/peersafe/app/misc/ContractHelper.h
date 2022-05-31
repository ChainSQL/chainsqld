#ifndef __H_CHAINSQL_CONTRACT_HELPER_H__
#define __H_CHAINSQL_CONTRACT_HELPER_H__

#include <vector>
#include <boost/bind.hpp>
#include <ripple/protocol/STTx.h>
#include <ripple/basics/TaggedCache.h>
#include <ripple/json/json_value.h>
#include <ripple/basics/base_uint.h>
#include <ripple/shamap/SHAMap.h>
#include <ripple/ledger/OpenView.h>
#include <ripple/protocol/TER.h>
#include <peersafe/schema/Schema.h>

namespace ripple {

class ContractHelper
{
public:
    enum class ValueOpType { invalid = 0, insert = 1, modify = 2, erase = 3 };
    struct ValueType
    {
        uint256 value;
        bool existInDB;

        ValueType() : value(uint256(0)), existInDB(false)
        {}
    };

	using map256 = std::map<uint256, ValueType>;
	ContractHelper(Schema& app);

	//new tx when smart contract executing
	void addTx(uint256 const& txHash, STTx const& tx);
	std::vector<STTx> getTxsByHash(uint256 const& txHash);

	//for get interface
	void addRecord(uint256 const& handle, std::vector<std::vector<Json::Value>> const& result);
	std::vector<std::vector<Json::Value>>const& getRecord(uint256 const& handle);
	void releaseHandle(uint256 const& handle);
	uint256 genRandomUniqueHandle();

	//Contract state storage related
	bool hasStorage(AccountID const& contract);

    boost::optional<uint256>
    fetchFromCache(
        AccountID const& contract,
        uint256 const& key,
        bool bQuery = false);

    boost::optional<uint256>
    fetchFromDB(
        AccountID const& contract,
        boost::optional<uint256> const& root,
        uint256 const& key,
        bool bQuery = false);

    boost::optional<uint256>
    fetchValue(
        AccountID const& contract,
        boost::optional<uint256> const& root,
        uint256 const& key,
        bool bQuery = false);

    std::shared_ptr<SHAMap>
    getSHAMap(
        AccountID const& contract, 
        boost::optional<uint256> const& root,
        bool bQuery = false
    );

	//set storage to dirty
	void
    setStorage(
        AccountID const& contract,
        boost::optional<uint256> root,
        uint256 const& key,
        uint256 const& value);
	//clear dirty before tx apply
    void clearDirty();
	//merge dirty to main cache
    void flushDirty(TER code);
    void clearCache();

    //apply cached modification to SHAMap and modify storage_overlay in contract SLE
    void apply(OpenView& openView);

    ValueOpType 
    getOpType(ValueType const& value);

private:
	Schema&									app_;
	TaggedCache<uint256, std::vector<STTx>>	mTxCache;
	TaggedCache<uint256, std::vector<std::vector<Json::Value>>>		
											mRecordCache;

	//LedgerIndex						mCurSeq;
    std::map<AccountID, map256>     mDirtyCache;
    std::map<AccountID, map256>     mStateCache;
    std::map<AccountID, std::shared_ptr<SHAMap>> mShaMapCache;
    beast::Journal                  mJournal;
};

}
#endif