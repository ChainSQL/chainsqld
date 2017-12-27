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
#include <ripple/ledger/View.h>
#include <ripple/app/paths/RippleCalc.h>
#include <ripple/basics/Log.h>
#include <ripple/core/Config.h>
#include <ripple/protocol/st.h>
#include <ripple/protocol/TxFlags.h>
#include <ripple/protocol/JsonFields.h>
#include <ripple/protocol/digest.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/basics/Slice.h>
#include <ripple/app/main/Application.h>
#include <ripple/json/json_reader.h>
#include <peersafe/protocol/STEntry.h>
#include <peersafe/protocol/TableDefines.h>
#include <peersafe/app/tx/TableListSet.h>
#include <peersafe/app/storage/TableStorage.h>

namespace ripple {

	bool getGrantFlag(STTx const& tx, std::uint32_t &uAdd, std::uint32_t &uCancel)
	{
		if (T_GRANT != tx.getFieldU16(sfOpType))  return false;
		if (!tx.isFieldPresent(sfRaw))            return false;
		
		Json::Value jsonRaw;
		auto sRaw = strCopy(tx.getFieldVL(sfRaw));		
		if (Json::Reader().parse(sRaw, jsonRaw))
		{
			if (!jsonRaw.isArray()) return false;
			for (auto &jsonFlag : jsonRaw)
			{
				for (Json::Value::iterator it = jsonFlag.begin(); it != jsonFlag.end(); it++)
				{
					std::string sKey = it.key().asString();
					TableRoleFlags opType = getOptypeFromString(sKey);
					if (jsonFlag[sKey].asBool())   uAdd |= opType;
					else                        uCancel |= opType;
				}
			}
			
			auto uTotal = uAdd | uCancel;
			if (uTotal == lsfNone)  return false;

			return true;
		}
		return false;
	}
    ZXCAmount
        TableListSet::calculateMaxSpend(STTx const& tx)
    {
        if (tx.isFieldPresent(sfSendMax))
        {
            auto const& sendMax = tx[sfSendMax];
            return sendMax.native() ? sendMax.zxc() : beast::zero;
        }
        /* If there's no sfSendMax in ZXC, and the sfAmount isn't
        in ZXC, then the transaction can not send ZXC. */
        auto const& saDstAmount = tx.getFieldAmount(sfAmount);
        return saDstAmount.native() ? saDstAmount.zxc() : beast::zero;
    }

    TER
        TableListSet::preflightHandler(const STTx & tx, Application& app)
    {
		auto j = app.journal("preflightHandler");

		auto optype = tx.getFieldU16(sfOpType);
        if (tx.isFieldPresent(sfRaw))  //check sfTables
        {
            auto raw = strCopy(tx.getFieldVL(sfRaw));
            if (raw.size() == 0)
            {
                JLOG(j.trace()) <<
                    "sfRaw is not invalid";
                return temINVALID;
            }
        }
		else
		{
			if (optype == T_GRANT)
			{
				JLOG(j.trace()) << "Malformed transaction: " <<
					"no raw field in grant operation.";
				return temBAD_RAW;
			}
		}
        
        if (optype != T_REPORT)        
        {
            if (!tx.isFieldPresent(sfTables))  //check sfTables
            {
                JLOG(j.trace()) <<
                    "sfTables is not invalid";
                return temINVALID;
            }
            else
            {
                auto tables = tx.getFieldArray(sfTables);
                if (tables.size() == 0)
                {
                    JLOG(j.trace()) <<
                        "sfTables is not filled";
                    return temINVALID;
                }
                else
                {
                    for (auto table : tables)
                    {
                        if (!table.isFieldPresent(sfTableName))
                        {
                            JLOG(j.trace()) <<
                                "sfTableName is not present";
                            return temINVALID;
                        }
                        auto tablename = table.getFieldVL(sfTableName);
                        if (tablename.size() == 0)
                        {
                            JLOG(j.trace()) <<
                                "sfTableName is not filled";
                            return temINVALID;
                        }
                        if (!table.isFieldPresent(sfNameInDB))
                        {
                            JLOG(j.trace()) <<
                                "sfNameInDB is not present";
                            return temINVALID;
                        }
                        auto nameindb = table.getFieldH160(sfNameInDB);
                        if (nameindb.isZero() == 1)
                        {
                            JLOG(j.trace()) <<
                                "sfNameInDB is not filled";
                            return temINVALID;
                        }
                    }
                }
            }
        }

		if (optype == T_CREATE)
		{
			if (tx.isFieldPresent(sfOperationRule))
			{
				Json::Value jsonRule;
				auto sOperationRule = strCopy(tx.getFieldVL(sfOperationRule));
				if (Json::Reader().parse(sOperationRule, jsonRule))
				{
					if (!jsonRule.isMember(jss::Insert) && 
						!jsonRule.isMember(jss::Update) && 
						!jsonRule.isMember(jss::Delete) && 
						!jsonRule.isMember(jss::Get))
					{
						JLOG(j.trace()) <<
							"sfOperationRule is in bad format";
						return temBAD_OPERATIONRULE;
					}
				}
			}
		}

        if (optype == T_RENAME)  //check sfTableNewName
        {
            auto tables = tx.getFieldArray(sfTables);
            if (!tables[0].isFieldPresent(sfTableNewName) || (tables[0].getFieldVL(sfTableNewName).size() == 0))
            {
                JLOG(j.trace()) <<
                    "rename opreator but sfTableNewName is not filled";
                return temINVALID;
            }
        }
		
		if (tx.isFieldPresent(sfFlags) && (optype == T_ASSIGN || optype == T_CANCELASSIGN))  //check sfFlags
        {
            auto flags = tx.getFieldU32(sfFlags);

            if (tx.isFieldPresent(sfUser))
            {
                if (!((flags & lsfSelect) || (flags & lsfInsert) || (flags & lsfUpdate) || (flags & lsfDelete) || (flags &  lsfExecute)))
                {
                    JLOG(j.trace()) <<
                        "bad auth";
                    return tefNO_AUTH_REQUIRED;
                }
            }
        }
        return tesSUCCESS;
    }

    TER
        TableListSet::preflight(PreflightContext const& ctx)
    {
        auto const ret = preflight1(ctx);
        if (!isTesSuccess(ret))
            return ret;
       
        auto tmpret = preflightHandler(ctx.tx, ctx.app);
        if (!isTesSuccess(tmpret))
            return tmpret;

        tmpret = ChainSqlTx::preflight(ctx);
        if (!isTesSuccess(tmpret))
            return tmpret;

        return preflight2(ctx);
    }


    STObject
        TableListSet::generateTableEntry(const STTx &tx, ApplyView& view)  //preflight assure sfTables must exist
    {
        STEntry obj_tableEntry;

        ripple::Blob tableName;      //store Name
        auto tables = tx.getFieldArray(sfTables);
        tableName = tables[0].getFieldVL(sfTableName);

        uint160 nameInDB = tables[0].getFieldH160(sfNameInDB);
        uint8 isTableDeleted = 0;  //is not deleted

        uint32 ledgerSequence = view.info().seq;
        uint32 createledgerSequence = view.info().seq - 1;  
        ripple::uint256 txnLedgerHash = view.info().hash;
        ripple::uint256 createdLedgerHash = view.info().hash; createdLedgerHash--;       
        ripple::uint256 createdtxnHash = tx.getTransactionID(); 
        ripple::uint256 prevTxnLedgerHash; //default null

        uint256 hashNew = sha512Half(makeSlice(strCopy(tx.getFieldVL(sfRaw))));

        STArray users;//store Users
        STObject obj_user(sfUser);

        if (tx.isFieldPresent(sfUser))//preflight assure sfUser and sfFlags exist together or not exist at all
        {
            obj_user.setAccountID(sfUser, tx.getAccountID(sfUser));
            obj_user.setFieldU32(sfFlags, tx.getFieldU32(sfFlags));
        }
        else
        {
            obj_user.setAccountID(sfUser, tx.getAccountID(sfAccount));
            obj_user.setFieldU32(sfFlags, lsfAll);
            //cipher encrypted by user's publickey
            if (tx.isFieldPresent(sfToken) && tx.getFieldVL(sfToken).size() > 0)
            {
                obj_user.setFieldVL(sfToken, tx.getFieldVL(sfToken));
            }
        }

        users.push_back(obj_user);

        obj_tableEntry.init(tableName, nameInDB,isTableDeleted, createledgerSequence, createdLedgerHash, createdtxnHash,ledgerSequence ,txnLedgerHash,0,prevTxnLedgerHash,hashNew, users);
		if (tx.isFieldPresent(sfOperationRule))
			obj_tableEntry.initOperationRule(tx.getFieldVL(sfOperationRule));
	
        return std::move(obj_tableEntry);
    }

    TER
        TableListSet::preclaimHandler(ReadView const& view, const STTx & tx, Application& app)
    {
		auto j = app.journal("preclaimHandler");
        JLOG(j.trace()) <<  "preclaimHandler begin";

        AccountID sourceID(tx.getAccountID(sfAccount));
        TER ret = tesSUCCESS;

        auto optype = tx.getFieldU16(sfOpType);
        if (!isTableListSetOpType((TableOpType)optype))     
            return temBAD_OPTYPE;

        auto key = keylet::table(sourceID);
        auto const tablesle = view.read(key);

        if (optype != T_REPORT)
        {
            auto tables = tx.getFieldArray(sfTables);
            Blob vTableNameStr = tables[0].getFieldVL(sfTableName);

            if (tablesle)  //table exist
            {
                STArray tablentries = tablesle->getFieldArray(sfTableEntries);
                STEntry const *pEntry = getTableEntry(tablentries, vTableNameStr);

                switch (optype)
                {
                case T_CREATE:
                {
                    if (pEntry != NULL)                ret = tefTABLE_EXISTANDNOTDEL;
                    else                               ret = tesSUCCESS;

					auto const sleAccount = view.read(keylet::account(sourceID));
					// Check reserve and funds availability
					{
						auto const reserve = view.fees().accountReserve(
							(*sleAccount)[sfOwnerCount] + 1);
						STAmount priorBalance = STAmount((*sleAccount)[sfBalance]).zxc();
						if (priorBalance < reserve)
							return tefINSUFFICIENT_RESERVE;
					}
                    break;
                }
                case T_DROP:
                {
                    if (pEntry != NULL)                ret = tesSUCCESS;
                    else                               ret = tefTABLE_NOTEXIST;

                    break;
                }
                case T_RENAME:
                {
                    if (vTableNameStr == tables[0].getFieldVL(sfTableNewName))
                    {
                        ret = tefTABLE_SAMENAME;
                        break;
                    }
                    Blob vTableNameNewStr = tables[0].getFieldVL(sfTableNewName);
                    STEntry const *pEntryNew = getTableEntry(tablentries, vTableNameNewStr);

                    if (pEntry != NULL)
                    {
                        if (pEntryNew != NULL)                    ret = tefTABLE_EXISTANDNOTDEL;
                        else   					                  ret = tesSUCCESS;
                    }
                    else
                    {
                        ret = tefTABLE_NOTEXIST;
                    }
                    break;
                }
                case T_ASSIGN:
                case T_CANCELASSIGN:
                {
                    //if confidential,for assign operation,tx must contains token field
                    STEntry const *pEntry = getTableEntry(tablentries, vTableNameStr);

                    if (pEntry != NULL)
                    {
                        if (pEntry->isFieldPresent(sfUsers))
                        {
                            auto& users = pEntry->getFieldArray(sfUsers);

                            if (tx.isFieldPresent(sfUser))
                            {
                                auto  addUserID = tx.getAccountID(sfUser);

                                //check user is effective?
                                auto key = keylet::account(addUserID);
                                if (!view.exists(key))
                                {
                                    return tefBAD_AUTH;
                                };

                                bool isSameUser = false;
                                for (auto & user : users)  //check if there same user
                                {
                                    auto userID = user.getAccountID(sfUser);
                                    if (userID == addUserID)
                                    {
                                        isSameUser = true;
                                        auto userFlags = user.getFieldU32(sfFlags);
                                        if (optype == T_CANCELASSIGN)
                                        {
                                            if (!(userFlags & tx.getFieldU32(sfFlags))) //optype == T_CANCELASSIGN but cancel auth not exist
                                            {
                                                ret = tefBAD_AUTH_NO;
                                            }
                                            else
                                                ret = tesSUCCESS;
                                        }
                                        else
                                        {
                                            if (~userFlags & tx.getFieldU32(sfFlags))  //optype == T_ASSIGN and same TableFlags
                                            {
                                                ret = tesSUCCESS;
                                            }
                                            else
                                                ret = tefBAD_AUTH_EXIST;
                                        }
                                    }
                                }
                                if (!isSameUser)  //mean that there no same user
                                {
                                    if (optype == T_CANCELASSIGN)
                                        ret = tefBAD_AUTH_NO;
                                    else
                                    {
                                        //for the first time to assign to one user
                                        if (users.size() > 0 && users[0].isFieldPresent(sfToken))
                                        {
											//cannot grant for 'everyone' if table is confidential
											if (addUserID == noAccount())
												return temINVALID;
                                            if (!tx.isFieldPresent(sfToken) || tx.getFieldVL(sfToken).size() == 0)
                                                return temINVALID;
                                        }
                                        ret = tesSUCCESS;
                                    }
                                }
                            }
                            else
                                ret = tefBAD_USER;
                        }
                        else
                        {
                            if (optype == T_ASSIGN)               ret = tesSUCCESS;
                            else                                  ret = tefBAD_USER;
                        }
                    }
                    else
                        ret = tefTABLE_NOTEXIST;
                    break;
                }
                case T_GRANT:
                {
					if (tx.isCrossChainUpload())
					{
						return tesSUCCESS;
					}
                    STEntry const *pEntry = getTableEntry(tablentries, vTableNameStr);
                    if (pEntry == NULL)
                    {
                        return tefTABLE_NOTEXIST;
                    }

                    if (!pEntry->isFieldPresent(sfUsers))
                    {
                        return tefTABLE_STATEERROR;
                    }

                    auto& users = pEntry->getFieldArray(sfUsers);

                    if (!tx.isFieldPresent(sfUser))
                    {
                        return tefBAD_USER;
                    }

                    auto  addUserID = tx.getAccountID(sfUser);
                    //check user is effective?
                    auto key = keylet::account(addUserID);
                    if (addUserID != noAccount() && !view.exists(key))
                    {
                        return tefBAD_USER;
                    };

                    uint32_t uAdd = 0, uCancel = 0;
                    bool bRet = getGrantFlag(tx, uAdd, uCancel);
                    if (!bRet)
                    {
                        return temMALFORMED;
                    }

                    /*
                    bool isSameUser = false;
                    for (auto & user : users)  //check if there same user
                    {
                        auto userID = user.getAccountID(sfUser);
                        if (userID == addUserID)
                        {
                            isSameUser = true;
                            auto userFlags = user.getFieldU32(sfFlags);


                            if ((!(userFlags & uCancel)) && !(~userFlags & uAdd))
                            {
                                if (uCancel)		return tefBAD_AUTH_NO;
                                if (uAdd)		    return tefBAD_AUTH_EXIST;
                            }
                        }
                    }
                    */
                    //if (!isSameUser)  //mean that there no same user
                    //{
                    //    if (uCancel != lsfNone && uAdd == lsfNone)
                    //        return tefBAD_AUTH_NO;
                    //    else
                        {
                            //for the first time to assign to one user
                            if (users.size() > 0 && users[0].isFieldPresent(sfToken))
                            {
								//cannot grant for 'everyone' if table is confidential
								if (addUserID == noAccount())
									return temINVALID;
                                if (!tx.isFieldPresent(sfToken) || tx.getFieldVL(sfToken).size() == 0)
                                    return temINVALID;
                            }
                            ret = tesSUCCESS;
                        }
                    //}

                    break;
                }
                case T_RECREATE:
                {
                    if (pEntry != NULL)                ret = tesSUCCESS;
                    else                               ret = tefTABLE_NOTEXIST;

                    break;
                }
                default:
                {
                    ret = temBAD_OPTYPE;
                    break;
                }
                }
            }
            else         //table not exist
            {
                if (optype >= T_DROP)
                    ret = tefTABLE_NOTEXIST;
            }
        }

        return ret;
    }


    TER
        TableListSet::preclaim(PreclaimContext const& ctx)  //just do some pre job
    {
        auto tmpret = preclaimHandler(ctx.view, ctx.tx, ctx.app);
        if (!isTesSuccess(tmpret))
            return tmpret;

        return ChainSqlTx::preclaim(ctx);
    }

    TER
        TableListSet::applyHandler(ApplyView& view,const STTx & tx, Application& app)
    {
		auto accountId = tx.getAccountID(sfAccount);
        auto optype = tx.getFieldU16(sfOpType);

        TER terResult = tesSUCCESS;

        // Open a ledger for editing.
        auto id = keylet::table(accountId);
        auto const tablesle = view.peek(id);
		auto viewJ = app.journal("View");
        if (!tablesle)
		{
			//create first table
            auto const tablesle = std::make_shared<SLE>(
                ltTABLELIST, id.key);
            
            std::uint64_t hint;
            auto result = dirAdd(view, hint, keylet::ownerDir(accountId),
				tablesle->key(), describeOwnerDir(accountId), viewJ);

            if (result.first == tesSUCCESS)
            {
                tablesle->setFieldU64(sfOwnerNode, hint);
            }

			STArray tablentries;
            STObject obj = generateTableEntry(tx, view);
            tablentries.push_back(obj);

            tablesle->setFieldArray(sfTableEntries, tablentries);

			//add owner count
			auto const sleAccount = view.peek(keylet::account(accountId));
			adjustOwnerCount(view, sleAccount, 1, viewJ);

			view.insert(tablesle);
        }
        else
        {
            auto &aTableEntries = tablesle->peekFieldArray(sfTableEntries);

            Blob sTxTableName;

            auto const & sTxTables = tx.getFieldArray(sfTables);
            sTxTableName = sTxTables[0].getFieldVL(sfTableName);

            for (auto & table : aTableEntries)
            {
                auto str = table.getFieldVL(sfTableName);
                if (table.getFieldVL(sfTableName) == sTxTableName && table.getFieldU8(sfDeleted) != 1)
                {
                    if (table.getFieldU32(sfTxnLgrSeq) != view.info().seq || table.getFieldH256(sfTxnLedgerHash) != view.info().hash)
                    {
                        table.setFieldU32(sfPreviousTxnLgrSeq, table.getFieldU32(sfTxnLgrSeq));
                        table.setFieldH256(sfPrevTxnLedgerHash, table.getFieldH256(sfTxnLedgerHash));
                        table.setFieldU32(sfTxnLgrSeq, view.info().seq);
                        table.setFieldH256(sfTxnLedgerHash, view.info().hash);
                    }
                }
            }

            auto tables = tx.getFieldArray(sfTables);
            Blob vTableNameStr = tables.begin()->getFieldVL(sfTableName);
            //add the new tx to the node
            switch (optype)
            {
            case T_CREATE:
			{
				//create table (not first one)
				aTableEntries.push_back(generateTableEntry(tx, view));

				//add owner count
				auto const sleAccount = view.peek(keylet::account(accountId));
				adjustOwnerCount(view, sleAccount, 1, viewJ);
				break;
			}
            case T_DROP:
            {
				STEntry  *pEntry = getTableEntry(aTableEntries, vTableNameStr);
                if (pEntry)
                {
					assert(pEntry->getFieldU8(sfDeleted) == 0);
					pEntry->setFieldU8(sfDeleted, 1);

					auto const sleAccount = view.peek(keylet::account(accountId));
					adjustOwnerCount(view, sleAccount, -1, viewJ);
                }
                break;
            }
            case  T_RENAME:
            {
				STEntry  *pEntry = getTableEntry(aTableEntries, vTableNameStr);
                if (pEntry)
                {
                    ripple::Blob tableNewName;
                    if (sTxTables[0].isFieldPresent(sfTableNewName))
                    {
                        tableNewName = sTxTables[0].getFieldVL(sfTableNewName);
                    }
                    pEntry->setFieldVL(sfTableName, tableNewName);
                }
                break;
            }
            case T_ASSIGN:
            case T_CANCELASSIGN:
			case T_GRANT:			
            {
				if (tx.isCrossChainUpload())
				{
					return tesSUCCESS;
				}
				uint32_t uAdd = 0, uCancel = 0;
				getGrantFlag(tx, uAdd, uCancel);
				STEntry  *pEntry = getTableEntry(aTableEntries, vTableNameStr);
                {
                    if (pEntry->isFieldPresent(sfUsers))
                    {
                        auto& users = pEntry->peekFieldArray(sfUsers);

                        if (tx.isFieldPresent(sfUser))
                        {
                            auto  addUserID = tx.getAccountID(sfUser);

                            bool isSameUser = false;
							uint32_t finalFlag = lsfNone;
                            for (auto & user : users)  //check if there same user
                            {
                                auto userID = user.getAccountID(sfUser);
                                if (userID == addUserID)
                                {
                                    isSameUser = true;

									auto newFlags = user.getFieldU32(sfFlags);
                                    if (optype == T_ASSIGN)
                                    {                                        
                                        if (tx.isFieldPresent(sfFlags))
                                        {
                                            newFlags = newFlags | tx.getFieldU32(sfFlags); //add auth of this user                                            
                                        }
                                    }
									else if (optype == T_CANCELASSIGN)
									{										
										if (tx.isFieldPresent(sfFlags))
										{
											newFlags = newFlags & (~tx.getFieldU32(sfFlags));   //cancel auth of this user											
										}
									}
									else   //grant optype
									{
										newFlags |= uAdd;
										newFlags &= ~uCancel;
									}
									finalFlag = newFlags;
									user.setFieldU32(sfFlags, newFlags);
									break;
                                }
                            }
                            if (!isSameUser)  //mean that there no same user
                            {
                                // optype must be Auth(preclaim assure that),just add a new user
                                STObject obj_user(sfUser);
                                if (tx.isFieldPresent(sfUser))
                                    obj_user.setAccountID(sfUser, tx.getAccountID(sfUser));
								if(optype == T_ASSIGN)
								{
									if (tx.isFieldPresent(sfFlags))
									{
										finalFlag = tx.getFieldU32(sfFlags);										
									}
								}
                                else if (optype == T_CANCELASSIGN)
                                {
                                    finalFlag = lsfNone;
                                }
								else
								{
									finalFlag = (uAdd & ~uCancel);									
								}
                                obj_user.setFieldU32(sfFlags, finalFlag);

								if (tx.isFieldPresent(sfToken))
									obj_user.setFieldVL(sfToken,tx.getFieldVL(sfToken));
                                users.push_back(obj_user);
							}
                        }
                    }
                }
                break;
            }
            case T_RECREATE:
            {
                STEntry  *pEntry = getTableEntry(aTableEntries, vTableNameStr);
                LedgerIndex createLgrSeq = view.info().seq; createLgrSeq--;
                ripple::uint256 createdLedgerHash = view.info().hash; createdLedgerHash--;
                ripple::uint256 createdTxnHash = tx.getTransactionID();
                UpdateTableSle(pEntry, createLgrSeq, createdLedgerHash, createdTxnHash);
                break;
            }
            default:
                break;
            }
           
			view.update(tablesle);
        }
        return terResult;
    }

	std::pair<TER, std::string> TableListSet::dispose(TxStore& txStore, const STTx& tx)
	{
		TER tmpRet = dealWithOperationRule(ctx_.view(), tx, ctx_.app);
		if (!isTesSuccess(tmpRet))
			return std::make_pair(tmpRet, "deal with operation-rule error");
		return ChainSqlTx::dispose(txStore, tx);
	}

	TER TableListSet::dealWithOperationRule(ApplyView& view, const STTx & tx, Application& app)
	{
		if (tx.getFieldU16(sfOpType) != T_CREATE)
			return tesSUCCESS;
		if (tx.isFieldPresent(sfOperationRule))
		{
			if (tx.isFieldPresent(sfToken))
				return temBAD_RULEANDTOKEN;

			auto j = app.journal("dealWithOperationRule");

			Json::Value jsonRule;
			auto sOperationRule = strCopy(tx.getFieldVL(sfOperationRule));
			if (Json::Reader().parse(sOperationRule, jsonRule))
			{
				std::string sRaw = strCopy(tx.getFieldVL(sfRaw));
				// will not dispose if raw is encrypted
				Json::Value jsonRaw;
				if (!Json::Reader().parse(sRaw, jsonRaw))
					return temBAD_RAW;

				std::vector<std::string> vecFields;
				for (Json::UInt idx = 0; idx < jsonRaw.size(); idx++)
				{
					auto& v = jsonRaw[idx];
					if (v.isMember(jss::field))
					{
						vecFields.push_back(v[jss::field].asString());
					}
				}

				bool bContainCountLimit = false;
				std::string sAccountCondition = "";
				//make sure all fields in sfOperationRule corresponding to fields in sfRaw
				std::string accountField;
				if (jsonRule.isMember(jss::Insert))
				{
					if (jsonRule[jss::Insert].isMember(jss::Count))
					{
						const auto& jsonCount = jsonRule[jss::Insert][jss::Count];
						if (!jsonCount.isMember(jss::AccountField) || !jsonCount.isMember(jss::CountLimit))
							return temBAD_OPERATIONRULE;
						accountField = jsonCount[jss::AccountField].asString();
						if (std::find(vecFields.begin(), vecFields.end(), accountField) == vecFields.end())
							return temBAD_OPERATIONRULE;
						if (!jsonCount[jss::CountLimit].isInt() && !jsonCount[jss::CountLimit].isUInt())
							return temBAD_OPERATIONRULE;
						if (jsonCount[jss::CountLimit].asInt() <= 0)
							return temBAD_OPERATIONRULE;
						bContainCountLimit = true;
					}
					auto members = jsonRule[jss::Insert].getMemberNames();
					for (int i = 0; i < members.size(); i++)
					{
						if (members[i] != jss::Condition && members[i] != jss::Count)
							return temBAD_OPERATIONRULE;
					}
					if (jsonRule[jss::Insert].isMember(jss::Condition))
					{
						Json::Value& condition = jsonRule[jss::Insert][jss::Condition];
						if (!condition.isObject())
							return temBAD_OPERATIONRULE;
						std::vector<std::string> members = condition.getMemberNames();
						// retrieve members in object
						for (size_t i = 0; i < members.size(); i++) {
							if (std::find(vecFields.begin(), vecFields.end(), members[i]) == vecFields.end())
								return temBAD_OPERATIONRULE;
							//make sure account field right
							if (members[i] == accountField) {
								auto sAccount = condition[accountField].asString();
								if (sAccount != "$account" && ripple::parseBase58<AccountID>(sAccount) == boost::none)
									return temBAD_OPERATIONRULE;
								sAccountCondition = "\"" + accountField + "\" : \"" + sAccount + "\"";
							}
						}
					}
				}
				if (jsonRule.isMember(jss::Update))
				{
					auto members = jsonRule[jss::Update].getMemberNames();
					for (int i = 0; i < members.size(); i++)
					{
						if (members[i] != jss::Condition && members[i] != jss::Fields)
							return temBAD_OPERATIONRULE;
					}
					if (jsonRule[jss::Update].isMember(jss::Fields))
					{
						Json::Value jsonFields = jsonRule[jss::Update][jss::Fields];
						for (Json::UInt idx = 0; idx < jsonFields.size(); idx++)
						{
							if (std::find(vecFields.begin(), vecFields.end(), jsonFields[idx].asString()) == vecFields.end())
								return temBAD_OPERATIONRULE;
							if (jsonFields[idx].asString() == accountField)
								return temBAD_OPERATIONRULE;
						}
					}
					else if (!accountField.empty()){
						return temBAD_UPDATERULE;
					}
					if (jsonRule[jss::Update].isMember(jss::Condition))
					{
						Json::Value& condition = jsonRule[jss::Update][jss::Condition];
						if (!condition.isObject())
							return temBAD_OPERATIONRULE;
					}
				}else if (!accountField.empty()) {
					return temBAD_UPDATERULE;
				}

				if (jsonRule.isMember(jss::Delete))
				{
					auto members = jsonRule[jss::Delete].getMemberNames();
					if (members.size() != 1 || members[0] != jss::Condition)
						return temBAD_DELETERULE;

					if (!jsonRule[jss::Delete][jss::Condition].isObject())
						return temBAD_DELETERULE;
					//if insert count is limited,then delete must define only the 'AccountField' account can delet
					if (bContainCountLimit)
					{
						if (jsonRule[jss::Delete][jss::Condition].isMember("$or"))
							return temBAD_DELETERULE;
						else if (jsonRule[jss::Delete][jss::Condition].isMember("$and"))
						{
							bool bFound = false;
							Json::Value jsonFields = jsonRule[jss::Delete][jss::Condition]["$and"];
							if(!jsonFields.isArray())
								return temBAD_DELETERULE;
							for (Json::UInt idx = 0; idx < jsonFields.size(); idx++)
							{
								std::string sDelete = jsonFields[idx].toStyledString();
								if (sAccountCondition != "") {
									if (sDelete.find(sAccountCondition) != std::string::npos)
									{
										bFound = true;
										break;
									}
								}									
								else
								{
									if (jsonFields[idx].isMember(accountField))
									{
										std::string strAccount = jsonFields[idx][accountField].asString();
										if (strAccount == "$account" || strAccount == to_string(tx.getAccountID(sfAccount)))
										{
											bFound = true;
											break;
										}
									}
								}
							}
							if (!bFound)
								return temBAD_DELETERULE;
						}
						else
						{
							std::string sDelete = jsonRule[jss::Delete][jss::Condition].toStyledString();
							if (sDelete.find(sAccountCondition) == std::string::npos)
								return temBAD_DELETERULE;
						}
					}
				}
				else if (bContainCountLimit)
				{
					return temBAD_DELETERULE;
				}

				if (jsonRule.isMember(jss::Get))
				{
					auto members = jsonRule[jss::Get].getMemberNames();
					if (members.size() != 1 || members[0] != jss::Condition)
						return temBAD_OPERATIONRULE;
					if (!jsonRule[jss::Get].isMember(jss::Condition))
						return temBAD_OPERATIONRULE;
					if (!jsonRule[jss::Get][jss::Condition].isObject())
						return temBAD_OPERATIONRULE;
				}
			}
			else
				return temBAD_OPERATIONRULE;
		}

		return tesSUCCESS;
	}

	TER TableListSet::preApply()
	{
		auto tables = ctx_.tx.getFieldArray(sfTables);
		uint160 nameInDB = tables[0].getFieldH160(sfNameInDB);

		auto item = ctx_.app.getTableStorage().GetItem(nameInDB);

		//canDispose is false if first_storage is true
		bool canDispose = true;
		if (item != NULL && item->isHaveTx(ctx_.tx.getTransactionID()))
			canDispose = false;

		if (canDispose)
			return dealWithOperationRule(ctx_.view(), ctx_.tx, ctx_.app);
		else
			return tesSUCCESS;
	}

    void TableListSet::UpdateTableSle(STEntry *pEntry, LedgerIndex createLgrSeq, uint256 createdLedgerHash, uint256 createdTxnHash, LedgerIndex previousTxnLgrSeq, uint256 previousTxnLgrHash)
    {
        pEntry->setFieldU32(sfCreateLgrSeq, createLgrSeq);
        pEntry->setFieldH256(sfCreatedLedgerHash, createdLedgerHash);
        pEntry->setFieldH256(sfCreatedTxnHash, createdTxnHash);
		pEntry->setFieldU32(sfPreviousTxnLgrSeq, previousTxnLgrSeq);
		pEntry->setFieldH256(sfPrevTxnLedgerHash, previousTxnLgrHash);
    }

    TER
        TableListSet::doApply()
    {
		auto tmpRet = preApply();
		if (!isTesSuccess(tmpRet))
			return tmpRet;

		tmpRet = applyHandler(ctx_.view(), ctx_.tx, ctx_.app);
        if (!isTesSuccess(tmpRet))
            return tmpRet;

        return ChainSqlTx::doApply();

    }  // ripple
}
