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

#include <peersafe/gmencrypt/hardencrypt/HardEncryptObj.h>
//#include <gmencrypt/hardencrypt/sdkey.h>
#include <peersafe/gmencrypt/hardencrypt/sjk1238_26.h>
#include <peersafe/gmencrypt/softencrypt/GmSoftEncrypt.h>

#ifdef SD_KEY_SWITCH
HardEncryptObj::hardEncryptType HardEncryptObj::hEType_ = HardEncryptObj::hardEncryptType::sdkeyType;
#else
HardEncryptObj::hardEncryptType HardEncryptObj::hEType_ = HardEncryptObj::hardEncryptType::sjk1238Type;
#endif

//HardEncrypt* HardEncryptObj::getInstance(hardEncryptType hEType)
HardEncrypt* HardEncryptObj::getInstance()
{
#ifdef GM_ALG_PROCESS
    switch (hEType_)
    {
#ifdef BEGIN_SDKEY
    case hardEncryptType::sdkeyType:
        static SDkey objSdkey;
        return &objSdkey;
#endif
    case hardEncryptType::sjk1238Type:
    {
        static SJK1238 objSjk1238;
        if (objSjk1238.isHardEncryptExist())
        {
            return &objSjk1238;
        }
        else
        {
#ifdef SOFTENCRYPT
            static SoftEncrypt objSoftEncrypt;
            return &objSoftEncrypt;
#else
            return nullptr;//if the card is not exist,then return nullptr------this is function before
#endif
        }
    }
    default:
        std::cout << "hardEncryptType error!" << std::endl;
    }
#endif
    return nullptr;
}
