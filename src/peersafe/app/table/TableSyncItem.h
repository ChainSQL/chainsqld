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

#ifndef RIPPLE_APP_TABLE_TABLESYNC_ITEM_H_INCLUDED
#define RIPPLE_APP_TABLE_TABLESYNC_ITEM_H_INCLUDED

#include <ripple/beast/utility/PropertyStream.h>
#include <ripple/app/main/Application.h>
#include <ripple/beast/clock/abstract_clock.h>
#include <ripple/beast/core/WaitableEvent.h>
#include <ripple/protocol/AccountID.h>
#include <ripple/protocol/Protocol.h>
#include <ripple/overlay/Peer.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/SecretKey.h>

namespace ripple {

class STTx;

//class Peer;
class TableSyncItem
{
public:
    using clock_type = beast::abstract_clock <std::chrono::steady_clock>;
    using sqldata_type = std::pair<LedgerIndex, protocol::TMTableData>;	

    enum TableSyncState
    {        
		/*
			three cases will be set to SYNC_INIT:
			1. after program launched 
			2. after table created
			3. a
		*/
        SYNC_INIT,
		//state after restart one table,will change to SYNC_BLOCK_STOP
        SYNC_REINIT,
		//waiting for remote data acquired
        SYNC_WAIT_DATA,
		//table enter sync state,will sync tx from local or remote node
        SYNC_BLOCK_STOP,
		//waiting local acquiring tx,will change to SYNC_BLOCK_STOP after finished 
        SYNC_WAIT_LOCAL_ACQUIRE,
		//acquiring tx from local
        SYNC_LOCAL_ACQUIRING,
		//will delete the real table and set 'deleted' to '1' in SyncTableState
        SYNC_DELETING,
		//will not sync until state change
        SYNC_STOP,
		//Table will be removed from listTableInfo_ the next ledger
		SYNC_REMOVE
    };

    enum TableSyncCondType
    {
        SYNC_NOCONDITION,
        SYNC_PRIOR,
        SYNC_AFTER,
        SYNC_JUMP
    };

    enum CheckConditionState
    {
        CHECK_ADVANCED,
        CHECK_REJECT,
        CHECK_JUMP
    };

    enum LedgerSyncState
    {
        SYNC_NO_LEDGER,
        SYNC_WAIT_LEDGER,
        SYNC_GOT_LEDGER
    };

	enum SyncTargetType
	{
		SyncTarget_db,
		SyncTarget_dump,
        SyncTarget_audit,
	};    

    struct BaseInfo
    {
        AccountID                                                    accountID;
        std::string                                                  sTableName;
        std::string                                                  sTableNameInDB;
        std::string                                                  sNickName;

        LedgerIndex                                                  u32SeqLedger; 
        uint256                                                      uHash;

        LedgerIndex                                                  uTxSeq ;
        uint256                                                      uTxHash;

		uint256                                                      uTxUpdateHash;

        LedgerIndex                                                  uStopSeq;

        TableSyncState                                               eState;
        LedgerSyncState                                              lState;
        
        bool                                                         isAutoSync;
		bool														 isDeleted;
		SyncTargetType                                               eTargetType;
    };

    struct cond
    {
        time_t                                                    utime;
        LedgerIndex                                               uledgerIndex;

        std::string                                               stxid;
        TableSyncCondType                                         eSyncType;

        cond ()
        {
            utime                = 0;
            uledgerIndex         = 0;

            stxid = "";
            eSyncType = SYNC_NOCONDITION;
        }
    };

    struct taskInfo
    {
        LedgerIndex                                              uStartPos;
        LedgerIndex                                              uStopPos;
        LedgerIndex                                              uCurPos;

        taskInfo()
        {
            uStartPos = 0;
            uStopPos  = 0;
            uCurPos   = 0;
        }
    };

public:     
    TableSyncItem(Application& app, beast::Journal journal,Config& cfg, SyncTargetType eTargetType = SyncTarget_db);
    virtual ~TableSyncItem();

    TxStoreDBConn& getTxStoreDBConn();
    TxStore& getTxStore();

    void ReInit();
	void InitCondition(const std::string& condition);
    void Init(const AccountID& id, const std::string& sName,bool isAutoSync);
	void Init(const AccountID& id, const std::string& sName,const std::string& cond,bool isAutoSync);
	void Init(const AccountID& id, const std::string& sName, const AccountID& userId,const SecretKey& user_secret, const std::string& condition, bool isAutoSync);
    void SetPara(std::string sNameInDB, LedgerIndex iSeq, uint256 hash, LedgerIndex TxnSeq, uint256 Txnhash, uint256 TxnUpdateHash);
    void SetPeer(std::shared_ptr<Peer> peer);	
	
    time_t StringToDatetime(const char *str);
    
    bool IsGetLedgerExpire();
    bool IsGetDataExpire();
    void UpdateLedgerTm();
    void UpdateDataTm();

    void StartLocalLedgerRead();
    void StopLocalLedgerRead();
    bool StopSync();

    AccountID                GetAccount();
    std::string              GetTableName();    
    std::string              GetNickName();
    TableSyncState           GetSyncState();
    cond const &             GetCondition();

    bool getAutoSync();
   
    std::shared_ptr<Peer> GetRightPeerTarget(LedgerIndex iSeq);
            
    void GetBaseInfo(BaseInfo &stInfo);
    void GetSyncLedger(LedgerIndex &iSeq, uint256 &uHash);
    void GetSyncTxLedger(LedgerIndex &iSeq, uint256 &uHash);
    LedgerSyncState GetCheckLedgerState();
     
    void SendTableMessage(Message::pointer const& m);

    void SetTableName(std::string sName);
    void SetSyncLedger(LedgerIndex iSeq, uint256 uHash);
    void SetSyncTxLedger(LedgerIndex iSeq, uint256 uHash);

    bool IsExist(AccountID accountID, std::string TableNameInDB);

    bool DeleteRecord(AccountID accountID, std::string TableName);
    bool DeleteTable(std::string nameInDB);

    bool GetMaxTxnInfo(std::string TableName, std::string Owner, LedgerIndex &TxnLedgerSeq, uint256 &TxnLedgerHash);

    bool RenameRecord(AccountID accountID, std::string TableNameInDB,std::string TableName);

    void SetSyncState(TableSyncState eState);
    void SetLedgerState(LedgerSyncState lState);
	void SetDeleted(bool deleted);

    bool IsInFailList(beast::IP::Endpoint& peerAddr);
    
    void TryOperateSQL();
    void OperateSQLThread();

	// try to decrypt raw field with configuration.
	void TryDecryptRaw(STTx& tx);
	void TryDecryptRaw(std::vector<STTx>& vecTxs);
	std::pair<bool, std::string> InitPassphrase();
	
    void PushDataToWaitCheckQueue(sqldata_type &sqlData);
    void DealWithWaitCheckQueue(std::function<bool(sqldata_type const&)>);

    bool GetRightRequestRange(TableSyncItem::BaseInfo &stRange);
    void PushDataToBlockDataQueue(sqldata_type &sqlData);
    bool TransBlock2Whole(LedgerIndex iSeq, uint256 uHash);

    void PushDataToWholeDataQueue(sqldata_type &sqlData);
    void PushDataToWholeDataQueue(std::list <sqldata_type>  &aSqlData);

    bool IsNameInDBExist(std::string TableName, std::string Owner, bool delCheck, std::string &TableNameInDB);

    bool UpdateSyncDB(AccountID accountID, std::string tableName, std::string nameInDB);

    bool UpdateStateDB(const std::string & owner, const std::string & tablename, const bool &isAutoSync);

    //bool DoUpdateSyncDB(const std::string &Owner, const std::string &TableNameInDB, const std::string &LedgerHash, const std::string &LedgerSeq, const std::string &PreviousCommit);

    bool DoUpdateSyncDB(const std::string &Owner, const std::string &TableName, const std::string &TxnLedgerHash, const std::string &TxnLedgerSeq, const std::string &LedgerHash, const std::string &LedgerSeq, const std::string &TxHash, const std::string &cond, const std::string &PreviousCommit);

    //bool DoUpdateSyncDB(const std::string &Owner, const std::string &TableNameInDB, const std::string &TxHash, const std::string &PreviousCommit);


    bool DoUpdateSyncDB(const std::string &Owner, const std::string &TableNameInDB, bool bDel,
        const std::string &PreviousCommit);
    
    std::string TableNameInDB();
    void SetTableNameInDB(uint160 NameInDB);
    void SetTableNameInDB(std::string sNameInDB);

	SyncTargetType TargetType();

    void SetIsDataFromLocal(bool bLocal);

    void ClearFailList();

    std::mutex &WriteDataMutex();
    
    void ReSetContexAfterDrop();

protected:
    CheckConditionState CondFilter(uint32 time, uint32 ledgerIndex, uint256 txid);
    bool isJumpThisTx(uint256 txid);
    std::string GetPosInfo(LedgerIndex iTxLedger, std::string sTxLedgerHash, LedgerIndex iCurLedger, std::string sCurLedgerHash, bool bStop, std::string sMsg);

private:
    bool GetIsChange();
    void PushDataByOrder(std::list <sqldata_type> &aData, sqldata_type &sqlData);

    void ReSetContex();
    
    TableStatusDB& getTableStatusDB();

	std::string getOperationRule(const STTx& tx);

	std::pair<bool, std::string> DealTranCommonTx(const STTx &tx);
	std::pair<bool, std::string> DealWithTx(const std::vector<STTx>& vecTxs);

	void InsertPressData(const STTx& tx,uint32 ledgerSeq,uint32 ledgerTime);
	virtual bool DealWithEveryLedgerData(const std::vector<protocol::TMTableData> &aData);
public:
    LedgerIndex                                                  u32SeqLedger_;  //seq of ledger, last syned ledger seq 
    LedgerIndex                                                  uTxSeq_;
    uint256                                                      uTxHash_;
    std::string                                                  sTableName_;
    std::string                                                  sNickName_;
protected:
	Application &                                                app_;
	beast::Journal                                               journal_;
	Config&                                                      cfg_;
	std::string                                                  sTableNameInDB_;
    LedgerIndex                                                  uCreateLedgerSequence_;   
    AccountID                                                    accountID_;		//owner accountId
private:
	boost::optional<AccountID>									 user_accountID_;	//user accountId
	boost::optional<SecretKey>									 user_secret_;		//user secret
	Blob														 passBlob_;			//decrypted passphrase
	bool														 confidential_;
	bool														 deleted_;

    uint256                                                      uHash_;
    uint256                                                      uTxDBUpdateHash_;

    TableSyncState                                               eState_;
    LedgerSyncState                                              lState_;	

    std::mutex                                                   mutexInfo_;
    std::mutex                                                   mutexWriteData_;
private:
    std::chrono::steady_clock::time_point                        clock_data_;
    std::chrono::steady_clock::time_point                        clock_ledger_;        

    std::list <sqldata_type>                                     aBlockData_;
    std::mutex                                                   mutexBlockData_;

    std::list <sqldata_type>                                     aWholeData_;
    std::mutex                                                   mutexWholeData_;   

    std::list <sqldata_type>                                     aWaitCheckData_;
    std::mutex                                                   mutexWaitCheckQueue_;
    
    bool                                                         bOperateSQL_;

    bool                                                         bGetLocalData_;

    beast::IP::Endpoint                                          uPeerAddr_;
    std::list <beast::IP::Endpoint>                              lfailList_;
    bool                                                         bIsChange_;

    bool                                                         bIsAutoSync_;

    std::unique_ptr <TxStoreDBConn>                              conn_;
    std::unique_ptr <TxStore>                                    pObjTxStore_;
    std::unique_ptr <TableStatusDB>                              pObjTableStatusDB_;
  
    cond                                                         sCond_;    

	SyncTargetType                                               eSyncTargetType_;
    
    beast::WaitableEvent                                         operateSqlEvent;
    beast::WaitableEvent                                         readDataEvent;
};

}
#endif

