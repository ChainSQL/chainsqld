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

#include <peersafe/gmencrypt/hardencrypt/sjk1238_26.h>
#include <cstdlib>
#include <iostream>

#ifdef GM_ALG_PROCESS

unsigned long  SJK1238::OpenDevice()
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
unsigned long  SJK1238::CloseDevice()
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
unsigned long SJK1238::OpenSession(HANDLE hKey, SGD_HANDLE *phSessionHandle)
{
    int rv;
    rv = SDF_OpenSession(hKey, phSessionHandle);
    if (rv != SDR_OK)
    {
        DebugPrint("Open session failed, failed number:[0x%08x]", rv);
    }
    return rv;
}
unsigned long  SJK1238::CloseSession(HANDLE hSession)
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
std::pair<unsigned char*, int> SJK1238::getPublicKey()
{
    mergePublicXYkey(pubKeyUser_, pubKeyUserExt_);
    return std::make_pair(pubKeyUser_,sizeof(pubKeyUser_));
}
std::pair<unsigned char*, int> SJK1238::getPrivateKey()
{
    return std::make_pair(priKeyUserExt_.D,sizeof(priKeyUserExt_.D));
}
void SJK1238::mergePublicXYkey(unsigned char* publickey, ECCrefPublicKey& originalPublicKey)
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
//SM2 interface
//Generate Publick&Secret Key
unsigned long SJK1238::SM2GenECCKeyPair(
    unsigned long ulAlias,
    unsigned long ulKeyUse,
    unsigned long ulModulusLen)
{
    int rv;
    memset(pubKeyUser_, 0, sizeof(pubKeyUser_));
    rv = SDF_GenerateKeyPair_ECC(hSessionHandle_, SGD_SM2_3, ulModulusLen, &pubKeyUserExt_, &priKeyUserExt_);
    
    if (rv != SDR_OK)
    {
        DebugPrint("Generate ECC key pair failed, failed number:[0x%08x]", rv);
        return rv;
    }
    else
    {
        DebugPrint("Generate ECC key pair successful!");
        return 0;
    }
}
//SM2 Sign&Verify
unsigned long SJK1238::SM2ECCSign(
    std::pair<unsigned char*, int>& pri4Sign,
    unsigned char *pInData,
    unsigned long ulInDataLen,
    unsigned char *pSignValue,
    unsigned long *pulSignValueLen,
    unsigned long ulAlias,
    unsigned long ulKeyUse)
{
    int rv;
    ECCrefPrivateKey pri4SignTemp;
    standPriToSM2Pri(pri4Sign.first, pri4Sign.second, pri4SignTemp);
    rv = SDF_ExternalSign_ECC(hSessionHandle_, SGD_SM2_1, &pri4SignTemp, pInData, pri4SignTemp.bits/8, (ECCSignature *)pSignValue);
    if (rv != SDR_OK)
    {
        DebugPrint("SM2 secret key sign failed, failed number:[0x%08x]", rv);
        return -1;
    }
    else
    {
        DebugPrint("SM2 secret key sign successful!");
        return 0;
    }
}
unsigned long SJK1238::SM2ECCVerify(
    std::pair<unsigned char*, int>& pub4Verify,
    unsigned char *pInData,
    unsigned long ulInDataLen,
    unsigned char *pSignValue,
    unsigned long ulSignValueLen,
    unsigned long ulAlias,
    unsigned long ulKeyUse)
{
    int rv;
    ECCrefPublicKey pub4VerifyTemp;
    standPubToSM2Pub(pub4Verify.first, pub4Verify.second, pub4VerifyTemp);
    rv = SDF_ExternalVerify_ECC(hSessionHandle_, SGD_SM2_1, &pub4VerifyTemp, pInData, PUBLIC_KEY_BIT_LEN / 8, (ECCSignature *)pSignValue);
    if (rv != SDR_OK)
    {
        DebugPrint("SM2 public key verify signature failed, failed number[0x%08x]", rv);
        return -1;
    }
    else
    {
        DebugPrint("SM2 public key verify signature successful!");
        return 0;
    }
}
//SM2 Encrypt&Decrypt
unsigned long SJK1238::SM2ECCEncrypt(
    std::pair<unsigned char*, int>& pub4Encrypt,
    unsigned char * pPlainData,
    unsigned long ulPlainDataLen,
    unsigned char * pCipherData,
    unsigned long * pulCipherDataLen,
    unsigned long ulAlias,
    unsigned long ulKeyUse)
{
    int rv;
    ECCrefPublicKey pub4EncryptTemp;
    standPubToSM2Pub(pub4Encrypt.first, pub4Encrypt.second, pub4EncryptTemp);
    rv = SDF_ExternalEncrypt_ECC(hSessionHandle_, SGD_SM2_3, &pub4EncryptTemp, pPlainData, ulPlainDataLen, (ECCCipher *)pCipherData);
    if (rv != SDR_OK)
    {
        DebugPrint("SM2 public key encrypt failed, failed number:[0x%08x]", rv);
        return -1;
    }
    else
    {
        DebugPrint("SM2 public key encrypt successful!");
        return 0;
    }
}
unsigned long SJK1238::SM2ECCDecrypt(
    std::pair<unsigned char*, int>& pri4Decrypt,
    unsigned char *pCipherData,
    unsigned long ulCipherDataLen,
    unsigned char *pPlainData,
    unsigned long *pulPlainDataLen,
    unsigned long ulAlias,
    unsigned long ulKeyUse)
{
    int rv;
    ECCrefPrivateKey pri4DecryptTemp;
    standPriToSM2Pri(pri4Decrypt.first, pri4Decrypt.second, pri4DecryptTemp);
    rv = SDF_ExternalDecrypt_ECC(hSessionHandle_, SGD_SM2_3, &pri4DecryptTemp, (ECCCipher *)pCipherData, pPlainData, (SGD_UINT32*)pulPlainDataLen);
    if (rv != SDR_OK)
    {
        DebugPrint("SM2 secret key decrypt failed, failed number:[0x%08x]", rv);
        return -1;
    }
    else
    {
        DebugPrint("SM2 secret key decrypt successful!");
        return 0;
    }
}
//SM3 interface
unsigned long SJK1238::SM3HashTotal(
    unsigned char *pInData,
    unsigned long ulInDataLen,
    unsigned char *pHashData,
    unsigned long *pulHashDataLen)
{
    int rv;
    rv = SDF_HashInit(hSessionHandle_, SGD_SM3, NULL, NULL, 0);
    if (rv == SDR_OK)
    {
        rv = SDF_HashUpdate(hSessionHandle_, pInData, ulInDataLen);
        if (rv == SDR_OK)
        {
            rv = SDF_HashFinal(hSessionHandle_, pHashData, (SGD_UINT32*)pulHashDataLen);
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
    return rv;
}
unsigned long SJK1238::SM3HashInit(SGD_HANDLE *phSM3Handle)
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

unsigned long SJK1238::SM3HashFinal(SGD_HANDLE phSM3Handle, unsigned char *pHashData, unsigned long *pulHashDataLen)
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
void SJK1238::operator()(SGD_HANDLE phSM3Handle, void const* data, std::size_t size) noexcept
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

//SM4 Symetry Encrypt&Decrypt
unsigned long SJK1238::SM4SymEncrypt(
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
    OpenSession(hEkey_, &hSM4EnHandle);
    unsigned char pIndata[16384];
    unsigned long nInlen = 1024;
    pkcs5Padding(pPlainData, ulPlainDataLen, pIndata, &nInlen);
    //rv = SDF_ImportKeyWithKEK(hSM4EnHandle, SGD_SMS4_ECB, SESSION_KEY_INDEX, pSessionKey, pSessionKeyLen, &hSessionKeyEnHandle);
    rv = SDF_ImportKey(hSM4EnHandle, pSessionKey, pSessionKeyLen, &hSessionKeyEnHandle);
    if(rv == SDR_OK)//if (nullptr != hSessionKeyHandle)
    {
        unsigned char pIv[16];
        memset(pIv, 0, 16);
        rv = SDF_Encrypt(hSM4EnHandle, hSessionKeyEnHandle, SGD_SMS4_ECB, pIv, pIndata, nInlen, pCipherData, (SGD_UINT32*)pulCipherDataLen);
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
unsigned long SJK1238::SM4SymDecrypt(
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
    OpenSession(hEkey_, &hSM4DeHandle);
    //rv = SDF_ImportKeyWithKEK(hSM4DeHandle, SGD_SMS4_ECB, SESSION_KEY_INDEX, pSessionKey, pSessionKeyLen, &hSessionKeyDeHandle);
    rv = SDF_ImportKey(hSM4DeHandle, pSessionKey, pSessionKeyLen, &hSessionKeyDeHandle);
    if (rv == SDR_OK)//if (nullptr != hSessionKeyHandle_)
    {
        unsigned char pIv[16];
        memset(pIv, 0, 16);
        unsigned char pOutdata[16384];
        unsigned long nOutlen = 1024;
        rv = SDF_Decrypt(hSM4DeHandle, hSessionKeyDeHandle, SGD_SMS4_ECB, pIv, pCipherData, ulCipherDataLen, pOutdata, (SGD_UINT32*)&nOutlen);
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

unsigned long SJK1238::SM4GenerateSessionKey(
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

unsigned long SJK1238::SM4SymEncryptEx(
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

void SJK1238::standPubToSM2Pub(unsigned char* standPub, int standPubLen, ECCrefPublicKey& sm2Publickey)
{
    //sm2Publickey.bits = standPubLen;
    sm2Publickey.bits = PUBLIC_KEY_BIT_LEN; //must be 256;
    memcpy(sm2Publickey.x, standPub + 1, 32);
    memcpy(sm2Publickey.y, standPub + 33, 32);
}

void SJK1238::standPriToSM2Pri(unsigned char* standPri, int standPriLen, ECCrefPrivateKey& sm2Privatekey)
{
    if (standPriLen > 32)
        return;
    else
    {
        sm2Privatekey.bits = standPriLen*8;
        memcpy(sm2Privatekey.D, standPri, standPriLen);
    }
}

#endif
