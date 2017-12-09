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

#ifndef RIPPLE_RPC_TX_SINGLE_TRANS_PREPARE_H_INCLUDED
#define RIPPLE_RPC_TX_SINGLE_TRANS_PREPARE_H_INCLUDED

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
	class SecretKey;
	class TxTransactionPrepare;

	class TxSingleTransPrepare : public TxPrepareBase
	{
	public:
		TxSingleTransPrepare(Application& app, TxTransactionPrepare* trans, 
			const std::string& secret, const std::string& publickey, Json::Value& tx_json, getCheckHashFunc func,bool ws);

	private:
		virtual bool checkConfidential(const AccountID& owner, const std::string& tableName) override;

		virtual uint256 getCheckHashOld(const std::string& sAccount, const std::string& sTableName) override;
		virtual void updateCheckHash(const std::string& sAccount, const std::string& sTableName,const uint256& checkHash) override;

		virtual Blob getPassblobExtra(const std::string& sAccount, const std::string& sTableName) override;
		virtual void updatePassblob(const std::string& sAccount, const std::string& sTableName, const Blob& passblob) override;

		void updateNameInDB(const std::string& sAccount, const std::string& sTableName, const std::string& sNameInDB) override;
		virtual std::string getNameInDB(const std::string& sAccount, const std::string& sTableName) override;

	private:
		TxTransactionPrepare*								m_pTransaction;
	};
} // ripple

#endif
