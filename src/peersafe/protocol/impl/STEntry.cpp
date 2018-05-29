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

#include <BeastConfig.h>
#include <ripple/protocol/HashPrefix.h>
#include <ripple/basics/contract.h>
#include <ripple/basics/Log.h>
#include <ripple/json/to_string.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/json/json_reader.h>
#include <ripple/protocol/JsonFields.h>
#include <peersafe/protocol/STEntry.h>

namespace ripple {

    STEntry::STEntry()
        : STObject(getFormat(), sfEntry)
    {

    }
    void STEntry::init(ripple::Blob tableName, uint160 nameInDB,uint8 deleted, uint32 createLgrSeq, uint256 createdLedgerHash, uint256 createdTxnHash, uint32 txnLedgerSequence, uint256 txnLedgerhash,uint32 prevTxnLedgerSequence,uint256 prevTxnLedgerhash, uint256 txCheckhash, STArray users)
    {
        setFieldVL(sfTableName, tableName); //if (tableName == NUll ) then set to null ,no exception
        setFieldH160(sfNameInDB, nameInDB);
        setFieldU32(sfCreateLgrSeq, createLgrSeq);
        setFieldH256(sfCreatedLedgerHash, createdLedgerHash);
        setFieldH256(sfCreatedTxnHash, createdTxnHash);
        setFieldU32(sfTxnLgrSeq, txnLedgerSequence);
        setFieldH256(sfTxnLedgerHash, txnLedgerhash);
        setFieldU32(sfPreviousTxnLgrSeq, prevTxnLedgerSequence);
        setFieldH256(sfPrevTxnLedgerHash, prevTxnLedgerhash);
		setFieldH256(sfTxCheckHash, txCheckhash);
        setFieldArray(sfUsers, users);

    }

	void STEntry::initOperationRule(ripple::Blob operationRule)
	{
		Json::Value jsonRule;
		STObject obj_rules(sfRules);
		auto sOperationRule = strCopy(operationRule);
		if (Json::Reader().parse(sOperationRule, jsonRule))
		{			
			std::string rule;
			if (jsonRule.isMember(jss::Insert))
			{
				rule = jsonRule[jss::Insert].toStyledString();
				obj_rules.setFieldVL(sfInsertRule, strCopy(rule));
			}
			if (jsonRule.isMember(jss::Update))
			{
				rule = jsonRule[jss::Update].toStyledString();
				obj_rules.setFieldVL(sfUpdateRule, strCopy(rule));
			}
			if (jsonRule.isMember(jss::Delete))
			{
				rule = jsonRule[jss::Delete].toStyledString();
				obj_rules.setFieldVL(sfDeleteRule, strCopy(rule));
			}
			if (jsonRule.isMember(jss::Get))
			{
				rule = jsonRule[jss::Get].toStyledString();
				obj_rules.setFieldVL(sfGetRule, strCopy(rule));
			}
		}
		setFieldObject(sfRules, obj_rules);
	}

	std::string STEntry::getOperationRule(TableOpType opType) const
	{
		std::string ret;
		if (!isSqlStatementOpType(opType) && opType != R_GET)
			return ret;

		if (!isFieldPresent(sfRules))
		{
			return ret;
		}
		auto obj = getFieldObject(sfRules);
		switch (opType)
		{
		case R_INSERT:
			if (obj.isFieldPresent(sfInsertRule))
				ret = strCopy(obj.getFieldVL(sfInsertRule));
			break;
		case R_UPDATE:
			if (obj.isFieldPresent(sfUpdateRule))
				ret = strCopy(obj.getFieldVL(sfUpdateRule));
			break;
		case R_DELETE:
			if (obj.isFieldPresent(sfDeleteRule))
				ret = strCopy(obj.getFieldVL(sfDeleteRule));
			break;
		case R_GET:
			if (obj.isFieldPresent(sfGetRule))
				ret = strCopy(obj.getFieldVL(sfGetRule));
			break;
		default:
			break;
		}

		return ret;
	}

	bool STEntry::hasAuthority(const AccountID& account, TableRoleFlags flag)
	{
		//check the authority
		auto const & aUsers(getFieldArray(sfUsers));
		//if all user has authority
		bool bAllGrant = false;
		for (auto const& user : aUsers)
		{
			auto const userID = user.getAccountID(sfUser);
			if (account == userID )
			{
				if ((user.getFieldU32(sfFlags) & flag) == 0)
					return false;
				else
					return true;
			}
			if (userID == noAccount())
				if ((user.getFieldU32(sfFlags) & flag) != 0)
					bAllGrant = true;
		}

		return bAllGrant;
	}

	bool STEntry::isConfidential()
	{
		auto const & aUsers(getFieldArray(sfUsers));
		if (aUsers.size() > 0 && aUsers[0].isFieldPresent(sfToken))
			return true;
		return false;
	}

    SOTemplate const& STEntry::getFormat()
    {
        struct FormatHolder
        {
            SOTemplate format;

            FormatHolder()
            {
                format.push_back(SOElement(sfTableName, SOE_REQUIRED));
                format.push_back(SOElement(sfNameInDB, SOE_REQUIRED));
                format.push_back(SOElement(sfCreateLgrSeq, SOE_REQUIRED));
                format.push_back(SOElement(sfCreatedLedgerHash, SOE_REQUIRED));
                format.push_back(SOElement(sfCreatedTxnHash, SOE_REQUIRED));
                format.push_back(SOElement(sfTxnLgrSeq, SOE_REQUIRED));
                format.push_back(SOElement(sfTxnLedgerHash, SOE_REQUIRED));
                format.push_back(SOElement(sfPreviousTxnLgrSeq, SOE_REQUIRED));
                format.push_back(SOElement(sfPrevTxnLedgerHash, SOE_REQUIRED));
                format.push_back(SOElement(sfTxCheckHash, SOE_REQUIRED));
                format.push_back(SOElement(sfUsers, SOE_REQUIRED));
				format.push_back(SOElement(sfRules, SOE_REQUIRED));
            }
        };

        static FormatHolder holder;

        return holder.format;
    }

} // ripple
