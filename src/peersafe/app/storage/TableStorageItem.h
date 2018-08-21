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

#ifndef RIPPLE_APP_TABLE_TABLESTORAGE_ITEM_H_INCLUDED
#define RIPPLE_APP_TABLE_TABLESTORAGE_ITEM_H_INCLUDED

#include <peersafe/app/sql/TxStore.h>
namespace ripple {
class ChainSqlTx;

class TableStorageItem
{
    enum TableStorageDBFlag
    {
        STORAGE_NONE,
        STORAGE_ROLLBACK,
        STORAGE_COMMIT
    };

    typedef struct txInfo_
    {
        AccountID                                                    accountID;
        uint256                                                      uTxHash;
        LedgerIndex                                                  uTxLedgerVersion;        
        bool                                                         bCommit;

        txInfo_()
        {
            uTxLedgerVersion = 0;
            bCommit = false;
        }

    }txInfo;

public:    
    TableStorageItem(Application& app, Config& cfg, beast::Journal journal);
    void InitItem(AccountID account ,std::string nameInDB, std::string tableName);
    void SetItemParam(LedgerIndex txnLedgerSeq, uint256 txnHash, LedgerIndex LedgerSeq, uint256 ledgerHash);
    virtual ~TableStorageItem();
    
    TER PutElem(ChainSqlTx& transactor, STTx const& tx, uint256 txhash);
    bool doJob(LedgerIndex CurLedgerVersion);

    TxStore& getTxStore();
    bool isHaveTx(uint256 txid);
    bool DoUpdateSyncDB(const std::string &Owner, const std::string &TableNameInDB, bool bDel,
        const std::string &PreviousCommit);
private: 
    bool rollBack();
    bool commit();
    void Put(STTx const& tx, uint256 txhash);
    bool CheckLastLedgerSeq(LedgerIndex CurLedgerVersion);
    void prehandleTx(STTx const& tx);
    TableStorageItem::TableStorageDBFlag CheckSuccess(LedgerIndex validatedIndex);
   
    TxStoreDBConn& getTxStoreDBConn();
    TxStoreTransaction& getTxStoreTrans();
   
    TableStatusDB& getTableStatusDB();

private:
    std::list<txInfo>                                                           txList_;
    std::shared_ptr <TxStoreDBConn>                                             dbconn_;
    std::unique_ptr <TxStoreTransaction>                                        uTxStoreTrans_;
    std::string                                                                 sTableNameInDB_;
    std::string                                                                 sTableName_;
    AccountID                                                                   accountID_;

    std::unique_ptr <TxStoreDBConn>                                             conn_;
    std::unique_ptr <TxStore>                                                   pObjTxStore_;
    std::unique_ptr <TableStatusDB>                                             pObjTableStatusDB_;

	bool                                                                        bExistInSyncTable_;
	bool                                                                        bDropped_; 

    uint256                                                                    txnHash_;
    LedgerIndex                                                                txnLedgerSeq_;
    uint256                                                                    ledgerHash_;
    LedgerIndex                                                                LedgerSeq_;
    uint256                                                                    txUpdateHash_;
	uint32                                                                     lastTxTm_;

    Application&                                                                app_;
    beast::Journal                                                              journal_;
    Config&                                                                     cfg_;
};
}
#endif

