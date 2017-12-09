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

#ifndef RIPPLE_APP_TABLE_TABLESTORAGE_H_INCLUDED
#define RIPPLE_APP_TABLE_TABLESTORAGE_H_INCLUDED

#include <peersafe/app/storage/TableStorageItem.h>
#include <peersafe/protocol/TableDefines.h>


namespace ripple {

class ChainSqlTx;
class Transactor;
class TableStorage
{
public:
    TableStorage(Application& app, Config& cfg, beast::Journal journal);
    virtual ~TableStorage();

    std::shared_ptr<TableStorageItem> GetItem(uint160 nameInDB);

    void SetHaveSyncFlag(bool flag);
    void TryTableStorage();

    TER InitItem(STTx const&tx,Transactor& transactor);
    void TableStorageThread();

    TxStore& GetTxStore(uint160 nameInDB);
    bool isStroageOn();
private:
    void GetTxParam(STTx const & tx, uint256 &txshash, uint160 &uTxDBName, std::string &sTableName, AccountID &accountID, uint32 &lastLedgerSequence);
    TER TableStorageHandlePut(ChainSqlTx& transactor,uint160 uTxDBName, AccountID accountID, std::string sTableName, uint32 lastLedgerSequence, uint256 txhash, STTx const & tx);

private:
    Application&                                                                app_;
    beast::Journal                                                              journal_;
    Config&                                                                     cfg_;

    std::mutex                                                                  mutexMap_;
    std::map<uint160,std::shared_ptr<TableStorageItem> >                        m_map;
    bool                                                                        m_IsHaveStorage;
    bool                                                                        m_IsStorageOn;
    bool                                                                        bTableStorageThread_;
	bool																		bAutoLoadTable_;
};

}
#endif

