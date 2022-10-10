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

#include <peersafe/gmencrypt/GmEncrypt.h>
#include <ripple/basics/StringUtilities.h>

// #define SOFTENCRYPT
#ifdef SOFTENCRYPT
#include <openssl/ec.h>
#include <openssl/pem.h>
#include <openssl/sm2.h>
#include <openssl/sm3.h>
#include <openssl/sms4.h>

// #include <peersafe/gmencrypt/softencrypt/GmSSL/include/openssl/engine.h>
// #include <peersafe/gmencrypt/softencrypt/GmSSL/include/openssl/evp.h>
// #include <peersafe/gmencrypt/softencrypt/GmSSL/include/openssl/ec.h>
// #include <peersafe/gmencrypt/softencrypt/GmSSL/include/openssl/rand.h>
// #include <peersafe/gmencrypt/softencrypt/GmSSL/include/openssl/obj_mac.h>
// #include <peersafe/gmencrypt/softencrypt/GmSSL/include/openssl/pem.h>
// #include <peersafe/gmencrypt/softencrypt/GmSSL/include/openssl/sm2.h>
// #include <peersafe/gmencrypt/softencrypt/GmSSL/include/openssl/sm3.h>
// #include <peersafe/gmencrypt/softencrypt/GmSSL/include/openssl/sms4.h>

const char g_signId[] = "1234567812345678";
const int SM2_VERIFY_SUCCESS=1;
const int SM2_ENCRYPT_PRE = 0x30;

class SoftEncrypt : public GmEncrypt
{
public:
    SoftEncrypt()
    {
        DebugPrint("SoftEncrypt ENGINE_init successfully!");
    }
    ~SoftEncrypt()
    {

    }
public:
    unsigned long  OpenDevice() override;
    unsigned long  CloseDevice() override;
    
    EC_KEY* standPubToSM2Pub(unsigned char* standPub, int standPubLen);
    //Generate random
	unsigned long GenerateRandom(
		unsigned int uiLength,
		unsigned char * pucRandomBuf) override;
	unsigned long GenerateRandom2File(
		unsigned int uiLength,
		unsigned char * pucRandomBuf,
		int times) override;
    bool randomSingleCheck(unsigned long randomCheckLen) override;
    //SM2 interface
    unsigned long getPrivateKeyRight(
		unsigned int uiKeyIndex,
		unsigned char *pucPassword = nullptr,
		unsigned int uiPwdLength = 0) override;
	//Release Private key access right
	unsigned long releasePrivateKeyRight(
		unsigned int uiKeyIndex) override;
	std::pair<unsigned char*, int> getECCSyncTablePubKey(unsigned char* publicKeyTemp) override;
	std::pair<unsigned char*, int> getECCNodeVerifyPubKey(unsigned char* publicKeyTemp, int keyIndex) override;
    //Generate Publick&Secret Key
    unsigned long SM2GenECCKeyPair(
        std::vector<unsigned char>& publicKey,
        std::vector<unsigned char>& privateKey,
        bool isRoot,
        unsigned long ulAlias,
        unsigned long ulKeyUse,
        unsigned long ulModulusLen) override;
    bool generatePubFromPri(
        const unsigned char* pPriUC,
        int priLen,
        std::vector<unsigned char>& publicKey) override;
    //SM2 Sign&Verify
    unsigned long SM2ECCSign(
        std::pair<int, int> pri4SignInfo,
        std::pair<unsigned char*, int>& pri4Sign,
        unsigned char *pInData,
        unsigned long ulInDataLen,
        std::vector<unsigned char>& signedDataV) override;
    unsigned long SM2ECCVerify(
        std::pair<unsigned char*, int>& pub4Verify,
        unsigned char *pInData,
        unsigned long ulInDataLen,
        unsigned char *pSignValue,
        unsigned long ulSignValueLen) override;
    //SM2 Encrypt&Decrypt
    unsigned long SM2ECCEncrypt(
        std::pair<unsigned char*, int>& pub4Encrypt,
        unsigned char * pPlainData,
        unsigned long ulPlainDataLen,
        std::vector<unsigned char>& cipherDataV) override;
    unsigned long SM2ECCDecrypt(
        std::pair<int, int> pri4DecryptInfo,
        std::pair<unsigned char*, int>& pri4Decrypt,
        unsigned char *pCipherData,
        unsigned long ulCipherDataLen,
        std::vector<unsigned char>& plainDataV,
		bool isSymmertryKey,
        void* sm4Handle) override;
    //SM3 interface
    unsigned long SM3HashTotal(
        unsigned char *pInData,
        unsigned long ulInDataLen,
        unsigned char *pHashData,
        unsigned long *pulHashDataLen) override;
    // unsigned long SM3HashInit(EVP_MD_CTX *phSM3Handle);
    unsigned long SM3HashInit(HANDLE *phSM3Handle) override;
    unsigned long SM3HashFinal(void* phSM3Handle, unsigned char *pHashData, unsigned long *pulHashDataLen) override;
    void operator()(void* phSM3Handle, void const* data, std::size_t size) noexcept override;
    //SM4 Symetry Encrypt&Decrypt
    unsigned long SM4SymEncrypt(
		unsigned int  uiAlgMode,
		unsigned char *pSessionKey,
		unsigned long pSessionKeyLen,
		unsigned char *pPlainData,
		unsigned long ulPlainDataLen,
		unsigned char *pCipherData,
		unsigned long *pulCipherDataLen,
		int secKeyType) override;
	unsigned long SM4SymDecrypt(
		unsigned int  uiAlgMode,
		unsigned char *pSessionKey,
		unsigned long pSessionKeyLen,
		unsigned char *pCipherData,
		unsigned long ulCipherDataLen,
		unsigned char *pPlainData,
		unsigned long *pulPlainDataLen,
		int secKeyType) override;
    unsigned long SM4GenerateSessionKey(
        unsigned char *pSessionKey,
        unsigned long *pSessionKeyLen) override;
    unsigned long SM4SymEncryptEx(
        unsigned char *pPlainData,
        unsigned long ulPlainDataLen,
        unsigned char *pSessionKey,
        unsigned long *pSessionKeyLen,
        unsigned char *pCipherData,
        unsigned long *pulCipherDataLen
    ) override;
//private:
//    ENGINE *sm_engine_;
//    const EVP_MD *md_;
//    sm3_ctx_t sm3_ctx_;

private:
    bool getPublicKey(EC_KEY *sm2Keypair, std::vector<unsigned char>& pubKey);
    bool getPrivateKey(EC_KEY *sm2Keypair, std::vector<unsigned char>& priKey);
    //bool setPubfromPri(EC_KEY* pEcKey);
    size_t EC_KEY_key2buf(const EC_KEY *key, unsigned char **pbuf);
    EC_KEY* CreateEC(unsigned char *key, int is_public);
    void cipherReEncode(unsigned char* pCipher, unsigned long cipherLen);
    void cipherReDecode(unsigned char* pCipher, unsigned long cipherLen);
    int computeDigestWithSm2(EC_KEY* ec_key, unsigned char* pInData, unsigned long ulInDataLen, unsigned char* dgst, unsigned int*dgstLen);
    unsigned long generateIV(unsigned int uiAlgMode, unsigned char * pIV);
};

#endif
#endif
