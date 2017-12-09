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
#ifndef SOFTENCRYPT_GMSOFTENCRYPT_H_INCLUDE
#define SOFTENCRYPT_GMSOFTENCRYPT_H_INCLUDE

#include <peersafe/gmencrypt/hardencrypt/HardEncrypt.h>
#ifdef GM_ALG_PROCESS

//#define SOFTENCRYPT
#ifdef SOFTENCRYPT
#include <peersafe/gmencrypt/softencrypt/usr/include/openssl/engine.h>
#include <peersafe/gmencrypt/softencrypt/usr/include/openssl/evp.h>
#include <peersafe/gmencrypt/softencrypt/usr/include/openssl/rand.h>
#include <peersafe/gmencrypt/softencrypt/usr/include/openssl/SMEngine.h>
#include <peersafe/gmencrypt/softencrypt/usr/include/openssl/sm2.h>
//#include <gmencrypt/softencrypt/usr/include/openssl/hmac.h>

const char g_signId[] = "1234567812345678";
class SoftEncrypt : public HardEncrypt
{
public:
    SoftEncrypt()
    {
        priAndPubKey_ = NULL;
        OpenSSL_add_all_algorithms();
        ENGINE_load_dynamic();
        sm_engine_ = ENGINE_by_id("CipherSuite_SM");
        if (sm_engine_ == NULL)
            DebugPrint("SM Engine is NULL.");
        ENGINE_init(sm_engine_);
        md_ = ENGINE_get_digest(sm_engine_, NID_sm3);
        DebugPrint("SoftEncrypt ENGINE_init successfully!");
    }
    ~SoftEncrypt()
    {

    }
public:
    unsigned long  OpenDevice();
    unsigned long  CloseDevice();
    std::pair<unsigned char*, int> getPublicKey();
    std::pair<unsigned char*, int> getPrivateKey();
    //SM2 interface
    //Generate Publick&Secret Key
    unsigned long SM2GenECCKeyPair(
        unsigned long ulAlias,
        unsigned long ulKeyUse,
        unsigned long ulModulusLen);
    //SM2 Sign&Verify
    unsigned long SM2ECCSign(
        std::pair<unsigned char*, int>& pri4Sign,
        unsigned char *pInData,
        unsigned long ulInDataLen,
        unsigned char *pSignValue,
        unsigned long *pulSignValueLen,
        unsigned long ulAlias,
        unsigned long ulKeyUse);
    unsigned long SM2ECCVerify(
        std::pair<unsigned char*, int>& pub4Verify,
        unsigned char *pInData,
        unsigned long ulInDataLen,
        unsigned char *pSignValue,
        unsigned long ulSignValueLen,
        unsigned long ulAlias,
        unsigned long ulKeyUse);
    //SM2 Encrypt&Decrypt
    unsigned long SM2ECCEncrypt(
        std::pair<unsigned char*, int>& pub4Encrypt,
        unsigned char * pPlainData,
        unsigned long ulPlainDataLen,
        unsigned char * pCipherData,
        unsigned long * pulCipherDataLen,
        unsigned long ulAlias,
        unsigned long ulKeyUse);
    unsigned long SM2ECCDecrypt(
        std::pair<unsigned char*, int>& pri4Decrypt,
        unsigned char *pCipherData,
        unsigned long ulCipherDataLen,
        unsigned char *pPlainData,
        unsigned long *pulPlainDataLen,
        unsigned long ulAlias,
        unsigned long ulKeyUse);
    //SM3 interface
    unsigned long SM3HashTotal(
        unsigned char *pInData,
        unsigned long ulInDataLen,
        unsigned char *pHashData,
        unsigned long *pulHashDataLen);
    unsigned long SM3HashInit(EVP_MD_CTX *phSM3Handle);
    unsigned long SM3HashFinal(void* phSM3Handle, unsigned char *pHashData, unsigned long *pulHashDataLen);
    void operator()(void* phSM3Handle, void const* data, std::size_t size) noexcept;
    //SM4 Symetry Encrypt&Decrypt
    unsigned long SM4SymEncrypt(
        unsigned char *pSessionKey,
        unsigned long pSessionKeyLen,
        unsigned char *pPlainData,
        unsigned long ulPlainDataLen,
        unsigned char *pCipherData,
        unsigned long *pulCipherDataLen);
    unsigned long SM4SymDecrypt(
        unsigned char *pSessionKey,
        unsigned long pSessionKeyLen,
        unsigned char *pCipherData,
        unsigned long ulCipherDataLen,
        unsigned char *pPlainData,
        unsigned long *pulPlainDataLen);
    unsigned long SM4GenerateSessionKey(
        unsigned char *pSessionKey,
        unsigned long *pSessionKeyLen);
    unsigned long SM4SymEncryptEx(
        unsigned char *pPlainData,
        unsigned long ulPlainDataLen,
        unsigned char *pSessionKey,
        unsigned long *pSessionKeyLen,
        unsigned char *pCipherData,
        unsigned long *pulCipherDataLen
    );
private:
    ENGINE *sm_engine_;
    const EVP_MD *md_;
    EVP_PKEY *priAndPubKey_;
};

#endif
#endif
#endif
