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
#include <peersafe/rpc/impl/TxPrepareBase.h>
#include <peersafe/rpc/TableUtils.h>
#include <peersafe/rpc/impl/TableAssistant.h>

namespace ripple {
	TxSingleTransPrepare::TxSingleTransPrepare(Application& app, TxTransactionPrepare* trans,
		const std::string& secret, const std::string& publickey, Json::Value& tx_json, getCheckHashFunc func,bool ws) :
		TxPrepareBase(app, secret, publickey, tx_json, func,ws),
		m_pTransaction(trans)
	{
	}

	bool TxSingleTransPrepare::checkConfidential(const AccountID& owner, const std::string& tableName)
	{
		auto sAccountId = to_string(owner);
		if (m_pTransaction->getPassblob(sAccountId, tableName).size() > 0)
			return true;

		return checkConfidentialBase(owner, tableName);
	}


	Blob TxSingleTransPrepare::getPassblobExtra(const std::string& sAccount, const std::string& sTableName)
	{
		return m_pTransaction->getPassblob(sAccount, sTableName);
	}

	void TxSingleTransPrepare::updatePassblob(const std::string& sAccount, const std::string& sTableName, const Blob& passblob)
	{
		m_pTransaction->updatePassblob(sAccount, sTableName, passblob);
	}

	void TxSingleTransPrepare::updateNameInDB(const std::string& sAccount, const std::string& sTableName, const std::string& sNameInDB)
	{
		m_pTransaction->updateNameInDB(sAccount, sTableName, sNameInDB);
	}
	std::string TxSingleTransPrepare::getNameInDB(const std::string& sAccount, const std::string& sTableName)
	{
		return m_pTransaction->getNameInDB(sAccount, sTableName);
	}

	uint256 TxSingleTransPrepare::getCheckHashOld(const std::string& sAccount, const std::string& sTableName)
	{
		return m_pTransaction->getCheckHash(sAccount, sTableName);
	}

	void TxSingleTransPrepare::updateCheckHash(const std::string& sAccount, const std::string& sTableName, const uint256& checkHash)
	{
		m_pTransaction->updateCheckHash(sAccount, sTableName, checkHash);
	}
}
