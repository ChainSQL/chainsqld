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

#include <ripple/protocol/JsonFields.h>
#include <ripple/protocol/STTx.h>
#include <ripple/json/json_reader.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/ledger/TransactionMaster.h>
#include <peersafe/app/table/TableDumpItem.h>
#include <fstream>
#include <boost/filesystem.hpp>
#include <ripple/ledger/impl/Tuning.h>


namespace ripple {

namespace fs = boost::filesystem;
#define BUFFER_FOR_POSINFO  1024

TableDumpItem::~TableDumpItem()
{
}

TableDumpItem::TableDumpItem(Application& app, beast::Journal journal, Config& cfg, SyncTargetType eTargetType)
	:TableSyncItem(app,journal,cfg, eTargetType)
{       
	sDumpPath_            = "";
	funDumpCB_            = NULL;
    uTxSeqRecord_         = 0;
    sTxHashRecord_        = "";
    uLedgerSeqRecord_     = 0;
    sLedgerHashRecord_    = "";
    uLedgerStart_         = 0;
    uLedgerStop_          = 0;
}

void TableDumpItem::GetCurrentPos(taskInfo &info)
{ 
    info.uStartPos     = uLedgerStart_;
    info.uStopPos      = uLedgerStop_;
    info.uCurPos       = uLedgerSeqRecord_;
    return;    
}

std::pair<int,int> TableDumpItem::GetRightTxEndPos(FILE * fp, bool &bEmptyTx)
{
	if (fp == NULL)  return std::make_pair(0,0);

	long lSeek = 0;
	int fRet = 0;	

	while (fRet == 0)
	{
		fRet = fseek(fp, lSeek--, SEEK_END);
		char ch;
		int iRet = fscanf(fp, "%c", &ch);
		if (iRet > 0 &&  ch == ']')
		{
			long lRightPos = lSeek + 1;
			int i = 1;
			do {
				fRet = fseek(fp, lRightPos - i++, SEEK_END);
				if (fRet == 0)
				{
					iRet = fscanf(fp, "%c", &ch);
					if (ch != '\r' && ch != '\n')
					{
						bEmptyTx = ch == '[';
						return std::make_pair(lRightPos,i-1);
					}					
				}
			} while (fRet == 0);

			return std::make_pair(lRightPos,0);
		}
	}

	return std::make_pair(0, 0);
}
std::pair<bool, std::string> TableDumpItem::SetDumpPara(std::string sPath, funDumpCB funCB)
{		
	sDumpPath_ = sPath;
    uLedgerStart_ = uCreateLedgerSequence_;

	fs::path sFullPath(sDumpPath_);
	auto filePath = sFullPath.parent_path();
	auto fileName = sFullPath.filename();

	if (!fs::exists(filePath))
	{
       bool bRet = false;
       try
       {
           bRet = fs::create_directories(filePath);
       }
       catch (std::exception const&)
       {
       }
		if (!bRet)  return std::make_pair(false,"path is invalid.");
	}

	FILE *fDump;
	fDump = fopen(sDumpPath_.c_str(), "a+");
	if (!fDump)
	{
		return std::make_pair(false, "fail to open the file.");
	}

	char buf[BUFFER_FOR_POSINFO];
	std::string sPosJson;
	int fRet = 0;
	bool bEmptyTx = false;
	auto posPair = GetRightTxEndPos(fDump, bEmptyTx);

	if (posPair.first < 0)
	{
		fRet = fseek(fDump, posPair.first + posPair.second, SEEK_END);
        
		if (fRet == 0)
		{    
			size_t result = fread(buf, 1, BUFFER_FOR_POSINFO, fDump);
			if(result > 0 && result < BUFFER_FOR_POSINFO)
				sPosJson = buf;
		}
	}	
	
	if (sPosJson.length() > 0)//before 1024,if encounter \n,then stop
	{
		Json::Value pos;
		if (Json::Reader().parse(sPosJson, pos) == true)
		{
            //check first
            if (to_string(accountID_) != pos["Account"].asString())
            {
                fclose(fDump);
                return std::make_pair(false, "account in parameter list is different form in target file, please set a new target file.");
            }
            if (sTableName_ != pos["TableName"].asString())
            {
                fclose(fDump);
                return std::make_pair(false, "tablename in parameter list is different form in target file, please set a new target file.");
            }
            if (uCreateLedgerSequence_ != pos["TxnCreateSeq"].asUInt())
            {
                fclose(fDump);
                return std::make_pair(false, sTableName_ + " is a new created file," + sTableName_ + " in target file may have been deleted , please set a new target file.");
            }

			SetPara("", pos["LedgerSeq"].asUInt(), from_hex_text<uint256>(pos["LedgerHash"].asString()), pos["TxnLedgerSeq"].asUInt(), from_hex_text<uint256>(pos["TxnHash"].asString()), uint256(0));
            uTxSeqRecord_ = pos["TxnLedgerSeq"].asUInt();
            sTxHashRecord_ = pos["TxnHash"].asString();

            uLedgerStart_ = u32SeqLedger_;
		}
		else
		{
			SetPara("", 0, uint256(0), 0, uint256(0), uint256(0));
		}
	}
	else
	{		
		std::string sWrite = "[\n]\n";

		std::string sPos = GetPosInfo(0, to_string(uint256(0)), 0, to_string(uint256(0)),false, "");

		sWrite = sWrite + sPos;
		fwrite(sWrite.c_str(), 1, sWrite.size(), fDump);

		SetPara("", 0, uint256(0), 0, uint256(0), uint256(0));
	}
	fclose(fDump);

    uLedgerStop_ = app_.getLedgerMaster().getValidLedgerIndex();
	return std::make_pair(true, "");
}

Json::Value TableDumpItem::TransRaw2Json(const STTx & tx)
{
	Json::Value jsonRaw;
	if (tx.isFieldPresent(sfRaw))
	{		
		Json::Reader().parse(strCopy(tx.getFieldVL(sfRaw)), jsonRaw);
	}
	return jsonRaw;
}
bool TableDumpItem::UnHexTableName(Json::Value &jsonTx)
{
	Json::Value &jsonTables = jsonTx[jss::Tables];
	for (auto &jsonTable : jsonTables)
	{
		std::string sTableName = jsonTable[jss::Table][jss::TableName].asString();
		auto retPair = strUnHex(sTableName);
		if (retPair.second)
		{
			sTableName = strCopy(retPair.first);
			jsonTable[jss::Table][jss::TableName] = sTableName;
		}		
	}
	return true;
}
std::string TableDumpItem::ConstructTxStr(std::vector<STTx> &vecTxs, const STTx & tx)
{
	if (tx.getTxnType() == ttSQLTRANSACTION)
	{
		Json::Value jsonTransTx;

		jsonTransTx[jss::TransactionType] = "SQLTransaction";
		jsonTransTx[jss::account] = to_string(vecTxs.at(0).getAccountID(sfAccount));

		for (int i = 0; i < vecTxs.size(); i++)
		{
			Json::Value jsonTx = vecTxs[i].getJson(0);
			UnHexTableName(jsonTx);
			Json::Value jsonRaw = TransRaw2Json(vecTxs[i]);
			if (!jsonRaw.isNull())		jsonTx[jss::Raw] = jsonRaw;
			jsonTransTx[jss::Statements].append(jsonTx);
		}
		return jsonTransTx.toStyledString();
	}
	else
	{
		assert(vecTxs.size() > 0);
		if (vecTxs.size() > 0)
		{
			Json::Value jsonTx = vecTxs[0].getJson(0);
			UnHexTableName(jsonTx);
			Json::Value jsonRaw = TransRaw2Json(vecTxs[0]);
			if (!jsonRaw.isNull())		jsonTx[jss::Raw] = jsonRaw;
			return jsonTx.toStyledString();
		}
	}    
	return "";
}

bool TableDumpItem::DealWithEveryLedgerData(const std::vector<protocol::TMTableData> &aData)
{
    std::lock_guard<std::mutex> lock(mutexFileOperate_);

    FILE *fp;
    fp = fopen(sDumpPath_.c_str(), "r+");

    if (!fp)
    {
        std::cout << "open file failed." << std::endl;
		SetSyncState(SYNC_STOP);
        return false;
    }
	LedgerIndex uCurSynPos = 0;
		
	
    for (std::vector<protocol::TMTableData>::const_iterator iter = aData.begin(); iter != aData.end(); ++iter)
    {     
		uCurSynPos = iter->ledgerseq();

        std::string sLedgerHash = iter->ledgerhash();
        std::string sLedgerCheckHash = iter->ledgercheckhash();
        LedgerIndex uLedgerSeq = iter->ledgerseq();
        std::string PreviousCommit;
        
		int fRet = 0;
		bool bEmptyTx = false;
		auto posPair = GetRightTxEndPos(fp, bEmptyTx);
		if (posPair.first == 0)
		{            
            StopInnerDeal(fp, "");
            fclose(fp);
			return false;
		}

		std::string sWrite = "";
        //should fseek to file last 
		fRet = fseek(fp, posPair.first - posPair.second + 1, SEEK_END);
        if (fRet < 0)
        {            
            StopInnerDeal(fp, "");
            fclose(fp);
            return false;
        }

        //check for jump one seq, check for deadline time and deadline seq
        CheckConditionState  checkRet = CondFilter(iter->closetime(), iter->ledgerseq(), uint256(0));
        if (checkRet == CHECK_REJECT && GetSyncState() != SYNC_STOP)
        {
            StopInnerDeal(fp, "catch the condition point.");
            fclose(fp);
            return false;
        }

        if (iter->txnodes().size() > 0)
        {			
            for (int i = 0; i < iter->txnodes().size(); i++)
            {
                const protocol::TMLedgerNode &node = iter->txnodes().Get(i);

                auto str = node.nodedata();
                Blob blob;
                blob.assign(str.begin(), str.end());
                STTx tx = std::move((STTx)(SerialIter{ blob.data(), blob.size() }));

                std::vector<STTx> vecTxs = app_.getMasterTransaction().getTxs(tx, sTableNameInDB_, nullptr, iter->ledgerseq());
                TryDecryptRaw(vecTxs);
                bool bOutPut = isTxNeededOutput(tx, vecTxs);

                //CAUTION, the following code should behidnd the fun isTxNeededOutput
                //check for jump one tx.
                if (!isJumpThisTx(tx.getTransactionID()) && checkRet != CHECK_JUMP && bOutPut)
                {
                    std::string sTx = ConstructTxStr(vecTxs, tx);

                    if (bEmptyTx) sWrite += "\n";
                    else          sWrite += ",\n";

                    bEmptyTx = false;
                    sWrite += sTx;
                }
			}
			sWrite += "]\n";
            uTxSeqRecord_ = uLedgerSeq;
            sTxHashRecord_ = sLedgerCheckHash;
        }
		else
		{
			sWrite += "\n]\n";
		}		
        uLedgerSeqRecord_ = uLedgerSeq;
        sLedgerHashRecord_ = sLedgerHash;
		//update the pos info		
        std::string sPos = GetPosInfo(uTxSeqRecord_, sTxHashRecord_, uLedgerSeqRecord_, sLedgerHashRecord_, false, "");		

		sWrite += sPos;
		fwrite(sWrite.c_str(), 1, sWrite.size(), fp);

        if (uLedgerSeq >= uLedgerStop_)  break;
    }         

	//stop the dump task
	if (uLedgerStop_ <= uCurSynPos && GetSyncState() != SYNC_STOP)
	{        
        StopInnerDeal(fp, "catch the stop ledger.");
	}

    fclose(fp);
    return true;
}
void TableDumpItem::issuesAfterStop()
{

}
bool TableDumpItem::isTxNeededOutput(const STTx& tx, std::vector<STTx>& vecTxs)
{
    return true;
}
void TableDumpItem::SetStopInfo(FILE *fileTarget, std::string sMsg)
{
    SetSyncState(SYNC_STOP);    

    FILE *fp = fileTarget;
    if (fp == NULL)
    {
        fp = fopen(sDumpPath_.c_str(), "r+");
        if (!fp)
        {
            std::cout << "open file failed." << std::endl;
            return;
        }
    }

    std::string sWrite = "";

    bool bEmptyTx = false;
    auto posPair = TableDumpItem::GetRightTxEndPos(fp, bEmptyTx);
    if (posPair.first == 0)
    {
        return SetErroeInfo2FileEnd(fp);
    }


    //should fseek to file last 
    int fRet = fseek(fp, posPair.first + posPair.second, SEEK_END);
    if (fRet < 0)
    {
        return SetErroeInfo2FileEnd(fp);

    }
    //sWrite += "\n";
    std::string sPos = GetPosInfo(uTxSeqRecord_, sTxHashRecord_, uLedgerSeqRecord_, sLedgerHashRecord_, true, sMsg);
    sWrite += sPos;

    fwrite(sWrite.c_str(), 1, sWrite.size(), fp);
    if(fileTarget == NULL)       fclose(fp);
}

void TableDumpItem::SetErroeInfo2FileEnd(FILE *fileTarget)
{
    if (!fileTarget)   return;
    FILE *fp = fileTarget;

    std::string  sWrite;
    fseek(fp, 0, SEEK_END);
    std::string sError = "can't fint the position to add the new operation.";
    sWrite += "\n";
    sWrite += sError;

    fwrite(sWrite.c_str(), 1, sWrite.size(), fp);
    fclose(fp);
    return;
}

std::pair<bool, std::string> TableDumpItem::StopTask()
{
    std::lock_guard<std::mutex> lock(mutexFileOperate_);
    if (GetSyncState() == SYNC_STOP)
    {
        return std::make_pair(false, "the task has been stopped.");
    }
    StopInnerDeal(NULL, "stop on man-made.");
    return std::make_pair(true, "");
}
void TableDumpItem::StopInnerDeal(FILE *fileTarget, std::string sMsg)
{
    SetStopInfo(fileTarget, sMsg);
    issuesAfterStop();
    return ;
}

}
