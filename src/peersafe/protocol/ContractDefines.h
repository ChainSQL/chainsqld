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

#ifndef RIPPLE_PROTOCOL_CONTRACTDEFINES_H_INCLUDED
#define RIPPLE_PROTOCOL_CONTRACTDEFINES_H_INCLUDED


enum ContractOpType {
    ContractCreation = 1,			///< Transaction to create contracts - receiveAddress() is ignored.
    MessageCall = 2,			///< Transaction to invoke a message call - receiveAddress() is used.

    typeMax
};

inline bool isContractTypeValid(ContractOpType eType)
{
    if (eType >= ContractCreation && eType < typeMax)  return true;
    
	return false;
}

#endif
