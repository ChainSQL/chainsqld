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

#ifndef RIPPLE_APP_TABLE_TABLEAUDIT_ITEM_H_INCLUDED
#define RIPPLE_APP_TABLE_TABLEAUDIT_ITEM_H_INCLUDED

#include <peersafe/app/table/TableDumpItem.h>

namespace ripple {

class TableAuditItem : public TableDumpItem
{
public:     
    TableAuditItem(Application& app, beast::Journal journal,Config& cfg, SyncTargetType eTargetType);
    virtual ~TableAuditItem();

    std::pair<bool, std::string> SetAuditPara(std::string sPath, const std::list<int>& idArray, const std::list<std::string> & fieldArray);
    std::pair<bool, std::string> SetAuditPara(std::string sSql, std::string sPath);

private:	
    bool isTxNeededOutput(const STTx& tx, std::vector<STTx>& vecTxs);    
    void ConstructCheckJson();   
    void issuesAfterStop();
    bool checkSqlValid(std::string sSql);

private:
	
    ripple::uint160                                              uNewTableNameInDB_;

    std::list <int>                                              aCheckID_;
    std::list <std::string>                                      aCheckField_;
    Json::Value                                                  jsonCheck_;

    Json::Value                                                  jsonLashResult_;
    std::string                                                  sCheckSQL_;
};

}
#endif

