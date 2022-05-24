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


#include <ripple/ledger/View.h>
#include <ripple/app/paths/RippleCalc.h>
#include <ripple/basics/Log.h>
#include <ripple/core/Config.h>
#include <ripple/protocol/st.h>
#include <ripple/protocol/TxFlags.h>
#include <ripple/protocol/jss.h>
#include <ripple/protocol/digest.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/basics/Slice.h>
#include <ripple/json/json_reader.h>
#include <ripple/protocol/Feature.h>
#include <peersafe/schema/Schema.h>
#include <peersafe/protocol/STEntry.h>
#include <peersafe/protocol/TableDefines.h>
#include <peersafe/app/tx/TableListSet.h>
#include <peersafe/app/tx/impl/Tuning.h>
#include <peersafe/app/tx/OperationRule.h>
#include <peersafe/rpc/TableUtils.h>

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

    uint32_t
        getFinalFlag(STTx const& tx,bool bHasSameUser,STObject const& user)
    {
        uint32_t uAdd = 0, uCancel = 0;
        auto optype = tx.getFieldU16(sfOpType);
        getGrantFlag(tx, uAdd, uCancel);
        uint32_t finalFlag = 0;
        if (bHasSameUser)
        {
            auto newFlags = user.getFieldU32(sfFlags);
            if (optype == T_ASSIGN)
            {
                if (tx.isFieldPresent(sfFlags))
                {
                    newFlags = newFlags |
                        tx.getFieldU32(sfFlags);  // add auth of this user
                }
            }
            else if (optype == T_CANCELASSIGN)
            {
                if (tx.isFieldPresent(sfFlags))
                {
                    newFlags = newFlags &
                        (~tx.getFieldU32(sfFlags));  // cancel auth of this user
                }
            }
            else  // grant optype
            {
                newFlags |= uAdd;
                newFlags &= ~uCancel;
            }
            finalFlag = newFlags;
        }
        else
        {
            if (optype == T_ASSIGN)
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
        }
        return finalFlag;
    }

	NotTEC
        TableListSet::preflightHandler(const STTx & tx, Schema& app)
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
			auto tables = tx.getFieldArray(sfTables);
			if (!tables[0].isFieldPresent(sfTableName) ||
				tables[0].getFieldVL(sfTableName).size() == 0 ||
				tables[0].getFieldVL(sfTableName).size() > 64)
			{
				return temINVALID;
			}
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
            if (!tables[0].isFieldPresent(sfTableNewName) ||
				tables[0].getFieldVL(sfTableNewName).size() == 0 ||
				tables[0].getFieldVL(sfTableNewName).size() > 64)
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

	NotTEC
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

    void
    setTableGrant(const STTx& tx, STObject& obj,uint32_t finalFlag,bool bGrantEnabled)
    {
        if (bGrantEnabled)
        {
            auto tables = tx.getFieldArray(sfTables);
            obj.setFieldH160(sfNameInDB, tables[0].getFieldH160(sfNameInDB));
        }        

        if (tx.getFieldU16(sfOpType) == T_GRANT)  // preflight assure sfUser and sfFlags
                                        // exist together or not exist at all
        {
            obj.setAccountID(sfUser, tx.getAccountID(sfUser));
            obj.setFieldU32(sfFlags, finalFlag);
        }
        else if (tx.getFieldU16(sfOpType) == T_CREATE)
        {
            obj.setAccountID(sfUser, tx.getAccountID(sfAccount));
            obj.setFieldU32(sfFlags, lsfAll);
        }

        // cipher encrypted by user's publickey
        if (tx.isFieldPresent(sfToken) && tx.getFieldVL(sfToken).size() > 0)
        {
            obj.setFieldVL(sfToken, tx.getFieldVL(sfToken));
        }
    }

    TER
    addSleToDir(
        AccountID const& accountId,
        ApplyView& view,
        std::shared_ptr<SLE> pSle,
        beast::Journal viewJ)
    {
        if (pSle != nullptr)
        {
            auto result = dirAdd(
                view,
                keylet::ownerDir(accountId),
                pSle->key(),
                false,
                describeOwnerDir(accountId),
                viewJ);

            if (!result)
                return tecDIR_FULL;
            (*pSle)[sfOwnerNode] = *result;
            view.insert(pSle);
        }
        return tesSUCCESS;
    }

    void
    getRelatedGrantSles(
        AccountID const& accountId,
        ApplyView& view,
        STObject* pEntry,
        std::vector<std::shared_ptr<SLE>>& vecDel)
    {
        auto nameInDB = pEntry->getFieldH160(sfNameInDB);

        // Need non-const sle-pointer,so not use foreachItem
        auto ownerDir = keylet::ownerDir(accountId);
        auto pos = ownerDir;
        for (;;)
        {
            auto sle = view.read(pos);
            if (!sle)
                return;

            for (auto const& key : sle->getFieldV256(sfIndexes))
            {
                auto sleNode = view.peek(keylet::child(key));
                if (!sle)
                    return;
                if (sleNode->getType() == ltTABLEGRANT &&
                    sleNode->getFieldH160(sfNameInDB) == nameInDB)
                {
                    vecDel.push_back(sleNode);
                }
            }
            auto const next = sle->getFieldU64(sfIndexNext);
            if (!next)
                break;
            pos = keylet::page(ownerDir, next);
        }
    }

    void
        TableListSet::prepareTableEntry(const STTx &tx, ApplyView& view,STObject * pEntry)  //preflight assure sfTables must exist
    {
        STEntry obj_tableEntry;

        ripple::Blob tableName;      //store Name
        auto tables = tx.getFieldArray(sfTables);
        tableName = tables[0].getFieldVL(sfTableName);

        uint160 nameInDB = tables[0].getFieldH160(sfNameInDB);
        ripple::uint256 createdLedgerHash = view.info().hash; createdLedgerHash--; 
        uint256 hashNew = sha512Half(makeSlice(strCopy(tx.getFieldVL(sfRaw))));

        if (!view.rules().enabled(featureTableGrant))
        {
            STArray users;//store Users
            STObject obj_user(sfUser);

            setTableGrant(tx, obj_user,0,false);

            users.push_back(obj_user);
            pEntry->setFieldArray(sfUsers, users);
        }
       
        // if (tableName == NUll ) then set to null ,no exception
        pEntry->setFieldVL(sfTableName, tableName);  
        pEntry->setFieldH160(sfNameInDB, nameInDB);
        pEntry->setFieldU32(sfCreateLgrSeq, view.info().seq - 1);
        pEntry->setFieldH256(sfCreatedLedgerHash, createdLedgerHash);
        pEntry->setFieldH256(sfCreatedTxnHash, tx.getRealTxID());
        pEntry->setFieldU32(sfTxnLgrSeq, view.info().seq);
        pEntry->setFieldH256(sfTxnLedgerHash, view.info().hash);
        pEntry->setFieldU32(sfPreviousTxnLgrSeq, 0);
        pEntry->setFieldH256(sfPrevTxnLedgerHash, beast::zero);
        pEntry->setFieldH256(sfTxCheckHash, hashNew);
		if (tx.isFieldPresent(sfOperationRule))
			STEntry::initOperationRule(*pEntry,tx.getFieldVL(sfOperationRule));
    }

    TER
        TableListSet::preclaimHandler(ReadView const& view, const STTx & tx, Schema& app)
    {
		auto j = app.journal("preclaimHandler");
        JLOG(j.trace()) <<  "preclaimHandler begin";

        AccountID sourceID(tx.getAccountID(sfAccount));
        TER ret = tesSUCCESS;

        auto optype = tx.getFieldU16(sfOpType);
        if (!isTableListSetOpType((TableOpType)optype))     
            return temBAD_OPTYPE;

        auto tables = tx.getFieldArray(sfTables);
        Blob vTableNameStr = tables[0].getFieldVL(sfTableName);

        auto tup = getTableEntry(view, tx);
        auto pEntry = std::get<1>(tup);
        auto tableEntries = std::get<2>(tup);
        if (!pEntry && optype >= T_DROP)
            return tefTABLE_NOTEXIST;

        if (optype == T_REPORT)
            return tesSUCCESS;

        switch (optype)
        {
        case T_CREATE:
        {
            bool bSleChangeEnabled = view.rules().enabled(featureTableSleChange);
            auto const sleAccount = view.read(keylet::account(sourceID));
			if (!bSleChangeEnabled &&
                tableEntries != nullptr &&
                (*tableEntries).size() >= ACCOUNT_OWN_TABLE_COUNT)
				return tefTABLE_COUNTFULL;

            if (pEntry != NULL)                
                ret = tefTABLE_EXISTANDNOTDEL;

			
			// Check reserve and funds availability
			{
                 bool isContract = false;
                 if (sleAccount->isFieldPresent(sfContractCode))
                    isContract = true;
		
				auto const reserve = view.fees().accountReserve(
					(*sleAccount)[sfOwnerCount] + 1, isContract);
				STAmount priorBalance = STAmount((*sleAccount)[sfBalance]).zxc();
				if (priorBalance < reserve)
					return tefINSU_RESERVE_TABLE;
			}
            break;
        }
        case T_DROP:
        {
            if (pEntry == NULL)
                ret = tefTABLE_NOTEXIST;
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
            auto tup = getTableEntry(view,sourceID, strCopy(vTableNameNewStr));
            auto pEntryNew = std::get<1>(tup);
            if (pEntry != NULL)
            {
                if (pEntryNew != NULL)                   
                    ret = tefTABLE_EXISTANDNOTDEL;
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
                                        ret = tefBAD_AUTH_NO;
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
                                    // if confidential,for assign operation,tx must contains token field
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

            if (pEntry == NULL)
            {
                return tefTABLE_NOTEXIST;
            }

            //if (!pEntry->isFieldPresent(sfUsers))
            //{
            //    return tefTABLE_STATEERROR;
            //}


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

            if (pEntry->isFieldPresent(sfUsers))
            {
                auto& users = pEntry->getFieldArray(sfUsers);
                bool isSameUser = false;
                for (auto& user : users)  // check if there same user
                {
                    auto userID = user.getAccountID(sfUser);
                    if (userID == addUserID)
                    {
                        isSameUser = true;
                        break;
                    }
                }

                if (!isSameUser && users.size() - 1 >= TABLE_GRANT_COUNT)
                    return tefTABLE_GRANTFULL;

                {
                    // for the first time to assign to one user
                    if (users.size() > 0 && users[0].isFieldPresent(sfToken))
                    {
                        // cannot grant for 'everyone' if table is confidential
                        if (addUserID == noAccount())
                            return temINVALID;
                        if (!tx.isFieldPresent(sfToken) ||
                            tx.getFieldVL(sfToken).size() == 0)
                            return temINVALID;
                    }
                }
            }
            else
            {
                auto keylet = keylet::tablegrant(
                    sourceID,
                    strCopy(tables[0].getFieldVL(sfTableName)),
                    sourceID);
                auto pSle = view.read(keylet);
                if (pSle && pSle->isFieldPresent(sfToken) &&
                    pSle->getFieldVL(sfToken).size() != 0)
                {
                    if (addUserID == noAccount())
                        return temINVALID;
                    if (!tx.isFieldPresent(sfToken) ||
                        tx.getFieldVL(sfToken).size() == 0)
                        return temINVALID;
                }
            }

            ret = tesSUCCESS;
            break;
        }
        case T_RECREATE:
		case T_ADD_FIELDS:
		case T_DELETE_FIELDS:
		case T_MODIFY_FIELDS:
		case T_CREATE_INDEX:
		case T_DELETE_INDEX:
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
        return ret;
    }


    TER
        TableListSet::preclaim(PreclaimContext const& ctx)  //just do some pre job
    {
        if (ctx.tx.getFieldU16(sfOpType) == T_CREATE)
        {
            auto checkRet = checkAuthority(ctx, ctx.tx.getAccountID(sfAccount), lsfCreateTableAuth);
            if (checkRet != tesSUCCESS)
                return checkRet;
        }

        auto tmpret = preclaimHandler(ctx.view, ctx.tx, ctx.app);
        if (!isTesSuccess(tmpret))
            return tmpret;

        return ChainSqlTx::preclaim(ctx);
    }

    TER
        TableListSet::applyHandler(ApplyView& view,const STTx & tx, Schema& app)
    {
		auto accountId = tx.getAccountID(sfAccount);
        auto optype = tx.getFieldU16(sfOpType);

        TER terResult = tesSUCCESS;

        // Open a ledger for editing.
        auto viewJ = app.journal("View");
        auto tables = tx.getFieldArray(sfTables);
        Blob vTableNameStr = tables[0].getFieldVL(sfTableName);
        auto sTableName = strCopy(vTableNameStr);

        STObject* pEntry = nullptr;
        STArray* tableEntries = nullptr;
        std::shared_ptr<SLE> tableSleExist = nullptr;
        std::tie(tableSleExist, pEntry, tableEntries) = getTableEntryVar(view, tx);
        
		if (pEntry)
		{
			auto& table = *pEntry;
			if (table.getFieldU32(sfTxnLgrSeq) != view.info().seq || table.getFieldH256(sfTxnLedgerHash) != view.info().hash)
			{
				table.setFieldU32(sfPreviousTxnLgrSeq, table.getFieldU32(sfTxnLgrSeq));
				table.setFieldH256(sfPrevTxnLedgerHash, table.getFieldH256(sfTxnLedgerHash));
				table.setFieldU32(sfTxnLgrSeq, view.info().seq);
				table.setFieldH256(sfTxnLedgerHash, view.info().hash);
			}
		}

        //add the new tx to the node
        switch (optype)
        {
        case T_CREATE:
		{
            bool bTableGrantEnabled = view.rules().enabled(featureTableGrant);
            bool bSleChangeEnabled = view.rules().enabled(featureTableSleChange);
            std::shared_ptr<SLE> tableSle = nullptr;
            std::shared_ptr<SLE> tableGrantSle = nullptr;
            if (!bSleChangeEnabled)
            {
                STEntry tableEntry;
                prepareTableEntry(tx, view, &tableEntry);

                if (!tableEntries)
                {
                    Keylet key = keylet::tablelist(accountId);
                    tableSle = std::make_shared<SLE>(ltTABLELIST, key.key);

                    STArray tableEntries;
                    tableEntries.push_back(tableEntry);

                    tableSle->setFieldArray(sfTableEntries, tableEntries);
                }
                else
                {
                    tableEntries->push_back(tableEntry);
                    view.update(tableSleExist);
                }
            }
            else
            {
                Keylet key = keylet::table(accountId, sTableName);
                tableSle = std::make_shared<SLE>(ltTABLE, key.key);
                STObject tableEntry(STEntry::getFormat(), sfTableEntry);
                prepareTableEntry(tx, view, &tableEntry);
                tableSle->setFieldObject(sfTableEntry, tableEntry);
            }
            if (bTableGrantEnabled)
            {
                Keylet key = keylet::tablegrant(accountId, sTableName, accountId);
                tableGrantSle = std::make_shared<SLE>(ltTABLEGRANT, key.key);
                setTableGrant(tx,*tableGrantSle,0,true);
                terResult = addSleToDir(accountId, view, tableGrantSle, viewJ);
                if (terResult != tesSUCCESS)
                    return terResult;
            }

            terResult = addSleToDir(accountId, view, tableSle, viewJ);
            if (terResult != tesSUCCESS)
                return terResult;

			//add owner count
			auto const sleAccount = view.peek(keylet::account(accountId));
			adjustOwnerCount(view, sleAccount, 1, viewJ);
            if (tableGrantSle != nullptr)
                adjustOwnerCount(view, sleAccount, 1, viewJ);
            
			break;
		}
        case T_DROP:
        {
            bool bDropped = false;
            if (tableEntries && pEntry)
            {
                auto iter = std::find_if(
                    tableEntries->begin(),
                    tableEntries->end(),
                    [vTableNameStr](STObject const& item) {
                        if (!item.isFieldPresent(sfTableName))
                            return false;

                        return item.getFieldVL(sfTableName) == vTableNameStr;
                    });
                if (iter != tableEntries->end())
                {
                    bDropped = true;
                    tableEntries->erase(iter);
                    view.update(tableSleExist);
                }
            }
            else if (tableSleExist && pEntry)
            {
                Keylet key = keylet::table(accountId, sTableName);
                if (!view.dirRemove(
                        keylet::ownerDir(accountId),
                        (*tableSleExist)[sfOwnerNode],
                        tableSleExist->key(),
                        true))
                {
                    return tefBAD_LEDGER;
                }
                view.erase(tableSleExist);
                bDropped = true;
            }

			if (bDropped)
            {
				auto const sleAccount = view.peek(keylet::account(accountId));
				adjustOwnerCount(view, sleAccount, -1, viewJ);

                //Remove related TableGrant sles and directory node
                if (!pEntry->isFieldPresent(sfUsers))
                {
                    auto nameInDB = pEntry->getFieldH160(sfNameInDB);
                    std::vector<std::shared_ptr<SLE>> vecDel;

                    //Need non-const sle-pointer,so not use foreachItem
                    getRelatedGrantSles(accountId, view, pEntry, vecDel);
                    if (!vecDel.empty())
                    {
                        auto ownerDir = keylet::ownerDir(accountId);
                        for (auto sleNode : vecDel)
                        {
                            if (!view.dirRemove(
                                    ownerDir,
                                    (*sleNode)[sfOwnerNode],
                                    sleNode->key(),
                                    true))
                            {
                                return tefBAD_LEDGER;
                            }
                            view.erase(sleNode);
                        }
                        adjustOwnerCount(
                            view, sleAccount, -1 * vecDel.size(), viewJ);
                    }
                }                
			}

            break;
        }
        case  T_RENAME:
        {
            if (pEntry)
            {
                 ripple::Blob tableNewName;
                if (tables[0].isFieldPresent(sfTableNewName))
                {
                    tableNewName = tables[0].getFieldVL(sfTableNewName);
                }
                pEntry->setFieldVL(sfTableName, tableNewName);
                if (tableEntries)
                {
                    view.update(tableSleExist);
                }
                else if (tableSleExist && !tableEntries)
                {
                    //add new tablesle
                    std::shared_ptr<SLE> tableSle = nullptr; 
                    Keylet key = keylet::table(accountId, strCopy(tableNewName));
                    tableSle = std::make_shared<SLE>(ltTABLE, key.key);
                    tableSle->setFieldObject(sfTableEntry, *pEntry);
            
                    terResult = addSleToDir(accountId, view, tableSle, viewJ);
                    if (terResult != tesSUCCESS)
                        return terResult;
                    //delete old tablesle
                    // Keylet oldKey = keylet::table(accountId, sTableName);
                    if (!view.dirRemove(
                            keylet::ownerDir(accountId),
                            (*tableSleExist)[sfOwnerNode],
                            tableSleExist->key(),
                            true))
                    {
                        return tefBAD_LEDGER;
                    }
                    view.erase(tableSleExist);
                }

                if (pEntry->isFieldPresent(sfUsers))
                    break;
                //Delete and re-add TableGrant sles
                auto nameInDB = pEntry->getFieldH160(sfNameInDB);
                std::vector<std::shared_ptr<SLE>> vecDel;

                // Need non-const sle-pointer,so not use foreachItem
                getRelatedGrantSles(accountId, view, pEntry, vecDel);
                if (!vecDel.empty())
                {
                    auto sTableNewName = strCopy(tableNewName);
                    auto ownerDir = keylet::ownerDir(accountId);
                    for (auto sleNode : vecDel)
                    {
                        Keylet key = keylet::tablegrant(
                            accountId,
                            sTableNewName,
                            sleNode->getAccountID(sfUser));
                        auto tableGrantSle =
                            std::make_shared<SLE>(ltTABLEGRANT, key.key);
                        tableGrantSle->setAccountID(sfUser, sleNode->getAccountID(sfUser));
                        tableGrantSle->setFieldU32(sfFlags, sleNode->getFieldU32(sfFlags));
                        tableGrantSle->setFieldH160(sfNameInDB, nameInDB);
                        if (sleNode->isFieldPresent(sfToken))
                            tableGrantSle->setFieldVL(sfToken, sleNode->getFieldVL(sfToken));
                        auto ret = addSleToDir(accountId, view, tableGrantSle, viewJ);
                        if (ret != tesSUCCESS)
                            return ret;

                        if (!view.dirRemove(
                                ownerDir,
                                (*sleNode)[sfOwnerNode],
                                sleNode->key(),
                                true))
                        {
                            return tefBAD_LEDGER;
                        }
                        view.erase(sleNode);
                    }
                }
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
            
            auto addUserID = tx.getAccountID(sfUser);
	        uint32_t finalFlag = lsfNone;
            if (pEntry->isFieldPresent(sfUsers))
            {
                auto& users = pEntry->peekFieldArray(sfUsers);
                bool isSameUser = false;
                for (auto & user : users)  //check if there same user
                {
                    auto userID = user.getAccountID(sfUser);
                    if (userID == addUserID)
                    {
                        isSameUser = true;
                        finalFlag = getFinalFlag(tx, true, user);
                        user.setFieldU32(sfFlags, finalFlag);
						break;
                    }
                }
                if (!isSameUser)  //mean that there no same user
                {
                    // Optype must be t_grant(preclaim assure that),just add a new user
                    STObject obj_user(sfUser);
                    finalFlag = getFinalFlag(tx, false, obj_user);
                    setTableGrant(tx, obj_user, finalFlag, false);
                    users.push_back(obj_user);
				}
                view.update(tableSleExist);
            }
            else
            {
                auto keylet = keylet::tablegrant(
                    accountId,
                    strCopy(tables[0].getFieldVL(sfTableName)),
                    addUserID);
                auto pSle = view.peek(keylet);
                if (pSle)
                {
                    finalFlag = getFinalFlag(tx, true, *pSle);
                    pSle->setFieldU32(sfFlags, finalFlag);
                    view.update(pSle);
                }
                else
                {
                    auto tableGrantSle = std::make_shared<SLE>(ltTABLEGRANT, keylet.key);
                    finalFlag = getFinalFlag(tx, false, *tableGrantSle);
                    setTableGrant(tx, *tableGrantSle, finalFlag, true);

                    terResult = addSleToDir(accountId, view, tableGrantSle, viewJ);
                    if (terResult != tesSUCCESS)
                        return terResult;
                    auto const sleAccount = view.peek(keylet::account(accountId));
                    adjustOwnerCount(view, sleAccount, 1, viewJ);
                }
            }
            break;
        }
        case T_RECREATE:
        {
            LedgerIndex createLgrSeq = view.info().seq; createLgrSeq--;
            ripple::uint256 createdLedgerHash = view.info().hash; createdLedgerHash--;
            ripple::uint256 createdTxnHash = tx.getTransactionID();
            UpdateTableSle(pEntry, createLgrSeq, createdLedgerHash, createdTxnHash);
            view.update(tableSleExist);
            break;
        }
        case T_DELETE_FIELDS:
        {
            updateOperationRules(pEntry, tx);
            view.update(tableSleExist);
            break;
        }
        case T_ADD_FIELDS:
        case T_MODIFY_FIELDS:
        case T_CREATE_INDEX:
        case T_DELETE_INDEX:
            view.update(tableSleExist);
            break;
        default:
            break;
        }

        return terResult;
    }

    void TableListSet::updateOperationRules(STObject* pEntry, const STTx& tx)
    {
        auto opType = tx.getFieldU16(sfOpType);
        if (opType != (int)T_DELETE_FIELDS)
            return;

        auto sRaw = strCopy(tx.getFieldVL(sfRaw));
        Json::Value jsonRaw;
        if (Json::Reader().parse(sRaw, jsonRaw) == false)
            return;
        assert(jsonRaw.isArray());

        STObject operationRuleObject(sfRules);
        for (Json::UInt i = 0; i < jsonRaw.size(); i++) 
        {
            Json::Value& item = jsonRaw[i];
            if (item.isMember(jss::field) == false)
                continue;
            Json::Value& field = item[jss::field];
            assert(field.isString());
            auto fieldName = field.toStyledString();
            if (fieldName.empty())
                continue;

            Json::Value insertionRule;
            if (updateOperationRuleForInserting(
                fieldName,
                pEntry,
                insertionRule))
            {
                std::string rule = insertionRule.toStyledString();
                operationRuleObject.setFieldVL(sfInsertRule, strCopy(rule));
            }

            Json::Value updateRule;
            if (updateOperationRuleForUpdating(
                fieldName,
                pEntry,
                updateRule))
            {
                std::string rule = updateRule.toStyledString();
                operationRuleObject.setFieldVL(sfUpdateRule, strCopy(rule));
            }

            Json::Value deleteRule;
            if (updateOperationRuleForDeletingOrGet(
                fieldName,
                pEntry,
                (TableOpType)R_DELETE,
                deleteRule))
            {
                std::string rule = deleteRule.toStyledString();
                operationRuleObject.setFieldVL(sfDeleteRule, strCopy(rule));
            }

            Json::Value getRule;
            if (updateOperationRuleForDeletingOrGet(
                fieldName,
                pEntry,
                (TableOpType)R_GET,
                getRule))
            {
                std::string rule = getRule.toStyledString();
                operationRuleObject.setFieldVL(sfGetRule, strCopy(rule));
            }
        }
        pEntry->setFieldObject(sfRules, operationRuleObject);
    }

    bool TableListSet::updateOperationRuleForInserting(
        const std::string& fieldName, 
        STObject* pEntry, 
        Json::Value& jsonRule)
    {
        bool ret = false;
        auto insertOperationRule = STEntry::getOperationRule(*pEntry, (TableOpType)R_INSERT);
        if (insertOperationRule.empty())
            return ret;

        if (Json::Reader().parse(insertOperationRule, jsonRule) == false)
            return ret;

        if (jsonRule.isMember(jss::Condition))
        {
            Json::Value& condition = jsonRule[jss::Condition];
            auto members = condition.getMemberNames();
            auto found = std::find_if(members.begin(), members.end(), [&](const std::string& key) {
                return key == fieldName;
            });

            if (found != members.end())
            {
                condition.removeMember(fieldName);
                ret = true;
            }

            if (condition.getMemberNames().size() == 0)
            {
                jsonRule.removeMember(jss::Condition);
            }
        }

        if (jsonRule.isMember(jss::Count))
        {
            Json::Value& count = jsonRule[jss::Count];
            if (count[jss::AccountField].toStyledString() != fieldName)
                return ret;
            jsonRule.removeMember(jss::Count);
            ret = true;
        }

        return ret;
    }

    bool updateCondtion(Json::Value& condition, const std::string& fieldName)
    {
        if (condition.isMember("$or") || condition.isMember("$and"))
        {
            assert(condition.isArray());
            Json::Value::UInt size = condition.size();
            for (auto i = 0; i < size; i++) {
                if (updateCondtion(condition[i], fieldName))
                    return true;
                else
                    continue;
            }
        }
        else
        {
            auto members = condition.getMemberNames();
            auto found = std::find_if(members.begin(), members.end(), [&](const std::string& key) {
                return key == fieldName;
            });

            if (found != members.end())
            {
                condition.removeMember(fieldName);
                return true;
            }
        }
        return false;
    }

    bool TableListSet::updateOperationRuleForUpdating(
        const std::string& fieldName, 
        STObject* pEntry, 
        Json::Value& jsonRule) {

        bool ret = false;
        auto updateOperationRule = STEntry::getOperationRule(*pEntry, (TableOpType)R_UPDATE);
        if (updateOperationRule.empty())
            return ret;

        if (Json::Reader().parse(updateOperationRule, jsonRule) == false)
            return ret;

        if (jsonRule.isMember(jss::Condition))
        {
            Json::Value& condition = jsonRule[jss::Condition];
            if (updateCondtion(condition, fieldName))
                ret = true;
        }

        if (jsonRule.isMember(jss::Fields))
        {
            Json::Value& fields = jsonRule[jss::Fields];
            Json::Value newFields(Json::arrayValue);
            assert(fields.isArray());
            Json::Value::UInt size = fields.size();
            for (auto i = 0; i < size; i++)
            {
                Json::Value& value = fields[i];
                assert(value.isString());
                if (value.asString() != fieldName) {
                    newFields.append(value);
                }
                else {
                    ret = true;
                }
            }
            jsonRule[jss::Fields] = newFields;
        }

        return ret;
    }

    bool TableListSet::updateOperationRuleForDeletingOrGet(
        const std::string& fieldName, 
        STObject* pEntry, 
        const int opType, 
        Json::Value& jsonRule) {

        bool ret = false;
        auto operationRule = STEntry::getOperationRule(*pEntry, (TableOpType)opType);
        if (operationRule.empty())
            return ret;

        if (Json::Reader().parse(operationRule, jsonRule) == false)
            return ret;

        if (jsonRule.isMember(jss::Condition))
        {
            Json::Value& condition = jsonRule[jss::Condition];
            if (updateCondtion(condition, fieldName))
                ret = true;
        }

        return ret;
    }

	std::pair<TER, std::string> TableListSet::dispose(TxStore& txStore, const STTx& tx)
	{
		TER tmpRet = OperationRule::dealWithTableListSetRule(ctx_, tx);
		if (!isTesSuccess(tmpRet))
			return std::make_pair(tmpRet, "deal with operation-rule error");
		return ChainSqlTx::dispose(txStore, tx);
	}

    void
        TableListSet::UpdateTableSle(
            STObject* pEntry,
            LedgerIndex createLgrSeq,
            uint256 createdLedgerHash,
            uint256 createdTxnHash,
            LedgerIndex previousTxnLgrSeq,
            uint256 previousTxnLgrHash)
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
		//Deal with operation-rule,if first-storage have called 'dispose',not need here
		TER tmpRet = tesSUCCESS;
		if (canDispose(ctx_))
		{
			tmpRet = OperationRule::dealWithTableListSetRule(ctx_, ctx_.tx);
			if (!isTesSuccess(tmpRet))
				return tmpRet;
		}

		// apply to sle
		tmpRet = applyHandler(ctx_.view(), ctx_.tx, ctx_.app);
        if (!isTesSuccess(tmpRet))
            return tmpRet;

        return ChainSqlTx::doApply();

    }  // ripple
}
