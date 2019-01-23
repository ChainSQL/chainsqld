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
#ifdef BEGIN_SDKEY

#include <peersafe/gmencrypt/hardencrypt/sdkey.h>
#include <cstdlib>
#include <iostream>

unsigned long  SDkey::OpenDevice()
{
    unsigned long rv = 0;
    rv = SDKEY_OpenCard(&hEkey_);
    if (rv)
    {
        DebugPrint("SDKEY_OpenCard error. rv=0x%08x.",rv);
    }
    else
    {
        DebugPrint("SDKEY_OpenCard OK!");
    }
    return rv;
}
unsigned long  SDkey::CloseDevice()
{
    if (hEkey_ != nullptr)
    {
        unsigned long rv = 0;
        rv = SDKEY_CloseCard(hEkey_);
        if (rv)
        {
            DebugPrint("SDKEY_CloseCard error. rv=0x%08x.",rv);
        }
        else
        {
            DebugPrint("SDKEY_CloseCard OK!");
        }
        return rv;
    }
    DebugPrint("device handle is null.");
    return -1;
}

std::pair<unsigned char*, int> SDkey::getPublicKey()
{
    DebugPrint("pubKeyLen_:%u,%s", pubKeyLen_,pubKey_);
    if (pubKeyLen_ > PUBLIC_KEY_EXT_LEN - 1)
        return std::make_pair(nullptr, 0);
    else if (pubKey_[0] == 0x47)
    {
        return std::make_pair(pubKey_, pubKeyLen_);
    }
    else
    {
        return std::make_pair(nullptr, 0);
    }
}
std::pair<unsigned char*, int> SDkey::getPrivateKey()
{
    DebugPrint("priKeyLen_:%u,%s", priKeyLen_, priKey_);
    return std::make_pair(priKey_, priKeyLen_);
}
//Generate Random
unsigned long SDkey::GenerateRandom(unsigned int uiLength, unsigned char * pucRandomBuf)
{
	int rv;
	memset(pucRandomBuf, 0, sizeof(pucRandomBuf));
	rv = SDKEY_GenRandom(hEkey_, uiLength, pucRandomBuf);
	if (rv)
	{
		DebugPrint("Generate random failed, failed number:[0x%08x]\n", rv);
		return rv;
	}
	else
	{
		DebugPrint("Generate random successful!");
		return 0;
	}
}

//SM2 interface
//Generate Publick&Secret Key
unsigned long SDkey::SM2GenECCKeyPair(
    unsigned long ulAlias,
    unsigned long ulKeyUse,
    unsigned long ulModulusLen)
{
    unsigned long rv = 0;
    memset(pubKey_,0,sizeof(pubKey_));
    memset(priKey_,0,sizeof(priKey_));

    unsigned char pubKeyTemp[256] = { 0 };
    unsigned long pubKeyTempLen = 256;
    DebugPrint("hEkey_:%p,ulModulusLen:%u", hEkey_, ulModulusLen);
    rv = SDKEY_GenExtECCKeyPair(hEkey_, ulModulusLen, pubKeyTemp, &pubKeyTempLen, priKey_, &priKeyLen_);
    if (rv)
    {
        DebugPrint("GenECCKeyPair error! rv = [0x%04x]", rv);
    }
    else
    {
        DebugPrint("GenECCKeyPair OK!");
        if (pubKeyTempLen > PUBLIC_KEY_EXT_LEN - 1)
        {
            DebugPrint("Public Key length is bigger than 64!");
        }
        else
        {
            pubKeyLen_ = pubKeyTempLen + 1;
            pubKey_[0] = 0x47;
            memcpy(pubKey_ + 1, pubKeyTemp, pubKeyTempLen);
        }
    }
    return rv;
}
//SM2 Sign&Verify
unsigned long SDkey::SM2ECCSign(
    std::pair<unsigned char*, int>& pri4Sign,
    unsigned char *pInData,
    unsigned long ulInDataLen,
    unsigned char *pSignValue,
    unsigned long *pulSignValueLen,
    unsigned long ulAlias,
    unsigned long ulKeyUse)
{
    unsigned long rv = 0;

    rv = SDKEY_ExtECCSign(hEkey_, pri4Sign.first, pri4Sign.second, pInData, ulInDataLen, pSignValue, pulSignValueLen);
    if (rv)
    {
        DebugPrint("SM2-sdkey secret key sign failed, failed number:0x%04x", rv);
    }
    else
    {
        DebugPrint("SM2-sdkey secret key sign successful!");
    }
    return rv;
}
unsigned long SDkey::SM2ECCVerify(
    std::pair<unsigned char*, int>& pub4Verify,
    unsigned char *pInData,
    unsigned long ulInDataLen,
    unsigned char *pSignValue,
    unsigned long ulSignValueLen,
    unsigned long ulAlias,
    unsigned long ulKeyUse)
{
    unsigned long rv = 0;

    rv = SDKEY_ExtECCVerify(hEkey_, pub4Verify.first, pub4Verify.second, pInData, ulInDataLen, pSignValue, ulSignValueLen);
    if (rv)
    {
        DebugPrint("SM2-sdkey public key verify signature failed, failed number:0x%04x", rv);
    }
    else
    {
        DebugPrint("SM2-sdkey public keyverify signature successful!");
    }
    return rv;
}

//SM2 Encrypt&Decrypt
unsigned long SDkey::SM2ECCEncrypt(
    std::pair<unsigned char*, int>& pub4Encrypt,
    unsigned char * pPlainData,
    unsigned long ulPlainDataLen,
    unsigned char * pCipherData,
    unsigned long * pulCipherDataLen,
    unsigned long ulAlias,
    unsigned long ulKeyUse)
{
    unsigned long rv = 0;
    unsigned long ulConNum = 0, ulKeyUseLocal = 1;
    unsigned char outData[512] = { 0 };
    unsigned long outDataLen = 512;

    rv = SDKEY_ExtECCEncrypt(hEkey_, pub4Encrypt.first, pub4Encrypt.second, pPlainData, ulPlainDataLen, pCipherData, pulCipherDataLen);
    if (rv)
    {
        DebugPrint("ECCEncrypt error! rv = 0x%04x", rv);
    }
    else
    {
        DebugPrint("ECCEncrypt OK!");
    }
    return rv;
}
unsigned long SDkey::SM2ECCDecrypt(
    std::pair<unsigned char*, int>& pri4Decrypt,
    unsigned char *pCipherData,
    unsigned long ulCipherDataLen,
    unsigned char *pPlainData,
    unsigned long *pulPlainDataLen,
    unsigned long ulAlias,
    unsigned long ulKeyUse)
{
    unsigned long rv = 0;
    unsigned long ulConNum = 0, ulKeyUseLocal = 1;
    unsigned char plain[256] = { 0 };
    unsigned long plainLen = 256;

    rv = SDKEY_ExtECCDecrypt(hEkey_, pri4Decrypt.first, pri4Decrypt.second, pCipherData, ulCipherDataLen, pPlainData, pulPlainDataLen);
    if (rv)
    {
        DebugPrint("ECCDecrypt error! rv = 0x%04x", rv);
    }
    else
    {
        DebugPrint("ECCDecrypt OK! out data:%s", plain);
    }
    return rv;
}

//SM3 interface
unsigned long SDkey::SM3HashTotal(
    unsigned char *pInData,
    unsigned long ulInDataLen,
    unsigned char *pHashData,
    unsigned long *pulHashDataLen)
{
    unsigned long rv = 0;
    rv = SDKEY_Hash(hEkey_, SGD_SM3, pInData, ulInDataLen, pHashData, pulHashDataLen);
    if (rv)
    {
        DebugPrint("Hash error! rv = 0x%04x", rv);
    }
    else
    {
        DebugPrint("Hash OK!");
    }
    return rv;
}

unsigned long SDkey::SM3HashInit(HANDLE *phSM3Handle)
{
    unsigned long rv = 0;
    rv = SDKEY_HashInit(hEkey_, SGD_SM3, phSM3Handle);
    if (rv)
    {
        DebugPrint("SDKEY_HashInit failed, failed number:0x%04x", rv);
    }
    else
    {
        DebugPrint("SDKEY_HashInit OK!");
    }
    return rv;
}
unsigned long SDkey::SM3HashFinal(HANDLE phSM3Handle, unsigned char *pHashData, unsigned long *pulHashDataLen)
{
    unsigned long rv = 0;
    if(phSM3Handle != nullptr)
    {
        DebugPrint("phSM3Handle:%p,pHashData:%p,pulHashDataLen:%u", phSM3Handle, pHashData, *pulHashDataLen);
        rv = SDKEY_HashFinal(hEkey_, phSM3Handle, pHashData, pulHashDataLen);
        if (rv)
        {
            DebugPrint("SDKEY_HashFinal failed, failed number:0x%04x", rv);
        }
        else
        {
            DebugPrint("SM3 Hash success!");
        }
        return rv;
    }
    else
    {
        DebugPrint("Hash handle is nullptr! Please check!");
        return -1;
    }
}
void SDkey::operator()(HANDLE phSM3Handle, void const* data, std::size_t size) noexcept
{
    unsigned long rv = 0;
    if (phSM3Handle != nullptr)
    {
        DebugPrint("phSM3Handle:%p,size:%u", phSM3Handle, size);
        rv = SDKEY_HashUpdate(hEkey_, phSM3Handle, (unsigned char*)data, size);
        if (rv == 0)
        {
            DebugPrint("SDKEY_HashUpdate() success!");
        }
        else
        {
            DebugPrint("SDKEY_HashUpdate() failed, failed number:[0x%04x]", rv);
        }
    }
    else
    {
        DebugPrint("SessionHandle is null, please check!");
    }
}

unsigned long SM4SymEncryptECB(
	unsigned char *pSessionKey,
	unsigned long pSessionKeyLen,
	unsigned char *pPlainData,
	unsigned long ulPlainDataLen,
	unsigned char *pCipherData,
	unsigned long *pulCipherDataLen)
{
	int rv = 0;
	rv = SM4SymEncrypt(ALG_MOD_ECB, pSessionKey, pSessionKeyLen, pPlainData, ulPlainDataLen, pCipherData, pulCipherDataLen);
	return rv;
}
unsigned long SM4SymDecryptECB(
	unsigned char *pSessionKey,
	unsigned long pSessionKeyLen,
	unsigned char *pPlainData,
	unsigned long ulPlainDataLen,
	unsigned char *pCipherData,
	unsigned long *pulCipherDataLen)
{
	int rv = 0;
	rv = SM4SymDecrypt(ALG_MOD_ECB, pSessionKey, pSessionKeyLen, pPlainData, ulPlainDataLen, pCipherData, pulCipherDataLen);
	return rv;
}
unsigned long SM4SymEncryptCBC(
	unsigned char *pSessionKey,
	unsigned long pSessionKeyLen,
	unsigned char *pPlainData,
	unsigned long ulPlainDataLen,
	unsigned char *pCipherData,
	unsigned long *pulCipherDataLen)
{
	int rv = 0;
	rv = SM4SymEncrypt(ALG_MOD_CBC, pSessionKey, pSessionKeyLen, pPlainData, ulPlainDataLen, pCipherData, pulCipherDataLen);
	return rv;
}
unsigned long SM4SymDecryptCBC(
	unsigned char *pSessionKey,
	unsigned long pSessionKeyLen,
	unsigned char *pPlainData,
	unsigned long ulPlainDataLen,
	unsigned char *pCipherData,
	unsigned long *pulCipherDataLen)
{
	int rv = 0;
	rv = SM4SymDecrypt(ALG_MOD_CBC, pSessionKey, pSessionKeyLen, pPlainData, ulPlainDataLen, pCipherData, pulCipherDataLen);
	return rv;
}

unsigned long SDkey::generateIV(unsigned int uiAlgMode, unsigned char * pIV)
{
	int rv = 0;
	if (pIV != NULL)
	{
		switch (uiAlgMode)
		{
		case ALG_MOD_CBC:
			memset(pIV, 0x00, 16);
			rv = GenerateRandom(16, pIV);
			break;
		case ALG_MOD_ECB:
		default:
			memset(pIV, 0, 16);
			break;
		}
	}
	else rv = -1;
	return rv;
}

//SM4 Symetry Encrypt&Decrypt
unsigned long SDkey::SM4SymEncrypt(
	unsigned int uiAlgMode,
    unsigned char *pSessionKey,
    unsigned long pSessionKeyLen,
    unsigned char *pPlainData,
    unsigned long ulPlainDataLen,
    unsigned char *pCipherData,
    unsigned long *pulCipherDataLen)
{
    if (pSessionKeyLen > 16)
    {
        return -1;
    }
    else
    {
        unsigned long rv = 0;
        unsigned char pIv[16];
		generateIV(uiAlgMode, pIv);
        rv = SDKEY_SymEncrypt(hEkey_, ALG_SM4, uiAlgMode, pIv, pSessionKey, pPlainData, ulPlainDataLen, pCipherData, pulCipherDataLen);
        if (rv)
        {
            DebugPrint("SM4 symetry encrypt failed, failed number:[0x%04x]", rv);
        }
        else
        {
            DebugPrint("SM4 symetry encrypt successful!");
        }
        return rv;
    }
}
unsigned long SDkey::SM4SymDecrypt(
	unsigned int uiAlgMode,
    unsigned char *pSessionKey,
    unsigned long pSessionKeyLen,
    unsigned char *pCipherData,
    unsigned long ulCipherDataLen,
    unsigned char *pPlainData,
    unsigned long *pulPlainDataLen)
{
    if (pSessionKeyLen > 16)
    {
        return -1;
    }
    else
    {
        unsigned long rv = 0;
        unsigned char pIv[16];
		generateIV(uiAlgMode, pIv);
        rv = SDKEY_SymDecrypt(hEkey_, ALG_SM4, uiAlgMode, pIv, pSessionKey, pCipherData, ulCipherDataLen, pPlainData, pulPlainDataLen);
        if (rv)
        {
            DebugPrint("SM4 symmetry decrypt failed, failed number:[0x%04x]", rv);
        }
        else
        {
            DebugPrint("SM4 symmetry decrypt successful!");
        }
        return rv;
    }
}
unsigned long SDkey::SM4GenerateSessionKey(
    unsigned char *pSessionKey,
    unsigned long *pSessionKeyLen)
{
    unsigned long rv = 0;
    rv = SDKEY_GenRandom(hEkey_, 16, pSessionKey);
    if (rv)
    {
        DebugPrint("Generating SymmetryKey is failed, failed number:[0x%04x]", rv);
        pSessionKey = nullptr;
        pSessionKeyLen = nullptr;
    }
    else
    {
        DebugPrint("Generate SymmetryKey successfully!");
        *pSessionKeyLen = 16;
    }
    return rv;
}
unsigned long SDkey::SM4SymEncryptEx(
    unsigned char *pPlainData,
    unsigned long ulPlainDataLen,
    unsigned char *pSessionKey,
    unsigned long *pSessionKeyLen,
    unsigned char *pCipherData,
    unsigned long *pulCipherDataLen)
{
    unsigned long rv = 0;
    rv = SDKEY_GenRandom(hEkey_, 16, pSessionKey);
    if (rv)
    {
        DebugPrint("Generating SymmetryKey is failed, failed number:[0x%04x]", rv);
        pSessionKey = nullptr;
        pSessionKeyLen = nullptr;
    }
    else
    {
        DebugPrint("Generate SymmetryKey successfully!");
        *pSessionKeyLen = 16;
        SM4SymEncrypt(pSessionKey, *pSessionKeyLen, pPlainData, ulPlainDataLen, pCipherData, pulCipherDataLen);
    }
    return rv;
}

#endif
