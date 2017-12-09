//------------------------------------------------------------------------------
/*
 This file is part of chainsqld: https://github.com/chainsql/chainsqld
 Copyright (c) 2016-2018 Peersafe Technology Co., Ltd.
 
	chainsqld is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.
 
	chainsqld is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
 */
//==============================================================================

#ifndef RIPPLE_RPC_TX_TRANSACTION_PREPARE_H_INCLUDED
#define RIPPLE_RPC_TX_TRANSACTION_PREPARE_H_INCLUDED

#include <ripple/json/json_value.h>
#include <ripple/basics/base_uint.h>
#include <ripple/protocol/Protocol.h>
#include <ripple/protocol/AccountID.h>
#include <ripple/beast/utility/Journal.h>
#include <peersafe/rpc/impl/TxPrepareBase.h>
#include <string>
#include <map>

namespace ripple {

	class Application;
	class TxPrepareBase;

	class TxTransactionPrepare
	{
		//transaction related
		typedef struct
		{
			uint256														 uTxCheckHash;
			Blob														 pass;
			std::string													 sNameInDB;
		}transInfo;

	public:
		TxTransactionPrepare(Application& app, const std::string& secret,const std::string& publickey, Json::Value& tx_json, getCheckHashFunc func,bool ws);
		virtual ~TxTransactionPrepare();

		Json::Value prepare();

		void updateNameInDB(const std::string& accountId,const std::string& tableName,const std::string& sNameInDB);
		std::string getNameInDB(const std::string& accountId, const std::string& tableName);

		void updateCheckHash(const std::string& accountId, const std::string& tableName, const uint256& checkHash);
		uint256 getCheckHash(const std::string& accountId, const std::string& tableName);

		void updatePassblob(const std::string& accountId, const std::string& tableName, const Blob& pass);
		Blob getPassblob(const std::string& accountId, const std::string& tableName);

	private:
		//key: accountId,value:map
		//key: table_name,value:transInfo
		std::shared_ptr<std::map<std::string,std::map<std::string, transInfo>>>	m_pMap;

		Application&										app_;
		const std::string&									secret_;
        const std::string&                                  public_;
		Json::Value&										tx_json_;
		getCheckHashFunc									getCheckHashFunc_;
		bool												ws_;
	};
} // ripple

#endif
