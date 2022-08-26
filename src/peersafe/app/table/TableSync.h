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

#ifndef RIPPLE_APP_TABLE_TABLESYNC_H_INCLUDED
#define RIPPLE_APP_TABLE_TABLESYNC_H_INCLUDED

#include <ripple/core/Config.h>
#include <ripple/beast/utility/PropertyStream.h>
#include <ripple/protocol/Protocol.h>
#include <ripple/basics/Log.h>
#include <ripple/basics/TaggedCache.h>
#include <peersafe/schema/Schema.h>
#include <peersafe/app/table/TableSyncItem.h>
#include <peersafe/app/table/TableDumpItem.h>
#include <peersafe/app/table/TableAuditItem.h>


namespace ripple {


class STEntry;
class Ledger;


class TableSync
{
public:
    using clock_type = beast::abstract_clock <std::chrono::steady_clock>;
    //using SleCache = TaggedCache<LedgerIndex, std::map<AccountID, std::shared_ptr<const ripple::SLE>>>;
    TableSync(Schema& app, Config& cfg, beast::Journal journal);
    virtual ~TableSync();

    //receiver find the ledger which includes the tableNode TX
    void SeekTableTxLedger(std::shared_ptr <protocol::TMGetTable> const& m, std::weak_ptr<Peer> const& wPeer);

    //send sync request to peers        
    bool SendSyncRequest(AccountID accountID, std::string sNameInDB, LedgerIndex iStartSeq, uint256 iStartHash, LedgerIndex iCheckSeq, uint256 iCheckHash, LedgerIndex iStopSeq, bool bGetLost, std::shared_ptr <TableSyncItem> pItem);
    
    bool isExist(std::list<std::shared_ptr <TableSyncItem>>  listTableInfo_, AccountID accountID, std::string sTableName, TableSyncItem::SyncTargetType eTargeType);

     bool
    isSync(std::list<std::shared_ptr<TableSyncItem>> listTableInfo_, std::string uTxDBName, TableSyncItem::SyncTargetType eTargeType);

    //get reply
    bool GotSyncReply(std::shared_ptr <protocol::TMTableData> const& m, std::weak_ptr<Peer> const& wPeer);
    bool SendSeekResultReply(std::string sAccountID, bool bStop, uint32_t time, std::weak_ptr<Peer> const& wPeer, std::string sNickName , TableSyncItem::SyncTargetType eTargeType, LedgerIndex TxnLgrSeq, uint256 TxnLgrHash, LedgerIndex PreviousTxnLgrSeq, uint256 PrevTxnLedgerHash, std::string sNameInDB);
    bool SendSeekEndReply(LedgerIndex iSeq, uint256 hash, LedgerIndex iLastSeq, uint256 lastHash, uint256 checkHash, std::string account, std::string tablename, std::string nickName, uint32_t time, TableSyncItem::SyncTargetType eTargeType, std::weak_ptr<Peer> const& wPeer);

    bool SendLedgerRequest(LedgerIndex iSeq, uint256 hash, std::shared_ptr <TableSyncItem> pItem);
    bool GotLedger(std::shared_ptr <protocol::TMLedgerData> const& m);

    void
    SeekTableTxLedger(TableSyncItem::BaseInfo& stItemInfo);
    void CheckSyncTableTxs(std::shared_ptr<Ledger const> const& ledger);
    bool OnCreateTableTx(STTx const& tx, std::shared_ptr<Ledger const> const& ledger, uint32_t time, uint256 const& chainId, bool isPubErrInfo);
    bool ReStartOneTable(AccountID accountID, std::string sNameInDB, std::string sTableName, bool bDrop, bool bCommit);
    bool StopOneTable(AccountID accountID, std::string sNameInDB, bool bNewTable);

	std::pair<bool, std::string> StartDumpTable(std::string sPara, std::string sPath, TableDumpItem::funDumpCB funCB);
	std::pair<bool, std::string> StopDumpTable(AccountID accountID, std::string sTableName);
    bool GetCurrentDumpPos(AccountID accountID, std::string sTableName, TableSyncItem::taskInfo &info);

    std::pair<bool, std::string> StartAuditTable(std::string sPara, std::string sSql, std::string sPath);
    std::pair<bool, std::string> StopAuditTable(std::string sNickName);
    bool GetCurrentAuditPos(std::string sNickName, TableSyncItem::taskInfo &info);

    void TryTableSync();
    void TableSyncThread();

    void TryLocalSync();
    void LocalSyncThread();

    void SetHaveSyncFlag(bool haveSync);
    void Sweep();
    bool InitTableItems();
    bool IsInitTable();
    Json::Value SyncInfo(std::string const& nameInDB);

private:
    std::tuple<AccountID, SecretKey, bool>
    ParseSecret(std::string secret, std::string user);
    std::tuple<AccountID, AccountID, SecretKey, bool> 
        ParseSyncAccount(std::string line);
	std::pair<std::shared_ptr<TableSyncItem>, std::string> 
        CreateOneItem(TableSyncItem::SyncTargetType eTargeType, std::string line);
    void CreateItemWithOwner(
            AccountID owner,
            std::pair<AccountID, SecretKey> user,
            STObject const& table,
            std::vector<std::shared_ptr<TableSyncItem>>& vec);
    std::vector<std::shared_ptr<TableSyncItem>> 
        CreateItemsWithOwner(AccountID owner,std::pair<AccountID,SecretKey> user);
    void CreateTableItems();
    //check ledger according to the skip node
    bool CheckTheReplyIsValid(std::shared_ptr <protocol::TMTableData> const& m);
    bool CheckSyncDataBy256thLedger(std::shared_ptr <TableSyncItem> pItem, LedgerIndex index, uint256 ledgerHash);    
    bool SendData(std::shared_ptr <TableSyncItem> pItem, std::shared_ptr <protocol::TMTableData> const& m);
    bool MakeTableDataReply(std::string sAccountID, bool bStop, uint32_t time, std::string sNickName, TableSyncItem::SyncTargetType eTargeType, LedgerIndex TxnLgrSeq, uint256 TxnLgrHash, LedgerIndex PreviousTxnLgrSeq, uint256 PrevTxnLedgerHash, std::string sNameInDB, protocol::TMTableData &m);
    bool MakeSeekEndReply(LedgerIndex iSeq, uint256 hash, LedgerIndex iLastSeq, uint256 lastHash, uint256 checkHash, std::string account, std::string nameInDB, std::string sNickName, uint32_t time, TableSyncItem::SyncTargetType eTargeType, protocol::TMTableData &reply);
    bool
    MakeDataForTx(
        std::shared_ptr<const STTx> stTx,
        protocol::TMTableData& m,
        std::string& sNameInDB,
        std::shared_ptr<Ledger const> ledger,
        int& txnCount);

    bool Is256thLedgerExist(LedgerIndex index);
    uint256 GetLocalHash(LedgerIndex ledgerSeq);

    bool InsertSnycDB(std::string TableName, std::string TableNameInDB, std::string Owner, LedgerIndex LedgerSeq, uint256 LedgerHash, bool IsAutoSync, std::string time, uint256 chainId);
    //bool ReadSyncDB(std::string nameInDB, std::string Owner, LedgerIndex &txnseq, uint256 &txnhash,LedgerIndex &seq, uint256 &hash, uint256 &ReadSyncDB, bool &bDeleted);

    bool ReadSyncDB(std::string nameInDB, LedgerIndex &txnseq, uint256 &txnhash, LedgerIndex &seq, uint256 &hash, uint256 &txnupdatehash);

    bool IsNeedSyn(std::shared_ptr <TableSyncItem> pItem);
    bool IsNeedSyn();

	bool ClearNotSyncItem();

    std::shared_ptr <TableSyncItem> GetRightItem(AccountID accountID, std::string sTableName, std::string sNickName, TableSyncItem::SyncTargetType eTargeType, bool bByNameInDB = true);

    std::pair<bool,std::string>
		InsertListDynamically(AccountID accountID, std::string sTableName, std::string sNameInDB, LedgerIndex seq, uint256 uHash, uint32_t time,uint256 chainId);

    bool IsAutoLoadTable();

private:
	Schema&										app_;
    beast::Journal                              journal_;
    Config&                                     cfg_;

    std::recursive_mutex                        mutexlistTable_;
    std::list<std::shared_ptr <TableSyncItem>>  listTableInfo_;
	std::map<std::string, std::string>			setTableInCfg_;
    std::map<AccountID, std::pair<AccountID,SecretKey>>
                                                mapOwnerInCfg_;

    std::mutex                                  mutexTempTable_;
    std::list<std::string>                      listTempTable_;

    std::mutex                                  mutexSkipNode_;
    TaggedCache <LedgerIndex, Blob>             checkSkipNode_;

    std::mutex                                  mutexCreateTable_;
    bool                                        bAutoLoadTable_;

    bool                                        bIsHaveSync_;

    bool bInitTableItems_{false};
    // if the sync thread is running
    std::atomic_bool bTableSyncThread_{false};
    std::atomic_bool bLocalSyncThread_{false};

	std::string                                 sLastErr_;

	//press test related
	bool										bPressSwitchOn_;
	std::string									pressRealName_;
};

}
#endif

