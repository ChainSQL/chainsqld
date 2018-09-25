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

#ifndef RIPPLE_RPC_TX_PREPARE_BASE_H_INCLUDED
#define RIPPLE_RPC_TX_PREPARE_BASE_H_INCLUDED

#include <ripple/json/json_value.h>
#include <ripple/basics/base_uint.h>
#include <ripple/protocol/Protocol.h>
#include <ripple/protocol/AccountID.h>
#include <ripple/beast/utility/Journal.h>
#include <string>
#include <map>

namespace ripple {

	class Application;
	class Config;
	class SecretKey;

	using getCheckHashFunc = std::function<uint256(uint160)>;

	class TxPrepareBase
	{
	public:
		TxPrepareBase(Application& app, const std::string& secret, const std::string& publickey, Json::Value& tx_json, getCheckHashFunc func, bool ws);
		virtual ~TxPrepareBase();

		virtual Json::Value prepare();	
        static Json::Value prepareFutureHash(const Json::Value& tx_json, Application& app, bool bWs);     //just check future hash is right.

		bool isConfidential();
	protected:
		Json::Value& getTxJson();
		Json::Value prepareBase();
        Json::Value prepareGetRaw();        

		Json::Value checkBaseInfo(const Json::Value& tx_json, Application& app, bool bWs);
		bool checkConfidentialBase(const AccountID& owner, const std::string& tableName);
		//get decrypted pass_blob
		std::pair<Blob, Json::Value> getPassBlobBase(AccountID& ownerId, AccountID& userId, boost::optional<SecretKey> secret_key);

		Json::Value prepareCheckHash(const std::string& sRaw, const uint256& checkHash, uint256& checkHashNew);

		Json::Value parseTableName();
	private:
		virtual void updateCheckHash(const std::string& sAccount, const std::string& sTableName, const uint256& checkHash) {};
		virtual uint256 getCheckHashOld(const std::string& sAccount, const std::string& sTableName);

		virtual Blob getPassblobExtra(const std::string& sAccount, const std::string& sTableName);
		virtual void updatePassblob(const std::string& sAccount, const std::string& sTableName, const Blob& passblob) {};
        virtual void updateNameInDB(const std::string& sAccount, const std::string& sTableName, const std::string& sNameInDB) {};
		virtual std::string getNameInDB(const std::string& sAccount, const std::string& sTableName);

		virtual bool checkConfidential(const AccountID& owner, const std::string& tableName);
        void updateInfo(const std::string& sAccount, const std::string& sTableName, const std::string& sNameInDB);
		Json::Value prepareDBName();        

		Json::Value prepareVL(Json::Value& json);

		Json::Value prepareRawEncode();
		Json::Value prepareStrictMode();
		void preparePressData();

		Json::Value prepareForOperating();
		Json::Value prepareForCreate();
		Json::Value prepareForAssign();

		std::pair<uint256,Json::Value> getCheckHash(const std::string& sAccount,const std::string& sTableName);

		std::pair<Blob, Json::Value> getPassBlob(
			AccountID& ownerId, AccountID& userId, boost::optional<SecretKey> secret_key);
	protected:
		Application&										app_;
		const std::string&									secret_;
        const std::string&                                  public_;
		Json::Value&										tx_json_;
		getCheckHashFunc									getCheckHashFunc_;
		bool												ws_;

	private:
		std::string                                         sTableName_;
		uint160                                             u160NameInDB_;		
		AccountID                                           ownerID_;

		//is table raw encoded
		bool												m_bConfidential;
	};
} // ripple

#endif
