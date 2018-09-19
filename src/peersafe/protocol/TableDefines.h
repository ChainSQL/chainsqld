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

#ifndef RIPPLE_PROTOCOL_TABLEDEFINES_H_INCLUDED
#define RIPPLE_PROTOCOL_TABLEDEFINES_H_INCLUDED


enum TableOpType {
    T_COMMON       = 0,
    T_CREATE       = 1,
    T_DROP         = 2,
    T_RENAME       = 3,
    T_ASSIGN       = 4,
    T_CANCELASSIGN = 5,
    R_INSERT       = 6,
    R_GET          = 7,
    R_UPDATE       = 8,
    R_DELETE       = 9,
    T_ASSERT       = 10,
	T_GRANT        = 11,
    T_RECREATE     = 12,
    T_REPORT       = 13,
};

inline bool isTableListSetOpType(TableOpType eType)
{
	if (eType < R_INSERT && eType >= T_CREATE)                            return true;
	if (eType == T_GRANT || eType == T_RECREATE || eType == T_REPORT)     return true;
    
	return false;
}

inline bool isNotNeedDisposeType(TableOpType eType)
{
	if (eType == T_ASSIGN       || 
		eType == T_CANCELASSIGN || 
		eType == T_GRANT        || 
		eType == T_RENAME       ||
        eType == T_REPORT)
		return true;
	return false;
}

inline bool isSqlStatementOpType(TableOpType eType)
{
	if (eType == R_INSERT || eType == R_UPDATE || eType == R_DELETE || eType == T_ASSERT)  return true;
	return false;
}

inline bool isStrictModeOpType(TableOpType eType)
{
	if (eType == R_INSERT || eType == R_UPDATE || eType == R_DELETE ||eType == T_CREATE || eType == T_RECREATE)  return true;
	return false;
}

enum TableRoleFlags
{
	lsfNone = 0,
	lsfSelect = 0x00010000,
	lsfInsert = 0x00020000,
	lsfUpdate = 0x00040000,
	lsfDelete = 0x00080000,
	lsfExecute = 0x00100000,

	lsfAll = lsfSelect | lsfInsert | lsfUpdate | lsfDelete | lsfExecute,
};


struct table_BaseInfo
{
    ripple::uint160                                              nameInDB;
    bool                                                         isDeleted;
    ripple::LedgerIndex                                          createLgrSeq;
    ripple::uint256                                              createdLedgerHash;
    ripple::uint256                                              createdTxnHash;
    ripple::LedgerIndex                                          previousTxnLgrSeq;
    ripple::uint256                                              prevTxnLedgerHash;


    table_BaseInfo()
    {
        nameInDB = 0;
        isDeleted = false;
        createLgrSeq = 0;
        createdLedgerHash = 0;
        createdTxnHash = 0;
        previousTxnLgrSeq = 0;
        prevTxnLedgerHash = 0;
    }
};

inline TableRoleFlags getFlagFromOptype(TableOpType eOpType)
{
	switch (eOpType)
	{
	case R_INSERT:
		return lsfInsert;
	case R_UPDATE:
		return lsfUpdate;
	case R_DELETE:
		return lsfDelete;
	case R_GET:
		return lsfSelect;
	default:
		return lsfNone;
	}
}

inline TableRoleFlags getOptypeFromString(std::string sOpType)
{
	if (sOpType == "select")              return lsfSelect;
	else if (sOpType == "insert")         return lsfInsert;
	else if (sOpType == "update")         return lsfUpdate;
	else if (sOpType == "delete")         return lsfDelete;
	else                                  return lsfNone;
}

#endif
