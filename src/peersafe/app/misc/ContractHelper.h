#ifndef __H_CHAINSQL_CONTRACT_HELPER_H__
#define __H_CHAINSQL_CONTRACT_HELPER_H__

#include <vector>
#include <boost/bind.hpp>
#include <ripple/protocol/STTx.h>
#include <ripple/basics/TaggedCache.h>
#include <ripple/json/json_value.h>
#include <ripple/basics/base_uint.h>
#include <ripple/app/main/Application.h>

namespace ripple {

class ContractHelper
{
public:
	ContractHelper(Application& app);

	
	//new tx when smart contract executing
	void addTx(uint256 const& txHash, STTx const& tx);
	std::vector<STTx> getTxsByHash(uint256 const& txHash);

	//for get interface
	void addRecord(uint256 const& handle, std::vector<std::vector<Json::Value>> const& result);
	std::vector<std::vector<Json::Value>>const& getRecord(uint256 const& handle);
	void releaseHandle(uint256 const& handle);
	uint256 genRandomUniqueHandle();

private:
	TaggedCache<uint256, std::vector<STTx>>	mTxCache;
	TaggedCache<uint256, std::vector<std::vector<Json::Value>>>		
											mRecordCache;
	Application&							app_;
};

}
#endif