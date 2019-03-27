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

#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/JsonFields.h>
#include <peersafe/rpc/impl/TxTransactionPrepare.h>
#include <peersafe/rpc/impl/TxSingleTransPrepare.h>
#include <peersafe/rpc/TableUtils.h>
#include <ripple/protocol/ErrorCodes.h>

namespace ripple {
	TxTransactionPrepare::TxTransactionPrepare(Application& app, const std::string& secret, const std::string& publickey, Json::Value& tx_json, getCheckHashFunc func,bool ws):
		app_(app),
		secret_(secret),
        public_(publickey),
		tx_json_(tx_json),
		getCheckHashFunc_(func),
		ws_(ws)
	{

		//create tmp map
		m_pMap = std::make_shared<std::map<std::string,std::map<std::string, transInfo>>>();
	}
	TxTransactionPrepare::~TxTransactionPrepare()
	{

	}
	Json::Value TxTransactionPrepare::prepare()
	{
		Json::Value ret;
		if (!tx_json_.isMember(jss::Statements))
			return RPC::missing_field_error(jss::Statements);
		if (tx_json_[jss::Statements].size() == 0)
		{
			return RPC::invalid_field_error(jss::Statements);
		}
		bool bNeedVerify = true;
		bool bStrictMode = false;
		if (tx_json_.isMember(jss::StrictMode))
			bStrictMode = tx_json_[jss::StrictMode].asBool();

        ret = TxPrepareBase::prepareFutureHash(tx_json_, app_, ws_);
		if(ret.isMember(jss::error))
            return ret;

		Json::Value statement(Json::arrayValue);
		for (auto json : tx_json_[jss::Statements])
		{
			json[jss::Account] = tx_json_[jss::Account].asString();
			if (bStrictMode && !json.isMember(jss::StrictMode))
			{
				json[jss::StrictMode] = bStrictMode;
			}

			auto pTransPrepare = std::make_shared<TxSingleTransPrepare>(app_,this,secret_,public_,json,getCheckHashFunc_,ws_);
			auto ret = pTransPrepare->prepare();
			if (ret.isMember(jss::error))
			{
				return ret;
			}
			else
			{
				json.removeMember(jss::StrictMode);
				json.removeMember(jss::Account);
				statement.append(json);
				if (pTransPrepare->isConfidential() && bNeedVerify)
					bNeedVerify = false;
			}
		}

		assert(tx_json_[jss::Statements].size() == statement.size());

		if (!ws_)
		{
			tx_json_[jss::Statements] = strHex(statement.toStyledString());
		}
		else
		{
			if (tx_json_.isMember(jss::StrictMode))
				tx_json_.removeMember(jss::StrictMode);
			tx_json_[jss::Statements] = statement;
		}

		if (!bNeedVerify)
			tx_json_[jss::NeedVerify] = 0;
		else if (!tx_json_.isMember(jss::NeedVerify))
			tx_json_[jss::NeedVerify] = 1;
		return ret;

	}

	void TxTransactionPrepare::updateNameInDB(const std::string& accountId, const std::string& tableName, const std::string& sNameInDB)
	{
		(*m_pMap)[accountId][tableName].sNameInDB = sNameInDB;
	}

	std::string TxTransactionPrepare::getNameInDB(const std::string& accountId, const std::string& tableName)
	{
		if (m_pMap->find(accountId) != m_pMap->end())
		{
			if ((*m_pMap)[accountId].find(tableName) != (*m_pMap)[accountId].end())
				return (*m_pMap)[accountId][tableName].sNameInDB;
		}

		return "";
	}

	void TxTransactionPrepare::updateCheckHash(const std::string& accountId, const std::string& tableName, const uint256& checkHash)
	{
		(*m_pMap)[accountId][tableName].uTxCheckHash = checkHash;
	}

	uint256 TxTransactionPrepare::getCheckHash(const std::string& accountId, const std::string& tableName)
	{
		if (m_pMap->find(accountId) != m_pMap->end())
		{
			if ((*m_pMap)[accountId].find(tableName) != (*m_pMap)[accountId].end())
				return (*m_pMap)[accountId][tableName].uTxCheckHash;
		}

		return beast::zero;
	}

	void TxTransactionPrepare::updatePassblob(const std::string& accountId, const std::string& tableName, const Blob& pass)
	{
		(*m_pMap)[accountId][tableName].pass = pass;
	}

	Blob TxTransactionPrepare::getPassblob(const std::string& accountId, const std::string& tableName)
	{
		if (m_pMap->find(accountId) != m_pMap->end())
		{
			if ((*m_pMap)[accountId].find(tableName) != (*m_pMap)[accountId].end())
				return (*m_pMap)[accountId][tableName].pass;
		}

		Blob blob;
		return blob;
	}
}
