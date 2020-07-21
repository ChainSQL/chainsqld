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

#pragma once
#ifndef GMENCRYPT_GMENCRYPT_OBJ_H_INCLUDE
#define GMENCRYPT_GMENCRYPT_OBJ_H_INCLUDE
#include <peersafe/gmencrypt/GmEncrypt.h>
#include <iostream>

class GmEncryptObj
{
private:
    GmEncryptObj();

public:
    enum gmAlgType { unknown = -1, sdkeyType, sjkCardType, soft };
    static gmAlgType hEType_;

public:
    static void setGmAlgType(gmAlgType gmAlgType);
    static GmEncrypt* getInstance();
    static gmAlgType fromString(std::string gmAlgTypeStr);
};

#endif
