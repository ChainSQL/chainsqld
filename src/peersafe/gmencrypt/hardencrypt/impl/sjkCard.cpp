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

#include <peersafe/gmencrypt/hardencrypt/sjkCard.h>
#include <peersafe/gmencrypt/GmCheck.h>
#include <cstdlib>
#include <iostream>
extern "C" {
#include <peersafe/gmencrypt/randomcheck/log.h>
}

#ifdef HARD_GM

unsigned long  SJKCard::OpenDevice()
{
    unsigned int rv = 0;
    rv = SDF_OpenDevice((SGD_HANDLE*)&hEkey_);
    if (rv)
    {
        DebugPrint("SDF_OpenDevice error. rv=0x%08x.",rv);
        return -1;
    }
    DebugPrint("SDF_OpenDevice OK!");

    SDF_OpenSession((SGD_HANDLE)hEkey_, (SGD_HANDLE*)&hSessionHandle_);
    return 0;
}
unsigned long  SJKCard::CloseDevice()
{
    if(!CloseSession(hSessionHandle_))
        return -1;
    if (hEkey_ != nullptr)
    {
        unsigned int rv = 0;
        rv = SDF_CloseDevice(hEkey_);
        if (rv)
        {
            DebugPrint("SDF_CloseDevice error. rv=0x%08x.",rv);
            return -1;
        }
        DebugPrint("SDF_CloseDevice OK!");
        return 0;
    }
    DebugPrint("device handle is null.");
    return -1;
}
unsigned long SJKCard::OpenSession(HANDLE hKey, SGD_HANDLE *phSessionHandle)
{
    int rv;
    rv = SDF_OpenSession(hKey, phSessionHandle);
    if (rv != SDR_OK)
    {
        DebugPrint("Open session failed, failed number:[0x%08x]", rv);
    }
    return rv;
}
unsigned long  SJKCard::CloseSession(HANDLE hSession)
{
    int rv;
    if (hSession != nullptr)
    {
        rv = SDF_CloseSession(hSession);
        return rv;
    }
    else
    {
        DebugPrint("close hSession error! Please check");
        return -1;
    }
}

bool SJKCard::getPublicKey(ECCrefPublicKey& pubKeyUserExt, std::vector<unsigned char> pubKeyV)
{
    pubKeyV.push_back(GM_ALG_MARK);
    pubKeyV.insert(pubKeyV.end(), sizeof(originalPublicKey.x), originalPublicKey.x);
    pubKeyV.insert(pubKeyV.end(), sizeof(originalPublicKey.y), originalPublicKey.y);
    return true;
}
bool SJKCard::getPrivateKey(ECCrefPrivateKey& priKeyUserExt, std::vector<unsigned char> priKeyV)
{
    priKeyV.insert(priKey.begin(), priKeyUserExt.bits/8, priKeyUserExt.D);
    return true;
    // return std::make_pair(priKeyUserExt_.D,sizeof(priKeyUserExt_.D));
}
void SJKCard::mergePublicXYkey(unsigned char* publickey, ECCrefPublicKey& originalPublicKey)
{
    if (GM_ALG_MARK == publickey[0])
        return;
    else
    {
        publickey[0] = GM_ALG_MARK;
        memcpy(publickey + 1, originalPublicKey.x, sizeof(originalPublicKey.x));
        memcpy(publickey + 33, originalPublicKey.y, sizeof(originalPublicKey.y));
    }
}
unsigned long SJKCard::GenerateRandom(unsigned int uiLength, unsigned char * pucRandomBuf)
{
	int rv;
	memset(pucRandomBuf, 0, sizeof(pucRandomBuf));
	rv = SDF_GenerateRandom(hSessionHandle_, uiLength, pucRandomBuf);
	if (rv != SDR_OK)
	{
		//DebugPrint("Generate random failed, failed number:[0x%08x]\n", rv);
		LOGP(LOG_ERROR, rv, "Generate random failed");
		return rv;
	}
	else
	{
		//DebugPrint("Generate random successful!");
		LOGP(LOG_INFO, 0, "Generate random successful!");
		return 0;
	}
}
unsigned long SJKCard::GenerateRandom2File(unsigned int uiLength, unsigned char * pucRandomBuf,int times)
{
	int rv, tmpLen;
	unsigned char pbtmpBuffer[1024];
	unsigned char pbkeyBuffer[16];
	unsigned char pbivBuffer[16];
	SGD_HANDLE hKey;
	memset(pbtmpBuffer, 0x00, 1024);
	rv = SDF_GenerateRandom(hSessionHandle_, 1024, pbtmpBuffer); //pbOutBuffer+(j*1024));
	if (rv != SDR_OK)
	{
		printf("Generate random failed, failed number:[0x%08x]\n", rv);
		return 0;
	}
	//Generate Key
	memset(pbkeyBuffer, 0x00, 16);
	rv = SDF_GenerateRandom(hSessionHandle_, 16, pbkeyBuffer); //pbOutBuffer+(j*1024));
	if (rv != SDR_OK)
	{
		printf("Generate random failed, failed number:[0x%08x]\n", rv);
		return 0;
	}
	//Generate IV
	memset(pbivBuffer, 0x00, 16);
	rv = SDF_GenerateRandom(hSessionHandle_, 16, pbivBuffer); //pbOutBuffer+(j*1024));
	if (rv != SDR_OK)
	{
		printf("Generate random failed, failed number:[0x%08x]\n", rv);
		return 0;
	}
	//sm4 encrypt

	rv = SDF_ImportKey(hSessionHandle_, pbkeyBuffer, 16, &hKey);
	if (rv != SDR_OK)
	{
		printf("Importing SymmetryKey is failed, failed number:[0x%08x]\n", rv);
		return 0;
	}
	tmpLen = 1024;
	rv = SDF_Encrypt(hSessionHandle_, hKey, SGD_SMS4_CBC, pbivBuffer, pbtmpBuffer, 1024, pucRandomBuf + (times * 1024), (SGD_UINT32 *)&tmpLen);
	if (rv != SDR_OK)
	{
		printf("SM4 symetry encrypt failed, failed number:[0x%08x]\n", rv);
		SDF_DestroyKey(hSessionHandle_, hKey);
		return 0;
	}
	SDF_DestroyKey(hSessionHandle_, hKey);
	return 0;
}

bool SJKCard::randomSingleCheck(unsigned long randomCheckLen)
{
    return GMCheck::getInstance()->randomSingleCheck(randomCheckLen);
}

unsigned long SJKCard::getPrivateKeyRight(unsigned int uiKeyIndex, unsigned char * pucPassword, unsigned int uiPwdLength)
{
	int rv;
	rv = SDF_GetPrivateKeyAccessRight(hSessionHandle_, uiKeyIndex, (unsigned char*)priKeyAccessCode_g.c_str(), priKeyAccessCode_g.size());
	if (rv != SDR_OK)
	{
		DebugPrint("Get PrivateKey Access Right failed, failed number:[0x%08x]", rv);
		return rv;
	}
	else
	{
		DebugPrint("Get PrivateKey Access Right successful!");
		return 0;
	}
	return 0;
}

unsigned long SJKCard::releasePrivateKeyRight(unsigned int uiKeyIndex)
{
	int rv;
	rv = SDF_ReleasePrivateKeyAccessRight(hSessionHandle_, uiKeyIndex);
	if (rv != SDR_OK)
	{
		DebugPrint("Release PrivateKey Access Right failed, failed number:[0x%08x]", rv);
		return rv;
	}
	else
	{
		DebugPrint("Release PrivateKey Access Right successful!");
		return 0;
	}
	return 0;
}
std::pair<unsigned char*, int> SJKCard::getECCSyncTablePubKey(unsigned char* publicKeyTemp)
{
	int rv = 0;
	ECCrefPublicKey pucPublicKey;
	rv = SDF_ExportEncPublicKey_ECC(hSessionHandle_, SYNC_TABLE_KEY_INDEX, &pucPublicKey);
	if (rv != SDR_OK)
	{
		DebugPrint("Get syncTable public key failed, failed number:[0x%08x]", rv);
		return std::make_pair(nullptr,0);
	}
	else
	{
		DebugPrint("Get syncTable public key successful!");
		mergePublicXYkey(publicKeyTemp, pucPublicKey);
		return std::make_pair(publicKeyTemp, PUBLIC_KEY_EXT_LEN);
	}
}
std::pair<unsigned char*, int> SJKCard::getECCNodeVerifyPubKey(unsigned char* publicKeyTemp, int keyIndex)
{
	int rv = 0;
	ECCrefPublicKey pucPublicKey;
	rv = SDF_ExportSignPublicKey_ECC(hSessionHandle_, keyIndex, &pucPublicKey);
	if (rv != SDR_OK)
	{
		DebugPrint("Get nodeVerify public key failed, failed number:[0x%08x]", rv);
		return std::make_pair(nullptr,0);
	}
	else
	{
		DebugPrint("Get nodeVerify public key successful!");
		mergePublicXYkey(publicKeyTemp, pucPublicKey);
		return std::make_pair(publicKeyTemp, PUBLIC_KEY_EXT_LEN);
	}
}

//SM2 interface
//Generate Publick&Secret Key
unsigned long SJKCard::SM2GenECCKeyPair(
    std::vector<unsigned char>& publicKey,
    std::vector<unsigned char>& privateKey,
    bool isRoot = false,
    unsigned long ulAlias,
    unsigned long ulKeyUse,
    unsigned long ulModulusLen)
{
    int ret = 0;
    if(isRoot)
    {
        getRootPublicKey(publicKey);
        getRootPrivateKey(privateKey);
    }
    else
    {
        ECCrefPublicKey pubKeyUserExt;
        ECCrefPrivateKey priKeyUserExt;
        ret = SDF_GenerateKeyPair_ECC(hSessionHandle_, SGD_SM2_3, ulModulusLen, &pubKeyUserExt, &priKeyUserExt);

        if (ret != SDR_OK)
        {
            DebugPrint("Generate ECC key pair failed, failed number:[0x%08x]", rv);
        }
        else
        {
            if (!getPublicKey(&pubKeyUserExt, publicKey) || !getPrivateKey(&priKeyUserExt, privateKey))
                ret = -1;
            else
            {
                DebugPrint("Generate ECC key pair successful!");
            }
        }
    }
    return ret;
}
bool SJKCard::generatePubFromPri(
    const unsigned char* pPriUC,
    int priLen,
    std::vector<unsigned char>& publicKey)
{
    return false;
}
//SM2 Sign&Verify
unsigned long SJKCard::SM2ECCSign(
	std::pair<int, int> pri4SignInfo,
    std::pair<unsigned char*, int>& pri4Sign,
    unsigned char *pInData,
    unsigned long ulInDataLen,
    std::vector<unsigned char>& signedDataV)
{
	//according key type to determine is inner key or external key to call interface
	int rv;
	if (SeckeyType::gmInCard == pri4SignInfo.first)
	{
		rv = SM2ECCInternalSign(pri4SignInfo.second, pInData, ulInDataLen, signedDataV);
	}
	else if (SeckeyType::gmOutCard == pri4SignInfo.first)
	{
		rv = SM2ECCExternalSign(pri4Sign, pInData, ulInDataLen, signedDataV);
	}
	else return 1;
    
    if (rv != SDR_OK)
    {
        DebugPrint("SM2 secret key sign failed, failed number:[0x%08x]", rv);
    }
    else
    {
        DebugPrint("SM2 secret key sign successful!");
    }
	return rv;
}

unsigned long SJKCard::SM2ECCExternalSign(
	std::pair<unsigned char*, int>& pri4Sign,
	unsigned char *pInData,
	unsigned long ulInDataLen,
	std::vector<unsigned char>& signedDataV)
{
	int rv;
	ECCrefPrivateKey pri4SignTemp;
    ECCSignature signedDataTemp;
	standPriToSM2Pri(pri4Sign.first, pri4Sign.second, pri4SignTemp);
	rv = SDF_ExternalSign_ECC(hSessionHandle_, SGD_SM2_1, &pri4SignTemp, pInData, pri4SignTemp.bits / 8, &signedDataTemp);
	if (rv != SDR_OK)
	{
		DebugPrint("SM2 external secret key sign failed, failed number:[0x%08x]", rv);
	}
	else
	{
        std::vector<unsigned char> signedDataTempV((unsigned char*)&signedDataTemp, (unsigned char*)&signedDataTemp + 64);
        signedDataV.assign(signedDataTempV.begin(), signedDataTempV.end());
		DebugPrint("SM2 external secret key sign successful!");
	}
	return rv;
}

unsigned long SJKCard::SM2ECCInternalSign(
	int pri4SignIndex,
	unsigned char *pInData,
	unsigned long ulInDataLen,
	std::vector<unsigned char>& signedDataV)
{
	int rv;
    ECCSignature signedDataTemp;
	rv = SDF_InternalSign_ECC(hSessionHandle_, pri4SignIndex, pInData, ulInDataLen, &signedDataTemp);
	if (rv != SDR_OK)
	{
		DebugPrint("SM2 internal secret key sign failed, failed number:[0x%08x]", rv);
	}
	else
	{
        std::vector<unsigned char> signedDataTempV((unsigned char*)&signedDataTemp, (unsigned char*)&signedDataTemp + 64);
        signedDataV.assign(signedDataTempV.begin(), signedDataTempV.end());
		DebugPrint("SM2 internal secret key sign successful!");
	}
	return rv;
}

unsigned long SJKCard::SM2ECCVerify(
    std::pair<unsigned char*, int>& pub4Verify,
    unsigned char *pInData,
    unsigned long ulInDataLen,
    unsigned char *pSignValue,
    unsigned long ulSignValueLen)
{
    int rv;
    ECCrefPublicKey pub4VerifyTemp;
	SGD_HANDLE pSM2VerifySesHandle;
	OpenSession(hEkey_, &pSM2VerifySesHandle);
    standPubToSM2Pub(pub4Verify.first, pub4Verify.second, pub4VerifyTemp);
    rv = SDF_ExternalVerify_ECC(pSM2VerifySesHandle, SGD_SM2_1, &pub4VerifyTemp, pInData, PUBLIC_KEY_BIT_LEN / 8, (ECCSignature *)pSignValue);
    if (rv != SDR_OK)
    {
        DebugPrint("SM2 public key verify signature failed, failed number[0x%08x]", rv);
    }
    else
    {
        DebugPrint("SM2 public key verify signature successful!");
    }
	CloseSession(pSM2VerifySesHandle);
	return rv;
}
//SM2 Encrypt&Decrypt
unsigned long SJKCard::SM2ECCEncrypt(
    std::pair<unsigned char*, int>& pub4Encrypt,
    unsigned char * pPlainData,
    unsigned long ulPlainDataLen,
    std::vector<unsigned char>& cipherDataV)
{
    int rv;
    ECCrefPublicKey pub4EncryptTemp;
    standPubToSM2Pub(pub4Encrypt.first, pub4Encrypt.second, pub4EncryptTemp);
    ECCCipher pEccCipher;
    rv = SDF_ExternalEncrypt_ECC(hSessionHandle_, SGD_SM2_3, &pub4EncryptTemp, pPlainData, ulPlainDataLen, &pEccCipher);
    if (rv != SDR_OK)
    {
        DebugPrint("SM2 public key encrypt failed, failed number:[0x%08x]", rv);
    }
    else
    {
        std::vector<unsigned char> cipherDataVTemp((unsigned char*)&pEccCipher, (unsigned char*)&pEccCipher + CARD_CIPHER_LEN);
        cipherDataV.assign(cipherDataVTemp.begin(), cipherDataVTemp.end());
        DebugPrint("SM2 public key encrypt successful!");
    }
	return rv;
}
unsigned long SJKCard::SM2ECCDecrypt(
	std::pair<int, int> pri4DecryptInfo,
    std::pair<unsigned char*, int>& pri4Decrypt,
    unsigned char *pCipherData,
    unsigned long ulCipherDataLen,
    std::vector<unsigned char>& plainDataV,
	bool isSymmertryKey,
    void* sm4Handle)
{
    int rv;
	if (SeckeyType::gmInCard == pri4DecryptInfo.first)
	{
		if (isSymmertryKey)
		{
			rv = SM2ECCInterDecryptSyncKey(pri4DecryptInfo.second, pCipherData, ulCipherDataLen, sm4Handle);
		}
		else
		{
			rv = SM2ECCInternalDecrypt(pri4DecryptInfo.second, pCipherData, ulCipherDataLen, plainDataV);
		}
	}
	else if (SeckeyType::gmOutCard == pri4DecryptInfo.first)
	{
		rv = SM2ECCExternalDecrypt(pri4Decrypt, pCipherData, ulCipherDataLen, plainDataV);
	}
	else return 1;
    if (rv != SDR_OK)
    {
        DebugPrint("SM2 secret key decrypt failed, failed number:[0x%08x]", rv);
    }
    else
    {
        DebugPrint("SM2 secret key decrypt successful!");
    }
	return rv;
}

unsigned long SJKCard::SM2ECCExternalDecrypt(
	std::pair<unsigned char*, int>& pri4Decrypt,
	unsigned char *pCipherData,
	unsigned long ulCipherDataLen,
    std::vector<unsigned char>& plainDataV)
{
	int rv;
	ECCrefPrivateKey pri4DecryptTemp;
    unsigned char* pCardCipher;
	standPriToSM2Pri(pri4Decrypt.first, pri4Decrypt.second, pri4DecryptTemp);
    if(pCipherData[1] != 0 || pCipherData[2] != 0 || pCipherData[3] != 0) //there is no mark for soft encrypt,so use this judge way
    {
        pCardCipher = new unsigned char[CARD_CIPHER_LEN];
        c1c2c3ToCardCipher(pCardCipher, CARD_CIPHER_LEN, pCipherData, ulCipherDataLen);
        // rv = SDF_ExternalDecrypt_ECC(hSessionHandle_, SGD_SM2_3, &pri4DecryptTemp, (ECCCipher *)pCardCipher, pPlainData, (SGD_UINT32*)pulPlainDataLen);
    }
    else
    {
        pCardCipher = pCipherData;
    }

    SGD_UINT32 plainDataLen;
    rv = SDF_ExternalDecrypt_ECC(hSessionHandle_, SGD_SM2_3, &pri4DecryptTemp, (ECCCipher *)pCardCipher, nullptr, &plainDataLen);
    if (rv != SDR_OK)
    {
        DebugPrint("SM2 External secret key decrypt failed, failed number:[0x%08x]", rv);
    }
    else
    {
        SGD_UCHAR *pPlainData = new SGD_UCHAR[plainDataLen];
        rv = SDF_ExternalDecrypt_ECC(hSessionHandle_, SGD_SM2_3, &pri4DecryptTemp, (ECCCipher *)pCardCipher, pPlainData, &plainDataLen);
        if (!rv)
        {
            std::vector<unsigned char> plainDataVTemp(pPlainData, pPlainData + plainDataLen);
            plainDataV.assign(plainDataVTemp.begin(), plainDataVTemp.end());
            DebugPrint("SM2 External secret key decrypt successful!");
        }
        else
            DebugPrint("SM2 External secret key decrypt failed, failed number:[0x%08x]", rv);
        delete[] pPlainData;
    }

	return rv;
}
unsigned long SJKCard::SM2ECCInternalDecrypt(
	int pri4DecryptIndex,
	unsigned char *pCipherData,
	unsigned long ulCipherDataLen,
	std::vector<unsigned char>& plainDataV)
{
	int rv;
    SGD_UINT32 plainDataLen;
	rv = SDF_InternalDecrypt_ECC(hSessionHandle_, pri4DecryptIndex, SGD_SM2_3,  (ECCCipher *)pCipherData, nullptr, &plainDataLen);
	if (rv != SDR_OK)
	{
		DebugPrint("SM2 Internal secret key decrypt failed, failed number:[0x%08x]", rv);
	}
	else
	{
        SGD_UCHAR* pPlainData = new SGD_UCHAR[plainDataLen];
    	rv = SDF_InternalDecrypt_ECC(hSessionHandle_, pri4DecryptIndex, SGD_SM2_3,  (ECCCipher *)pCipherData, pPlainData, &plainDataLen);
        if(!rv)
        {
            std::vector<unsigned char> plainDataVTemp(pPlainData, pPlainData + plainDataLen);
            plainDataV.assign(plainDataVTemp.begin(), plainDataVTemp.end());
            DebugPrint("SM2 Internal secret key decrypt successful!");
        }
		else DebugPrint("SM2 Internal secret key decrypt failed, failed number:[0x%08x]", rv);
        delete [] pPlainData;
	}
	return rv;
}
unsigned long SJKCard::SM2ECCInterDecryptSyncKey(
	int pri4DecryptIndex,
	unsigned char * pCipherData, 
	unsigned long ulCipherDataLen,
	void *hSM4Key)
{
	int rv;
	void *pHandle= nullptr;
	rv = SDF_ImportKeyWithISK_ECC(hSessionHandle_, pri4DecryptIndex, (ECCCipher *)pCipherData, &pHandle);
	if (rv != SDR_OK)
	{
		DebugPrint("SM2 internal secret key decrypt SyncKey failed, failed number:[0x%08x]", rv);
	}
	else
	{
		DebugPrint("SM2 inernal secret key decrypt SyncKey successful!");
		*((unsigned long*)(hSM4Key)) = (unsigned long)pHandle;
	}
	return rv;
}
//SM3 interface
unsigned long SJKCard::SM3HashTotal(
    unsigned char *pInData,
    unsigned long ulInDataLen,
    unsigned char *pHashData,
    unsigned long *pulHashDataLen)
{
    int rv;
	SGD_HANDLE pSM3TotalSesHandle;
	OpenSession(hEkey_, &pSM3TotalSesHandle);

    rv = SDF_HashInit(pSM3TotalSesHandle, SGD_SM3, NULL, NULL, 0);
    if (rv == SDR_OK)
    {
        rv = SDF_HashUpdate(pSM3TotalSesHandle, pInData, ulInDataLen);
        if (rv == SDR_OK)
        {
            rv = SDF_HashFinal(pSM3TotalSesHandle, pHashData, (SGD_UINT32*)pulHashDataLen);
            if (rv == SDR_OK)
            {
                DebugPrint("SM3HashTotal successful!");
            }
            else
            {
                DebugPrint("SM3HashTotal failed, failed number:[0x%08x]", rv);
            }
        }
        else
        {
            DebugPrint("SM3HashTotal update failed, failed number:[0x%08x]", rv);
        }
    }
    else
    {
        DebugPrint("SM3HashTotal initiate failed, failed number:[0x%08x]", rv);
    }
	CloseSession(pSM3TotalSesHandle);
    return rv;
}
unsigned long SJKCard::SM3HashInit(SGD_HANDLE *phSM3Handle)
{
    int rv;
    OpenSession(hEkey_, phSM3Handle);
    rv = SDF_HashInit(*phSM3Handle, SGD_SM3, NULL, NULL, 0);
    if (rv == SDR_OK)
    {
        DebugPrint("SDF_HashInit OK!");
    }
    else
    {
        DebugPrint("SDF_HashInit() failed, failed number:[0x%08x]", rv);
    }
    return rv;
}

unsigned long SJKCard::SM3HashFinal(SGD_HANDLE phSM3Handle, unsigned char *pHashData, unsigned long *pulHashDataLen)
{
    int rv;
    if (nullptr != phSM3Handle)
    {
        rv = SDF_HashFinal(phSM3Handle, pHashData, (SGD_UINT32*)pulHashDataLen);
        if (rv == SDR_OK)
        {
            DebugPrint("SM3 Hash success!");
        }
        else
        {
            DebugPrint("SDF_HashFinal() failed, failnumber:[0x%08x]", rv);
        }
        CloseSession(phSM3Handle);
        return rv;
    }
    else
    {
        DebugPrint("SessionHandle is null, please check!");
        return -1;
    }
}
void SJKCard::operator()(SGD_HANDLE phSM3Handle, void const* data, std::size_t size) noexcept
{
    int rv;
    if (nullptr != phSM3Handle)
    {
        rv = SDF_HashUpdate(phSM3Handle, (SGD_UCHAR*)data, size);
        if (rv == SDR_OK)
        {
            DebugPrint("SDF_HashUpdate() success!");
        }
        else
        {
            DebugPrint("SDF_HashUpdate() failed, failed number:[0x%08x]", rv);
        }
    }
    else
    {
        DebugPrint("SessionHandle is null, please check!");
    }
}

unsigned long SJKCard::SM4SymEncrypt(unsigned int uiAlgMode, unsigned char * pSessionKey, unsigned long pSessionKeyLen,  unsigned char * pPlainData, unsigned long ulPlainDataLen, unsigned char * pCipherData, unsigned long * pulCipherDataLen, int secKeyType)
{
	int rv = 0;
	if (SeckeyType::gmInCard == secKeyType)
	{
		void* hSM4Handle = (void*)pSessionKey;
		if (SM4AlgType::ECB == uiAlgMode)
		{
			rv = SM4InternalSymEncrypt(SGD_SMS4_ECB, hSM4Handle, pPlainData, ulPlainDataLen, pCipherData, pulCipherDataLen);
		}
		else if (SM4AlgType::CBC == uiAlgMode)
		{
			rv = SM4InternalSymEncrypt(SGD_SMS4_CBC, hSM4Handle, pPlainData, ulPlainDataLen, pCipherData, pulCipherDataLen);
		}
	}
	else
	{
		if (SM4AlgType::ECB == uiAlgMode)
		{
			rv = SM4ExternalSymEncrypt(SGD_SMS4_ECB, pSessionKey, pSessionKeyLen, pPlainData, ulPlainDataLen, pCipherData, pulCipherDataLen);
		}
		else if (SM4AlgType::CBC == uiAlgMode)
		{
			rv = SM4ExternalSymEncrypt(SGD_SMS4_CBC, pSessionKey, pSessionKeyLen, pPlainData, ulPlainDataLen, pCipherData, pulCipherDataLen);
		}
	}
	
	return rv;
}

unsigned long SJKCard::SM4SymDecrypt(unsigned int uiAlgMode, unsigned char * pSessionKey, unsigned long pSessionKeyLen, unsigned char * pCipherData, unsigned long ulCipherDataLen, unsigned char * pPlainData, unsigned long * pulPlainDataLen, int secKeyType)
{
	int rv = 0;
	if (SeckeyType::gmInCard == secKeyType)
	{
		void* hSM4Handle = (void*)pSessionKey;
		if (SM4AlgType::ECB == uiAlgMode)
		{
			rv = SM4InternalSymDecrypt(SGD_SMS4_ECB, hSM4Handle, pCipherData, ulCipherDataLen, pPlainData, pulPlainDataLen);
		}
		else if (SM4AlgType::CBC == uiAlgMode)
		{
			rv = SM4InternalSymDecrypt(SGD_SMS4_CBC, hSM4Handle, pCipherData, ulCipherDataLen, pPlainData, pulPlainDataLen);
		}
	}
	else
	{
		if (SM4AlgType::ECB == uiAlgMode)
		{
			rv = SM4ExternalSymDecrypt(SGD_SMS4_ECB, pSessionKey, pSessionKeyLen, pCipherData, ulCipherDataLen, pPlainData, pulPlainDataLen);
		}
		else if (SM4AlgType::CBC == uiAlgMode)
		{
			rv = SM4ExternalSymDecrypt(SGD_SMS4_CBC, pSessionKey, pSessionKeyLen, pCipherData, ulCipherDataLen, pPlainData, pulPlainDataLen);
		}
	}
	return rv;
}

//SM4 Symetry Encrypt&Decrypt
unsigned long SJKCard::generateIV(unsigned int uiAlgMode, unsigned char * pIV)
{
	int rv = 0;
	if (pIV != NULL)
	{
		switch (uiAlgMode)
		{
		case SGD_SMS4_CBC:
			memset(pIV, 0x00, 16);
			rv = SDF_GenerateRandom(hSessionHandle_, 16, pIV);
			if (rv != SDR_OK)
			{
				printf("Generate random failed, failed number:[0x%08x]\n", rv);
			}
			break;
		case SGD_SMS4_ECB:
		default:
			memset(pIV, 0, 16);
			break;
		}
	}
	else rv = -1;
	return rv;
}
unsigned long SJKCard::SM4ExternalSymEncrypt(
	unsigned int uiAlgMode,
    unsigned char *pSessionKey,
    unsigned long pSessionKeyLen,
    unsigned char *pPlainData,
    unsigned long ulPlainDataLen,
    unsigned char *pCipherData,
    unsigned long *pulCipherDataLen)
{
    int rv;  //Candidate algorithm:SGD_SMS4_ECB,SGD_SMS4_CBC,SGD_SM1_ECB,SGD_SM1_CBC
    SGD_HANDLE hSM4EnHandle;
    SGD_HANDLE hSessionKeyEnHandle = nullptr;
	unsigned char pIv[16];
	generateIV(uiAlgMode, pIv);
    OpenSession(hEkey_, &hSM4EnHandle);
    unsigned char pIndata[16384];
    unsigned long nInlen = 1024;
    pkcs5Padding(pPlainData, ulPlainDataLen, pIndata, &nInlen);
    //rv = SDF_ImportKeyWithKEK(hSM4EnHandle, SGD_SMS4_ECB, SESSION_KEY_INDEX, pSessionKey, pSessionKeyLen, &hSessionKeyEnHandle);
    rv = SDF_ImportKey(hSM4EnHandle, pSessionKey, pSessionKeyLen, &hSessionKeyEnHandle);
    if(rv == SDR_OK)//if (nullptr != hSessionKeyHandle)
    {
        rv = SDF_Encrypt(hSM4EnHandle, hSessionKeyEnHandle, uiAlgMode, pIv, pIndata, nInlen, pCipherData, (SGD_UINT32*)pulCipherDataLen);
        if (rv == SDR_OK)
        {
            DebugPrint("SM4 symetry encrypt successful!");
        }
        else
        {
            DebugPrint("SM4 symetry encrypt failed, failed number:[0x%08x]", rv);
        }
    }
    else
    {
        DebugPrint("Importing SymmetryKey is failed, failed number:[0x%08x]", rv);
        //DebugPrint("SessionKey handle is unavailable,please check whether it was generated or imported.\n");
    }

    if (nullptr != hSessionKeyEnHandle)
    {
        rv = SDF_DestroyKey(hSM4EnHandle, hSessionKeyEnHandle);
        if (rv != SDR_OK)
        {
            DebugPrint("Destroy SessionKey failed, failed number:[0x%08x]", rv);
        }
        else
        {
            hSessionKeyEnHandle = NULL;
            DebugPrint("Destroy SessionKey successfully!");
            CloseSession(hSM4EnHandle);
        }
    }
    return rv;
}

unsigned long SJKCard::SM4ExternalSymDecrypt(
	unsigned int uiAlgMode,
    unsigned char *pSessionKey,
    unsigned long pSessionKeyLen,
    unsigned char *pCipherData,
    unsigned long ulCipherDataLen,
    unsigned char *pPlainData,
    unsigned long *pulPlainDataLen)
{
    int rv;
    SGD_HANDLE hSM4DeHandle;
    SGD_HANDLE hSessionKeyDeHandle = nullptr;
	unsigned char pIv[16];
	generateIV(uiAlgMode, pIv);
    OpenSession(hEkey_, &hSM4DeHandle);
    //rv = SDF_ImportKeyWithKEK(hSM4DeHandle, SGD_SMS4_ECB, SESSION_KEY_INDEX, pSessionKey, pSessionKeyLen, &hSessionKeyDeHandle);
    rv = SDF_ImportKey(hSM4DeHandle, pSessionKey, pSessionKeyLen, &hSessionKeyDeHandle);
    if (rv == SDR_OK)//if (nullptr != hSessionKeyHandle_)
    {
        unsigned char pOutdata[16384];
        unsigned long nOutlen = 1024;
        rv = SDF_Decrypt(hSM4DeHandle, hSessionKeyDeHandle, uiAlgMode, pIv, pCipherData, ulCipherDataLen, pOutdata, (SGD_UINT32*)&nOutlen);
        if (rv == SDR_OK)
        {
            dePkcs5Padding(pOutdata, nOutlen, pPlainData, pulPlainDataLen);
            DebugPrint("SM4 symmetry decrypt successful!");
        }
        else
        {
            DebugPrint("SM4 symmetry decrypt failed, failed number:[0x%08x]", rv);
        }
    }
    else
    {
        DebugPrint("Importing SymmetryKey is failed, failed number:[0x%08x]", rv);
        //DebugPrint("SessionKey handle is unavailable,please check whether it was generated or imported.\n");
    }
    if (nullptr != hSessionKeyDeHandle)
    {
        rv = SDF_DestroyKey(hSM4DeHandle, hSessionKeyDeHandle);
        if (rv != SDR_OK)
        {
            DebugPrint("Destroy SessionKey failed, failed number:[0x%08x]", rv);
        }
        else
        {
            hSessionKeyDeHandle = NULL;
            DebugPrint("Destroy SessionKey successfully!");
            CloseSession(hSM4DeHandle);
        }
    }  
    return rv;
}

unsigned long SJKCard::SM4InternalSymEncrypt(unsigned int uiAlgMode, void * hSM4Handle, unsigned char * pPlainData, unsigned long ulPlainDataLen, unsigned char * pCipherData, unsigned long * pulCipherDataLen)
{
	int rv;  //Candidate algorithm:SGD_SMS4_ECB,SGD_SMS4_CBC,SGD_SM1_ECB,SGD_SM1_CBC
	unsigned char pIv[16];
	generateIV(uiAlgMode, pIv);
	unsigned char pIndata[16384];
	unsigned long nInlen = 1024;
	pkcs5Padding(pPlainData, ulPlainDataLen, pIndata, &nInlen);
	if (nullptr != hSessionHandle_)
	{
		rv = SDF_Encrypt(hSessionHandle_, hSM4Handle, uiAlgMode, pIv, pIndata, nInlen, pCipherData, (SGD_UINT32*)pulCipherDataLen);
		if (rv == SDR_OK)
		{
			DebugPrint("SM4 symetry inner encrypt successful!");
		}
		else
		{
			DebugPrint("SM4 symetry inner encrypt failed, failed number:[0x%08x]", rv);
		}
	}
	else
	{
		DebugPrint("SM4 symetry inner encrypt failed, hSessionHandle is null");
		//DebugPrint("SessionKey handle is unavailable,please check whether it was generated or imported.\n");
	}
	return rv;
}

unsigned long SJKCard::SM4InternalSymDecrypt(unsigned int uiAlgMode, void * hSM4Handle, unsigned char * pCipherData, unsigned long ulCipherDataLen, unsigned char * pPlainData, unsigned long * pulPlainDataLen)
{
	int rv;
	unsigned char pIv[16];
	generateIV(uiAlgMode, pIv);
	if (nullptr != hSessionHandle_)
	{
		unsigned char pOutdata[16384];
		unsigned long nOutlen = 1024;
		rv = SDF_Decrypt(hSessionHandle_, hSM4Handle, uiAlgMode, pIv, pCipherData, ulCipherDataLen, pOutdata, (SGD_UINT32*)&nOutlen);
		if (rv == SDR_OK)
		{
			dePkcs5Padding(pOutdata, nOutlen, pPlainData, pulPlainDataLen);
			DebugPrint("SM4 symmetry inner decrypt successful!");
		}
		else
		{
			DebugPrint("SM4 symmetry inner decrypt failed, failed number:[0x%08x]", rv);
		}
	}
	else
	{
		DebugPrint("SM4 symmetry inner decrypt failed, hSessionHandle_ is null");
	}
	return rv;
}

unsigned long SJKCard::SM4GenerateSessionKey(
    unsigned char *pSessionKey,
    unsigned long *pSessionKeyLen)
{
    int rv;
    SGD_HANDLE hSM4GeHandle;
    SGD_HANDLE hSessionKeyGeHandle;
    OpenSession(hEkey_, &hSM4GeHandle);
    rv = SDF_GenerateKeyWithKEK(hSM4GeHandle, SESSION_KEY_LEN * 8, SGD_SMS4_ECB, SESSION_KEY_INDEX, pSessionKey, (SGD_UINT32*)pSessionKeyLen, &hSessionKeyGeHandle);
    if (rv != SDR_OK)
    {
        if (hSessionKeyGeHandle != NULL)
        {
            hSessionKeyGeHandle = NULL;
        }
        DebugPrint("Generating SymmetryKey is failed, failed number:[0x%08x]", rv);
    }
    else
    {
        DebugPrint("Generate SymmetryKey successfully!");
    }
    rv = SDF_DestroyKey(hSM4GeHandle, hSessionKeyGeHandle);
    if (rv != SDR_OK)
    {
        DebugPrint("Destroy SessionKey failed, failed number:[0x%08x]", rv);
    }
    else
    {
        hSessionKeyGeHandle = NULL;
        DebugPrint("Destroy SessionKey successfully!");
        CloseSession(hSM4GeHandle);
    }
    return rv;
}

unsigned long SJKCard::SM4SymEncryptEx(
    unsigned char *pPlainData,
    unsigned long ulPlainDataLen,
    unsigned char *pSessionKey,
    unsigned long *pSessionKeyLen,
    unsigned char *pCipherData,
    unsigned long *pulCipherDataLen
)
{
    int rv;  //Candidate algorithm:SGD_SMS4_ECB,SGD_SMS4_CBC,SGD_SM1_ECB,SGD_SM1_CBC
    SGD_HANDLE hSM4EnxHandle;
    SGD_HANDLE hSessionKeyEnxHandle;
    OpenSession(hEkey_, &hSM4EnxHandle);
    rv = SDF_GenerateKeyWithKEK(hSM4EnxHandle, SESSION_KEY_LEN * 8, SGD_SMS4_ECB, SESSION_KEY_INDEX, pSessionKey, (SGD_UINT32*)pSessionKeyLen, &hSessionKeyEnxHandle);
    if (rv == SDR_OK)//if (nullptr != hSessionKeyHandle)
    {
        unsigned char pIv[16];
        memset(pIv, 0, 16);
        rv = SDF_Encrypt(hSM4EnxHandle, hSessionKeyEnxHandle, SGD_SMS4_ECB, pIv, pPlainData, ulPlainDataLen, pCipherData, (SGD_UINT32*)pulCipherDataLen);
        if (rv == SDR_OK)
        {
            DebugPrint("SM4 symetry encrypt successful!");
        }
        else
        {
            DebugPrint("SM4 symetry encrypt failed, failed number:[0x%08x]", rv);
        }
    }
    else
    {
        DebugPrint("Importing SymmetryKey is failed, failed number:[0x%08x]", rv);
        //DebugPrint("SessionKey handle is unavailable,please check whether it was generated or imported.");
    }

    rv = SDF_DestroyKey(hSM4EnxHandle, hSessionKeyEnxHandle);
    if (rv != SDR_OK)
    {
        DebugPrint("Destroy SessionKey failed, failed number:[0x%08x]", rv);
    }
    else
    {
        hSessionKeyEnxHandle = NULL;
        DebugPrint("Destroy SessionKey successfully!");
        CloseSession(hSM4EnxHandle);
    }
    return rv;
}

void SJKCard::standPubToSM2Pub(unsigned char* standPub, int standPubLen, ECCrefPublicKey& sm2Publickey)
{
    //sm2Publickey.bits = standPubLen;
    sm2Publickey.bits = PUBLIC_KEY_BIT_LEN; //must be 256;
    memcpy(sm2Publickey.x, standPub + 1, 32);
    memcpy(sm2Publickey.y, standPub + 33, 32);
}

void SJKCard::standPriToSM2Pri(unsigned char* standPri, int standPriLen, ECCrefPrivateKey& sm2Privatekey)
{
    if (standPriLen > 32)
        return;
    else
    {
        sm2Privatekey.bits = standPriLen*8;
        memcpy(sm2Privatekey.D, standPri, standPriLen);
    }
}

void SJKCard::c1c2c3ToCardCipher(unsigned char* pCardCipher, unsigned long cardCipherLen, unsigned char* pCipher, unsigned long cipherLen)
{
    unsigned long realCipherLen = cipherLen - 96;

    memset(pCardCipher, 0, cardCipherLen);
    pCardCipher[0] = realCipherLen;
    memcpy(pCardCipher + 4, pCipher, 64 + realCipherLen);
    memcpy(pCardCipher + 204, pCipher + 64 + realCipherLen, 32);
}

#endif
