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

#include <peersafe/gmencrypt/GmEncryptObj.h>
//#include <gmencrypt/hardencrypt/sdkey.h>
#include <peersafe/gmencrypt/hardencrypt/sjkCard.h>
#include <peersafe/gmencrypt/softencrypt/GmSoftEncrypt.h>

// #ifdef SD_KEY_SWITCH
// GmEncryptObj::gmAlgType GmEncryptObj::hEType_ = GmEncryptObj::gmAlgType::sdkeyType;
// #else
// GmEncryptObj::gmAlgType GmEncryptObj::hEType_ = GmEncryptObj::gmAlgType::sjkCardType;
// #endif
#ifdef HARD_GM
GmEncryptObj::gmAlgType GmEncryptObj::hEType_ = gmAlgType::sjkCardType;
#else
GmEncryptObj::gmAlgType GmEncryptObj::hEType_ = gmAlgType::soft;
#endif

GmEncryptObj::gmAlgType GmEncryptObj::fromString(std::string gmAlgTypeStr)
{
    if(gmAlgTypeStr == "soft")
        return gmAlgType::soft;
    if(gmAlgTypeStr == "hard")
        return gmAlgType::sjkCardType;
    return gmAlgType::unknown;
}

void GmEncryptObj::setGmAlgType(gmAlgType gmAlgType)
{
    GmEncryptObj::hEType_ = gmAlgType;
}

//GmEncrypt* GmEncryptObj::getInstance(gmAlgType hEType)
GmEncrypt* GmEncryptObj::getInstance(gmAlgType gmAlgType)
{
    switch (gmAlgType)
    {
#ifdef BEGIN_SDKEY
    case gmAlgType::sdkeyType:
    {
        static SDkey objSdkey;
        return &objSdkey;
    }
#endif
#ifdef HARD_GM
    case gmAlgType::sjkCardType:
    {

        static SJKCard objSjkCard;
        if (objSjkCard.isHardEncryptExist())
        {
            return &objSjkCard;
        }
        else
        {
            static SoftEncrypt objSoftEncrypt;
            return &objSoftEncrypt;
        }
    }
#endif
#ifdef SOFTENCRYPT
    case gmAlgType::soft:
    {
        static SoftEncrypt objSoftEncrypt;
        return &objSoftEncrypt;
    }
#endif
    default:
        std::cout << "GmEncryptType error!" << std::endl;
    }
    return nullptr;
}
