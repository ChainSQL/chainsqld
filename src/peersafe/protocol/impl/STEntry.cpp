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


#include <ripple/protocol/HashPrefix.h>
#include <ripple/basics/contract.h>
#include <ripple/basics/Log.h>
#include <ripple/json/to_string.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/json/json_reader.h>
#include <ripple/protocol/jss.h>
#include <peersafe/protocol/STEntry.h>

namespace ripple {

    STEntry::STEntry()
        : STObject(getFormat(), sfEntry)
    {

    }

	void
    STEntry::initOperationRule(STObject& entry, Blob operationRule)
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
        entry.setFieldObject(sfRules, obj_rules);
	}

	std::string
        STEntry::getOperationRule(STObject const& entry, TableOpType opType)
	{
		std::string ret;
		if (!isSqlStatementOpType(opType) && opType != R_GET)
			return ret;

		if (!entry.isFieldPresent(sfRules))
		{
			return ret;
		}
        auto obj = entry.getFieldObject(sfRules);
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

    SOTemplate const& STEntry::getFormat()
    {
        struct FormatHolder
        {
            SOTemplate format
            {
				{ sfTableName,			soeREQUIRED },
                { sfNameInDB,			soeREQUIRED },
				{ sfCreateLgrSeq,		soeREQUIRED },
				{ sfCreatedLedgerHash,	soeREQUIRED },
				{ sfCreatedTxnHash,		soeREQUIRED },
				{ sfTxnLgrSeq,			soeREQUIRED },
				{ sfTxnLedgerHash,		soeREQUIRED },
				{ sfPreviousTxnLgrSeq,	soeREQUIRED },
				{ sfPrevTxnLedgerHash,	soeREQUIRED },
				{ sfTxCheckHash,		soeREQUIRED },
				{ sfUsers,				soeOPTIONAL },
				{ sfRules,				soeREQUIRED }
			};
        };

        static FormatHolder holder;

        return holder.format;
    }

} // ripple
