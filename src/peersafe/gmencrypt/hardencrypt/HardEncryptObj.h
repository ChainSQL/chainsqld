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
#ifndef HARDENCRYPT_HARDENCRYPT_OBJ_H_INCLUDE
#define HARDENCRYPT_HARDENCRYPT_OBJ_H_INCLUDE
#include <peersafe/gmencrypt/hardencrypt/HardEncrypt.h>
#include <iostream>

class HardEncryptObj
{
private:
    HardEncryptObj();

public:
    enum hardEncryptType { unknown = -1, sdkeyType, sjk1238Type };
    static hardEncryptType hEType_;

public:
    static HardEncrypt* getInstance();
};

#endif
