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

#ifndef RIPPLE_APP_TABLE_TABLEDUMP_ITEM_H_INCLUDED
#define RIPPLE_APP_TABLE_TABLEDUMP_ITEM_H_INCLUDED

#include <peersafe/app/table/TableSyncItem.h>


namespace ripple {

class TableDumpItem : public TableSyncItem
{
public:
	using funDumpCB = std::function<void(AccountID, std::string, int, bool)>;	

public:     
    TableDumpItem(Application& app, beast::Journal journal,Config& cfg, SyncTargetType eTargetType);
    virtual ~TableDumpItem();

	std::pair<bool, std::string> SetDumpPara(std::string sPath, funDumpCB funCB);
    std::pair<bool, std::string> StopTask();    

    void GetCurrentPos(taskInfo &info);
    std::string GetOutputPath() { return sDumpPath_; }

protected:
    std::string ConstructTxStr(std::vector<STTx> &vecTxs, const STTx & tx);
    //first:pos of ']',second:distance of first char not \r & \n from ']'.	
    std::pair<int, int> GetRightTxEndPos(FILE * fp, bool &bEmptyTx);
    void SetStopInfo(FILE *fileTarget, std::string sMsg);
    void SetErroeInfo2FileEnd(FILE *fileTarget);
    virtual bool DealWithEveryLedgerData(const std::vector<protocol::TMTableData> &aData);

private:		
    static Json::Value TransRaw2Json(const STTx & tx);
    static bool UnHexTableName(Json::Value &jsonTx);
    void StopInnerDeal(FILE *fileTarget, std::string sMsg);

    virtual void issuesAfterStop();
    virtual bool isTxNeededOutput(const STTx& tx, std::vector<STTx>& vecTxs);

protected:
    std::string                                                  sDumpPath_;
    LedgerIndex                                                  uTxSeqRecord_;
    std::string                                                  sTxHashRecord_;
    LedgerIndex                                                  uLedgerSeqRecord_;
    std::string                                                  sLedgerHashRecord_;

    LedgerIndex                                                  uLedgerStart_;
    LedgerIndex                                                  uLedgerStop_;

private:	
	funDumpCB                                                    funDumpCB_;    
    std::mutex                                                   mutexFileOperate_;
};

}
#endif

