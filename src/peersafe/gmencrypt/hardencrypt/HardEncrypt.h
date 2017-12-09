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
#ifndef HARDENCRYPT_HARDENCRYPT_H_INCLUDE
#define HARDENCRYPT_HARDENCRYPT_H_INCLUDE

//#define GM_ALG_PROCESS

#ifdef GM_ALG_PROCESS
#ifndef _WIN32
//#define BEGIN_SDKEY //control the sdkey file compile, don't compile in default
//#define SD_KEY_SWITCH //switch sd and sjk in linux
#endif
#endif

#define DEBUG_PRINTF
#ifdef DEBUG_PRINTF
#define DebugPrint(fmt,...) printf(fmt"\n",##__VA_ARGS__)
#else
#define DebugPrint(fmt,...)
#endif // DEBUGLC_PRINTF

//#include <hardencrypt/skj1238_26/swsds.h>
//#include <hardencrypt/sdkey/swsdkey.h>
#include <ripple/beast/hash/endian.h>
#include <utility>
#include <mutex>

typedef void* HANDLE;
#define SD_KEY_ALIAS  0
#define SD_KEY_USE    1
#define GM_ALG_MARK   0x47
#define PRIVATE_KEY_BIT_LEN 256
#define PUBLIC_KEY_BIT_LEN  256
#define PRIVATE_KEY_EXT_LEN 32
#define PUBLIC_KEY_EXT_LEN  65

class HardEncrypt
{
public:
    class SM3Hash
    {
    public:
        SM3Hash(HardEncrypt *pEncrypt);
        ~SM3Hash();
        
        void SM3HashInitFun();
        void SM3HashFinalFun(unsigned char *pHashData, unsigned long *pulHashDataLen);
        void operator()(void const* data, std::size_t size) noexcept;
    private:
        HardEncrypt *pHardEncrypt_;
        static std::mutex mutexSM3_;
    protected:
        HANDLE hSM3Handle_;
    public:
        static beast::endian const endian = beast::endian::big;
    };

public:
    friend class SM3Hash;
    HardEncrypt();
    ~HardEncrypt();    
    enum KeyType { userKey, rootKey };

public:
    //SM3Hash &getSM3Obj();
    std::pair<unsigned char*, int> getRootPublicKey();
    std::pair<unsigned char*, int> getRootPrivateKey();
    virtual std::pair<unsigned char*, int> getPublicKey() = 0;
    virtual std::pair<unsigned char*, int> getPrivateKey() = 0;
    bool isHardEncryptExist();

    //SM2 interface
    //Generate Publick&Secret Key
    virtual unsigned long SM2GenECCKeyPair(
        unsigned long ulAlias = SD_KEY_ALIAS,
        unsigned long ulKeyUse = SD_KEY_USE,
        unsigned long ulModulusLen = PRIVATE_KEY_BIT_LEN) = 0;
    //SM2 Sign&Verify
    virtual unsigned long SM2ECCSign(
        std::pair<unsigned char*, int>& pri4Sign,
        unsigned char *pInData,
        unsigned long ulInDataLen,
        unsigned char *pSignValue,
        unsigned long *pulSignValueLen,
        unsigned long ulAlias = SD_KEY_ALIAS,
        unsigned long ulKeyUse = SD_KEY_USE) = 0;
    virtual unsigned long SM2ECCVerify(
        std::pair<unsigned char*, int>& pub4Verify,
        unsigned char *pInData,
        unsigned long ulInDataLen,
        unsigned char *pSignValue,
        unsigned long ulSignValueLen,
        unsigned long ulAlias = SD_KEY_ALIAS,
        unsigned long ulKeyUse = SD_KEY_USE) = 0;
    //SM2 Encrypt&Decrypt
    virtual unsigned long SM2ECCEncrypt(
        std::pair<unsigned char*, int>& pub4Encrypt,
        unsigned char * pPlainData,
        unsigned long ulPlainDataLen,
        unsigned char * pCipherData,
        unsigned long * pulCipherDataLen,
        unsigned long ulAlias = SD_KEY_ALIAS,
        unsigned long ulKeyUse = SD_KEY_USE) = 0;
    virtual unsigned long SM2ECCDecrypt(
        std::pair<unsigned char*, int>& pri4Decrypt,
        unsigned char *pCipherData,
        unsigned long ulCipherDataLen,
        unsigned char *pPlainData,
        unsigned long *pulPlainDataLen,
        unsigned long ulAlias = SD_KEY_ALIAS,
        unsigned long ulKeyUse = SD_KEY_USE) = 0;
    //SM3 interface
    virtual unsigned long SM3HashTotal(
        unsigned char *pInData,
        unsigned long ulInDataLen,
        unsigned char *pHashData,
        unsigned long *pulHashDataLen) = 0;   
    //SM4 interface
    //SM4 Symetry Encrypt&Decrypt
    virtual unsigned long SM4SymEncrypt(
        unsigned char *pSessionKey,
        unsigned long pSessionKeyLen,
        unsigned char *pPlainData,
        unsigned long ulPlainDataLen,
        unsigned char *pCipherData,
        unsigned long *pulCipherDataLen) = 0;
    virtual unsigned long SM4SymDecrypt(
        unsigned char *pSessionKey,
        unsigned long pSessionKeyLen,
        unsigned char *pCipherData,
        unsigned long ulCipherDataDataLen,
        unsigned char *pPlainData,
        unsigned long *pulPlainDataLen) = 0;
    virtual unsigned long SM4GenerateSessionKey(
        unsigned char *pSessionKey,
        unsigned long *pSessionKeyLen) = 0;
    virtual unsigned long SM4SymEncryptEx(
        unsigned char *pPlainData,
        unsigned long ulPlainDataLen,
        unsigned char *pSessionKey,
        unsigned long *pSessionKeyLen,
        unsigned char *pCipherData,
        unsigned long *pulCipherDataLen) = 0;

protected:
    void pkcs5Padding(unsigned char* srcUC, unsigned long srcUCLen, unsigned char* dstUC, unsigned long* dstUCLen);
    void dePkcs5Padding(unsigned char* srcUC, unsigned long srcUCLen, unsigned char* dstUC, unsigned long* dstUCLen);

private:
    virtual unsigned long  OpenDevice() = 0;
    virtual unsigned long  CloseDevice() = 0;
    //SM3 interface
    virtual void operator()(HANDLE phSM3Handle, void const* data, std::size_t size) noexcept = 0;
    virtual unsigned long SM3HashInit(HANDLE* phSM3Handle) = 0;
    virtual unsigned long SM3HashFinal(HANDLE phSM3Handle, unsigned char *pHashData, unsigned long *pulHashDataLen) = 0;

protected:
    HANDLE hEkey_;
    HANDLE hSessionHandle_;
    bool isHardEncryptExist_;
    unsigned long hashType_;     //4 means SM3 algorithm
    unsigned long symAlgFlag_;   //6 means SM4 algorithm
    unsigned long symAlgMode_;   //2 means CBC algo_mode
    unsigned char pubKeyRoot_[65];
/*private:
    SM3Hash objSM3_;*/
};

#endif
