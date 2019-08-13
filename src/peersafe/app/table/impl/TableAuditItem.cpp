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
#include <ripple/beast/utility/rngfill.h>
#include <ripple/crypto/csprng.h>
#include <ripple/protocol/STTx.h>
#include <ripple/json/json_reader.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <peersafe/app/table/TableAuditItem.h>
#include <peersafe/app/table/TableDumpItem.h>
#include <peersafe/app/sql/TxStore.h>
#include <fstream>
#include <boost/filesystem.hpp>


namespace ripple {

namespace fs = boost::filesystem;

TableAuditItem::~TableAuditItem()
{
}

TableAuditItem::TableAuditItem(Application& app, beast::Journal journal, Config& cfg, SyncTargetType eTargetType)
	:TableDumpItem(app,journal,cfg, eTargetType)
{      
	
    aCheckID_.clear();
    aCheckField_.clear();
}


std::pair<bool, std::string> TableAuditItem::SetAuditPara(std::string sPath, const std::list<int>& idArray, const std::list<std::string> & fieldArray)
{	    
    aCheckID_ = std::move(idArray);
    aCheckField_ = std::move(fieldArray);
    ConstructCheckJson();

    return SetAuditPara("", sPath);
}

bool TableAuditItem::checkSqlValid(std::string sSql)
{
    size_t iTableNamePos = sSql.find(sTableName_);
    if (iTableNamePos == std::string::npos) return false;

    size_t iFromBeginPos = sSql.find("from");
    if (iFromBeginPos == std::string::npos) return false;

    if (iFromBeginPos > iTableNamePos)  return false;    

    std::string sFrom = sSql.substr(iFromBeginPos, iTableNamePos - iFromBeginPos);
    size_t iFromEndPos = sFrom.find_last_not_of(' ');
    if (iFromEndPos != 3)   return false;

    std::string sTable = sSql.substr(iTableNamePos);
    size_t iOtherPos = sTable.find_first_of(' ');
    if (iOtherPos != std::string::npos)
    {
        sTable = sTable.substr(0, iOtherPos);
    }
    if (sTable != sTableName_)  return false;

    return true;
}

std::pair<bool, std::string> TableAuditItem::SetAuditPara(std::string sSql, std::string sPath)
{
    sDumpPath_ = sPath;
    beast::rngfill(uNewTableNameInDB_.begin(), uNewTableNameInDB_.size(), crypto_prng());
    sNickName_ = to_string(uNewTableNameInDB_);
    std::string sRealTableName = "t_" + sNickName_;

    if (!checkSqlValid(sSql))
    {
        return std::make_pair(false, "sql error , or table name is different form  the one in first para.");
    }

    sCheckSQL_ = sSql.replace(sSql.find(sTableName_), sTableName_.length(), sRealTableName);

    fs::path sFullPath(sDumpPath_);
    auto filePath = sFullPath.parent_path();
    auto fileName = sFullPath.filename();

    if (!fs::exists(filePath))
    {
        bool bRet = fs::create_directories(filePath);
        if (!bRet)  return std::make_pair(false, "path is invalid.");
    }

    FILE *fDump;
    fDump = fopen(sDumpPath_.c_str(), "w");
    if (!fDump)
    {
        return std::make_pair(false, "fail to open the file.");
    }
    else
    {
        std::string sWrite = "[\n]\n";
        fwrite(sWrite.c_str(), 1, sWrite.size(), fDump);
        fclose(fDump);
    }

    SetPara("", 0, uint256(0), 0, uint256(0), uint256(0));

    uLedgerStart_ = uCreateLedgerSequence_;
    uLedgerStop_  = app_.getLedgerMaster().getValidLedgerIndex();

    return std::make_pair(true, sNickName_);
}

void TableAuditItem::ConstructCheckJson()
{
    Json::Value aField, aIn, conditionJson,tableJson,rawJson;
    for (const auto &fieldItem : aCheckField_)
    {
        aField.append(fieldItem);
    }

    tableJson[jss::Table][jss::TableName] = to_string(uNewTableNameInDB_);
    tableJson[jss::Table][jss::NameInDB] = to_string(uNewTableNameInDB_);
    jsonCheck_[jss::Tables].append(tableJson);

    rawJson.append(aField);

    if (aCheckID_.size() > 1)
    {
        for (auto idItem : aCheckID_)
        {
            aIn.append(idItem);
        }
        conditionJson["id"]["$in"] = aIn;
    }
    else if(aCheckID_.size() == 1)
    {
        conditionJson["id"] = aCheckID_.front();
    }
    else
    {
        assert(0);
    }
    rawJson.append(conditionJson);

    jsonCheck_[jss::Raw] = rawJson.toStyledString();
}
bool TableAuditItem::isTxNeededOutput(const STTx& tx, std::vector<STTx>& vecTxs)
{
    //Json::Value  jsonRet1 = getTxStore().txHistory(jsonCheck_);
    for (auto& txItem : vecTxs)
    {
        STArray &tablesTx = txItem.peekFieldArray(sfTables);
        if (tablesTx.size()<= 0)  continue;
        STObject &tableItem = *(tablesTx.begin());

        ripple::uint160 uNameInDBOld = tableItem.getFieldH160(sfNameInDB);
        tableItem.setFieldH160(sfNameInDB, uNewTableNameInDB_);
        auto ret = std::make_pair(true, std::string(""));
        auto op_type = txItem.getFieldU16(sfOpType);
        if (!isNotNeedDisposeType((TableOpType)op_type))
        {
            ret = getTxStore().Dispose(txItem);
            if (ret.first)
            {
                JLOG(journal_.trace()) << "table "<< sTableName_<<"Dispose success";
            }
            else
                JLOG(journal_.trace()) << "table " << sTableName_ << "Dispose error";                
        }
        tableItem.setFieldH160(sfNameInDB, uNameInDBOld);
    }
    //Json::Value  jsonRet = getTxStore().txHistory(jsonCheck_);    
    Json::Value  jsonRet = getTxStore().txHistory(sCheckSQL_);
    if (jsonLashResult_ != jsonRet)
    {
        jsonLashResult_ = jsonRet;
        return true;
    }
    return false;
}


void TableAuditItem::issuesAfterStop()
{
    DeleteTable(to_string(uNewTableNameInDB_));
}
}
