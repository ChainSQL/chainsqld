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

#include <ripple/app/main/Application.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/ledger/TransactionMaster.h>
#include <ripple/json/json_reader.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/app/misc/Transaction.h>
#include <peersafe/app/table/TableSync.h>
#include <peersafe/app/table/TableStatusDBMySQL.h>
#include <peersafe/app/table/TableStatusDBSQLite.h>
#include <peersafe/protocol/TableDefines.h>
#include <peersafe/protocol/STEntry.h>
#include <peersafe/app/storage/TableStorageItem.h>
#include <peersafe/app/storage/TableStorage.h>
#include <peersafe/app/tx/ChainSqlTx.h>
#include <peersafe/app/util/TableSyncUtil.h>
#include <ripple/ledger/impl/Tuning.h>

namespace ripple {    
    
    TableStorageItem::TableStorageItem(Application& app, Config& cfg, beast::Journal journal)
        : app_(app)
        , journal_(journal)
        , cfg_(cfg)
    {       
		bExistInSyncTable_ = false;
		bDropped_ = false;
		lastTxTm_ = 0;
    }

    TableStorageItem::~TableStorageItem()
    {
    }

    void TableStorageItem::InitItem(AccountID account, std::string nameInDB, std::string tableName)
    {
        accountID_ = account;
        sTableNameInDB_ = nameInDB;
        sTableName_ = tableName;

        getTxStoreTrans();
    }

    void TableStorageItem::SetItemParam(LedgerIndex txnLedgerSeq, uint256 txnHash, LedgerIndex LedgerSeq, uint256 ledgerHash)
    {
        txnHash_ = txnHash;
        txnLedgerSeq_ = txnLedgerSeq;
        ledgerHash_ = ledgerHash;
        LedgerSeq_ = LedgerSeq;
    }

    void TableStorageItem::Put(STTx const& tx, uint256 txhash)
    {
		auto iter = std::find_if(txList_.begin(), txList_.end(),
			[txhash](txInfo& info)
		{
			return info.uTxHash == txhash;
		}
		);
		if (iter != txList_.end())
			return;

        txInfo txInfo_;
        txInfo_.accountID = accountID_;
        txInfo_.uTxHash = txhash;
        txInfo_.bCommit = false;
        if (tx.isFieldPresent(sfLastLedgerSequence))
            txInfo_.uTxLedgerVersion = tx.getFieldU32(sfLastLedgerSequence);  //uTxLedgerVersion			
		if (txInfo_.uTxLedgerVersion <= 0)
		{
			txInfo_.uTxLedgerVersion = app_.getLedgerMaster().getValidLedgerIndex() + MAX_GAP_LEDGERNUM_TXN_APPEARIN;
		}

        txList_.push_back(txInfo_);
    }

    void  TableStorageItem::prehandleTx(STTx const& tx)
    {
        if (txList_.size() <= 0)
        {
            app_.getTableSync().StopOneTable(accountID_, sTableNameInDB_, tx.getFieldU16(sfOpType) == T_CREATE);
        }
    }

    TER TableStorageItem::PutElem(ChainSqlTx& transactor, STTx const& tx, uint256 txhash)
    {
        std::pair<bool, std::string> ret = { true, "success" };
        auto  result = tefTABLE_STORAGEERROR;
     
        if (getTxStoreDBConn().GetDBConn() == NULL)
        {
            return tefTABLE_STORAGENORMALERROR;
        }

		prehandleTx(tx);

		auto op_type = tx.getFieldU16(sfOpType);
		if (!isNotNeedDisposeType((TableOpType)op_type))
		{
			auto resultPair = transactor.dispose(getTxStore(),tx);
			if (resultPair.first == tesSUCCESS)
			{
				JLOG(journal_.trace()) << "Dispose success";
			}
			else
			{
				ret = { false,"Dispose error" };
				if (resultPair.first != tefTABLE_TXDISPOSEERROR)
				{
					result = resultPair.first;
				}
				transactor.setExtraMsg(resultPair.second);
			}
		}
					
		if (tx.getFieldU16(sfOpType) == T_DROP)
		{
			bDropped_ = true;
			getTableStatusDB().UpdateSyncDB(to_string(accountID_), sTableNameInDB_, true, "");
		}
		else if (T_RENAME == op_type)
		{
			auto tables = tx.getFieldArray(sfTables);
			if (tables.size() > 0)
			{
				auto newTableName = strCopy(tables[0].getFieldVL(sfTableNewName));
				getTableStatusDB().RenameRecord(accountID_, sTableNameInDB_, newTableName);
			}
		}


        if (ret.first)
        {
            JLOG(journal_.trace()) << "Dispose success";
            if (!bExistInSyncTable_)
            {
                if (!getTableStatusDB().IsExist(accountID_, sTableNameInDB_))
                {
					auto chainId = TableSyncUtil::GetChainId(&transactor.view());
                    getTableStatusDB().InsertSnycDB(sTableName_, sTableNameInDB_, to_string(accountID_), LedgerSeq_, ledgerHash_, true, "",chainId);
                }
                bExistInSyncTable_ = true;
            }

            Put(tx, txhash);

            result = tesSUCCESS;
        }
      
        return result;
    }

    bool TableStorageItem::CheckLastLedgerSeq(LedgerIndex CurLedgerVersion)
    {
		auto ledger = app_.getLedgerMaster().getLedgerBySeq(CurLedgerVersion);
		if (!ledger) return false;

        auto iter = txList_.begin();
        for (; iter != txList_.end(); iter++)
        {
            if (iter->bCommit)   continue;
            
            if (iter->uTxLedgerVersion < CurLedgerVersion)
            {
				return false;
            }
			else if(iter->uTxLedgerVersion == CurLedgerVersion)
			{
				if (!ledger->txMap().hasItem(iter->uTxHash))
				{
					return false;
				}
			}
        }
        return true;
    }

    bool TableStorageItem::isHaveTx(uint256 txid)
    {
        auto iter(txList_.end());
        iter = std::find_if(txList_.begin(), txList_.end(),
            [txid](txInfo &info) {
            return info.uTxHash == txid;
        });

        if (iter != txList_.end())
            return true;
        else
            return false;
    }

    TableStorageItem::TableStorageDBFlag TableStorageItem::CheckSuccess(LedgerIndex validatedIndex)
    {     
        for (int index = LedgerSeq_ + 1; index <= validatedIndex; index++)
        {
            auto ledger = app_.getLedgerMaster().getLedgerBySeq(index);
            if (!ledger) continue;

            LedgerSeq_ = index;
            ledgerHash_ = app_.getLedgerMaster().getHashBySeq(index);

            auto const sleAccepted = ledger->read(keylet::table(accountID_));
            if (sleAccepted == NULL) continue;            
			
            const STEntry * pEntry = NULL;
            auto aTableEntries = sleAccepted->getFieldArray(sfTableEntries);
            auto retPair = TableSyncUtil::IsTableSLEChanged(aTableEntries, txnLedgerSeq_, sTableNameInDB_,true); 
			if (retPair.second == NULL)
			{
				if (retPair.first)
					continue;
				else if(bDropped_) //deleted;bug:RR-559
					return STORAGE_COMMIT;
			}				
			            
			pEntry = retPair.second;
			std::vector <uint256> aTx;
			for (auto const& item : ledger->txMap())
			{
				auto blob = SerialIter{ item.data(), item.size() }.getVL();
				STTx stTx(SerialIter{ blob.data(), blob.size() });				
                auto str = stTx.getFullText();

				auto vecTxs = app_.getMasterTransaction().getTxs(stTx, sTableNameInDB_);
				if (vecTxs.size() > 0)
				{
					aTx.push_back(stTx.getTransactionID());
				}
			}
			
            int iCount = 0;
            if (aTx.size() > 0) {
                txnHash_ = pEntry->getFieldH256(sfTxnLedgerHash);
                txnLedgerSeq_ = pEntry->getFieldU32(sfTxnLgrSeq);
            }
            for (auto tx : aTx)
            {
                iCount++;
                auto iter = std::find_if(txList_.begin(), txList_.end(),
                    [tx](txInfo &item) {
                    return item.uTxHash == tx;
                });
                
                if (iter == txList_.end())
                {
                    return STORAGE_ROLLBACK;
                }
                else
                {   
                    iter->bCommit = true;
                    auto initIter = std::find_if(txList_.begin(), txList_.end(),
                        [tx](txInfo &item) {
                        return !item.bCommit;
                    });

                    if (initIter == txList_.end()) //mean that each tx had set flag to commit
                    {
                        if (iCount < aTx.size())
                        {
                            LedgerSeq_ = index -1;
                            ledgerHash_ = app_.getLedgerMaster().getHashBySeq(LedgerSeq_);

                            txUpdateHash_ = tx;
                        }
                        else
                        {
                            txnHash_ = pEntry->getFieldH256(sfTxnLedgerHash);
                            txnLedgerSeq_ = pEntry->getFieldU32(sfTxnLgrSeq);
                        }
                        
						lastTxTm_ = ledger->info().closeTime.time_since_epoch().count();
                        return STORAGE_COMMIT;
                    }
                }
            }
        }
        return STORAGE_NONE;
    }

    bool TableStorageItem::rollBack()
    {
        {
            LockedSociSession sql_session = getTxStoreDBConn().GetDBConn()->checkoutDb();
            TxStoreTransaction &stTran = getTxStoreTrans();
            stTran.rollback();
            JLOG(journal_.warn()) << " TableStorageItem::rollBack " << sTableName_;
        }

        app_.getTableSync().ReStartOneTable(accountID_, sTableNameInDB_, sTableName_, false, false);
        return true;
    }


    bool TableStorageItem::commit()
    {
        {
            LockedSociSession sql_session = getTxStoreDBConn().GetDBConn()->checkoutDb();
            TxStoreTransaction &stTran = getTxStoreTrans();
			if(!bDropped_)
				getTableStatusDB().UpdateSyncDB(to_string(accountID_), sTableNameInDB_, to_string(txnHash_), to_string(txnLedgerSeq_), to_string(ledgerHash_), to_string(LedgerSeq_), txUpdateHash_.isNonZero()?to_string(txUpdateHash_) : "", to_string(lastTxTm_),"");
            stTran.commit();
        }

        app_.getTableSync().ReStartOneTable(accountID_, sTableNameInDB_, sTableName_, bDropped_, true);
        

		auto result = std::make_tuple("db_success", "", "");
		for (auto& info : txList_)
		{
			auto txn = app_.getMasterTransaction().fetch(info.uTxHash, true);
			if (txn) {
				app_.getOPs().pubTableTxs(accountID_, sTableName_, *txn->getSTransaction(), result, false);
			}
		}

        return true;
    }

    bool TableStorageItem::DoUpdateSyncDB(const std::string &Owner, const std::string &TableNameInDB, bool bDel,
        const std::string &PreviousCommit)
    {
        return getTableStatusDB().UpdateSyncDB(Owner, TableNameInDB, bDel, PreviousCommit);
    }

    TxStoreDBConn& TableStorageItem::getTxStoreDBConn()
    {
        if (conn_ == NULL)
        {
            conn_ = std::make_unique<TxStoreDBConn>(cfg_);
            if (conn_->GetDBConn() == NULL)
            {
                JLOG(journal_.error()) << "TableStorageItem::getTxStoreDBConn() return null";
            }
        }
        return *conn_;
    }

    TxStoreTransaction& TableStorageItem::getTxStoreTrans()
    {
        if (uTxStoreTrans_ == NULL)
        {
            uTxStoreTrans_ = std::make_unique<TxStoreTransaction>(&getTxStoreDBConn());
        }
        return *uTxStoreTrans_;
    }

    TxStore& TableStorageItem::getTxStore()
    {
        if (pObjTxStore_ == NULL)
        {
            auto& conn = getTxStoreDBConn();
            pObjTxStore_ = std::make_unique<TxStore>(conn.GetDBConn(), cfg_, journal_);
        }
        return *pObjTxStore_;
    }

    TableStatusDB& TableStorageItem::getTableStatusDB()
    {
        if (pObjTableStatusDB_ == NULL)
        {
			DatabaseCon::Setup setup = ripple::setup_SyncDatabaseCon(cfg_);
			std::pair<std::string, bool> result = setup.sync_db.find("type");
			if (result.first.compare("sqlite") == 0)
                pObjTableStatusDB_ = std::make_unique<TableStatusDBSQLite>(getTxStoreDBConn().GetDBConn(), &app_, journal_);                
            else
                pObjTableStatusDB_ = std::make_unique<TableStatusDBMySQL>(getTxStoreDBConn().GetDBConn(), &app_, journal_);
        }        

        return *pObjTableStatusDB_;
    }

    bool TableStorageItem::doJob(LedgerIndex CurLedgerVersion)
    {
        bool bRet = false;
        if (txList_.size() <= 0)
        {
            rollBack();
            return true;
        }
        bRet = CheckLastLedgerSeq(CurLedgerVersion);
        if (!bRet)
        {
            rollBack();
            return true;
        }
        auto eType = CheckSuccess(CurLedgerVersion);
        if (eType == STORAGE_ROLLBACK)
        {
            rollBack();
            return true;
        }
        else if (eType == STORAGE_COMMIT)
        {
            commit();
            return true;
        }
        else
        {
            return false;
        }
        
        return false;
    }
}
