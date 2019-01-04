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

#include <ripple/core/JobQueue.h>
#include <ripple/core/ConfigSections.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/basics/base_uint.h>
#include <ripple/app/ledger/TransactionMaster.h>
#include <ripple/json/json_reader.h>
#include <peersafe/protocol/TableDefines.h>
#include <peersafe/app/table/TableStatusDBMySQL.h>
#include <peersafe/app/table/TableStatusDBSQLite.h>
#include <peersafe/app/storage/TableStorage.h>
#include <peersafe/app/tx/ChainSqlTx.h>
#include <ripple/ledger/impl/Tuning.h>

namespace ripple {
    TableStorage::TableStorage(Application& app, Config& cfg, beast::Journal journal)
        : app_(app)
        , journal_(journal)
        , cfg_(cfg)
    {
		if (app.getTxStoreDBConn().GetDBConn() == nullptr ||
			app.getTxStoreDBConn().GetDBConn()->getSession().get_backend() == nullptr)
			m_IsHaveStorage = false;
		else
			m_IsHaveStorage = true;

		DatabaseCon::Setup setup = ripple::setup_SyncDatabaseCon(cfg_);
        std::pair<std::string, bool> result = setup.sync_db.find("first_storage");

        if (!result.second)
            m_IsStorageOn = false;
        else
        {
            if (result.first.compare("1") == 0)
                m_IsStorageOn = true;
            else
                m_IsStorageOn = false;
        }

		auto sync_section = cfg_.section(ConfigSection::autoSync());
		if (sync_section.values().size() > 0)
		{
			auto value = sync_section.values().at(0);
			bAutoLoadTable_ = atoi(value.c_str());
		}
		else
			bAutoLoadTable_ = false;

        bTableStorageThread_ = false;
    }

    TableStorage::~TableStorage()
    {
    }

    bool TableStorage::isStroageOn()
    {
        return m_IsStorageOn;
    }

    std::shared_ptr<TableStorageItem> TableStorage::GetItem(uint160 nameInDB)
    {
        std::lock_guard<std::mutex> lock(mutexMap_);
        auto it = m_map.find(nameInDB);
  
        if(it != m_map.end())
            return it->second;
        else return NULL;
    }

    void TableStorage::SetHaveSyncFlag(bool flag)
    {
        m_IsHaveStorage = flag;
    }

    TxStore& TableStorage::GetTxStore(uint160 nameInDB)
    {
        std::lock_guard<std::mutex> lock(mutexMap_);
        auto it = m_map.find(nameInDB);
        if (it == m_map.end())  return app_.getTxStore();
        else                    return it->second->getTxStore();
    }

    void TableStorage::TryTableStorage()
    {
        if (!m_IsHaveStorage) return;

        if (!bTableStorageThread_)
        {
            bTableStorageThread_ = true;
            app_.getJobQueue().addJob(jtTABLESTORAGE, "tableStorage", [this](Job&) { TableStorageThread(); });
        }
    }

    void TableStorage::GetTxParam(STTx const & tx, uint256 &txshash, uint160 &uTxDBName, std::string &sTableName, AccountID &accountID,uint32 &lastLedgerSequence)
    {
        txshash = tx.getTransactionID();
        
        auto const & sTxTables = tx.getFieldArray(sfTables);
        uTxDBName = sTxTables[0].getFieldH160(sfNameInDB);
        
        ripple::Blob nameBlob = sTxTables[0].getFieldVL(sfTableName);
        sTableName.assign(nameBlob.begin(), nameBlob.end());

        accountID = tx.getAccountID(sfAccount);
        if (tx.getTxnType() == ttSQLSTATEMENT)
            accountID = tx.getAccountID(sfOwner);

        if (tx.isFieldPresent(sfLastLedgerSequence))
            lastLedgerSequence = tx.getFieldU32(sfLastLedgerSequence);
    }

    TER TableStorage::InitItem(STTx const&tx, Transactor& transactor)
    {      
        if (!m_IsHaveStorage) return tesSUCCESS;

        if (!m_IsStorageOn) return tesSUCCESS;
        
        uint256 txhash;  //should get form every single tx
        uint160 uTxDBName;
        std::string sTableName;
        AccountID accountID;
        uint32 lastLedgerSequence;
		ChainSqlTx& chainSqlTx = dynamic_cast<ChainSqlTx&>(transactor);
		//skip if confidential
		if (app_.getLedgerMaster().isConfidential(tx))
			return tesSUCCESS;

		std::set<std::string>   nameInDBSet;
		auto vecTxs = app_.getMasterTransaction().getTxs(tx);
		for (auto& eachTx : vecTxs)
		{
			GetTxParam(eachTx, txhash, uTxDBName, sTableName, accountID, lastLedgerSequence);
			auto ret = TableStorageHandlePut(chainSqlTx, uTxDBName, accountID, sTableName, lastLedgerSequence, tx.getTransactionID(), eachTx);

			if (tesSUCCESS != ret)
				return ret;
		}

		return tesSUCCESS;
    }

	TER TableStorage::TableStorageHandlePut(ChainSqlTx& transactor,uint160 uTxDBName, AccountID accountID,std::string sTableName,uint32 lastLedgerSequence,uint256 txhash, STTx const & tx)
    {
        std::lock_guard<std::mutex> lock(mutexMap_);

        auto it = m_map.find(uTxDBName);
        if (it == m_map.end())
        {
            auto validIndex = app_.getLedgerMaster().getValidLedgerIndex();
            auto validLedger = app_.getLedgerMaster().getValidatedLedger();
            uint256 txnHash, ledgerHash, utxUpdatehash;
            LedgerIndex txnLedgerSeq, LedgerSeq;

            bool bRet = app_.getTableStatusDB().ReadSyncDB(to_string(uTxDBName), txnLedgerSeq, txnHash, LedgerSeq, ledgerHash, utxUpdatehash);
            if (bRet)
            {
                if (validIndex - LedgerSeq < MAX_GAP_NOW2VALID)  //catch up valid ledger
                {
                    auto pItem = std::make_shared<TableStorageItem>(app_, cfg_, journal_);
                    auto itRet = m_map.insert(make_pair(uTxDBName, pItem));
                    if (itRet.second)
                    {
                        pItem->InitItem(accountID, to_string(uTxDBName), sTableName);
                        if (utxUpdatehash.isNonZero())
                        {
                            LedgerSeq--;
                            ledgerHash--;
                        }
                        pItem->SetItemParam(txnLedgerSeq, txnHash, LedgerSeq, ledgerHash);

						return pItem->PutElem(transactor, tx, txhash);
                    }
                    else
                    {
                        return tesSUCCESS;
                    }
                }
                else
                {
                    return tefTABLE_STORAGENORMALERROR;
                }
            }
            else
            {
				if (!bAutoLoadTable_)
				{
					return tefTABLE_STORAGENORMALERROR;
				}
                auto const kOwner = keylet::account(accountID);
                auto const sleOwner = validLedger->read(kOwner);
                if (!sleOwner)  return  tefTABLE_STORAGENORMALERROR;

                auto const kTable = keylet::table(accountID);
                auto const sleTable = validLedger->read(kTable);

				if (!sleTable)
				{
					//In the case of first storage and no table sle, only T_CREATE OpType's tx can go on...bug:RR-559
					if ((tx.getTxnType() != ttTABLELISTSET) || (tx.getFieldU16(sfOpType) != T_CREATE) )
					{
						return tefTABLE_STORAGENORMALERROR;
					}
				}
				else
				{
					STArray tablentries = sleTable->getFieldArray(sfTableEntries);

					auto iter(tablentries.end());
					iter = std::find_if(tablentries.begin(), tablentries.end(),
						[uTxDBName](STObject const &item) {
						return item.getFieldH160(sfNameInDB) == uTxDBName;
					});
					if (iter != tablentries.end())
					{
						return tefTABLE_STORAGENORMALERROR;
					}
				}

				//
				{
					auto pItem = std::make_shared<TableStorageItem>(app_, cfg_, journal_);
					auto itRet = m_map.insert(make_pair(uTxDBName, pItem));
					if (itRet.second)
					{
						pItem->InitItem(accountID, to_string(uTxDBName), sTableName);
						pItem->SetItemParam(0, txhash, validLedger->info().seq, validLedger->info().hash);
						return pItem->PutElem(transactor, tx, txhash);
					}
					else
					{
						return tefTABLE_STORAGENORMALERROR;
					}
				}
            }
        }
        else
        {
			return it->second->PutElem(transactor, tx, txhash);
        }
    }
    
    void TableStorage::TableStorageThread()
    {
        auto validIndex = app_.getLedgerMaster().getValidLedgerIndex();
        auto mapTmp = m_map;

        for(auto item : mapTmp)
        {
            uint160 uTxDBName; //how to get value ?
            {
                std::lock_guard<std::mutex> lock(mutexMap_);
                bool bRet = item.second->doJob(validIndex);
                if (bRet)
                {
                    m_map.erase(item.first);
                }
            }
        }
        bTableStorageThread_ = false;
    }
}
  
