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
#ifndef HARDENCRYPT_SJKCARD_H_INCLUDE
#define HARDENCRYPT_SJKCARD_H_INCLUDE

#include <peersafe/gmencrypt/GmEncrypt.h>
#include <peersafe/gmencrypt/hardencrypt/sjkCard/swsds.h>
#include <cstring>

#define SESSION_KEY_INDEX  1
#define SESSION_KEY_LEN    16
#define CARD_CIPHER_LEN 236

const std::string priKeyAccessCode_g = "peersafe";

#ifdef HARD_GM

class SJKCard : public GmEncrypt
{
public:
    SJKCard()
    {
        memset(pubKeyUser_, 0, PUBLIC_KEY_EXT_LEN);
        if (!OpenDevice())
        {
            isHardEncryptExist_ = true;
        }
        else
        {
            isHardEncryptExist_ = false;
        }
    }
    ~SJKCard()
    {
        CloseDevice();
    }
public:
    unsigned long  OpenDevice();
    unsigned long  CloseDevice();
    unsigned long  OpenSession(HANDLE hKey, SGD_HANDLE *phSessionHandle);
    unsigned long  CloseSession(HANDLE hSession);
    
    void mergePublicXYkey(unsigned char* publickey33, ECCrefPublicKey& originalPublicKey);
	//Generate random
	unsigned long GenerateRandom(
		unsigned int uiLength,
		unsigned char * pucRandomBuf);
	unsigned long GenerateRandom2File(
		unsigned int uiLength,
		unsigned char * pucRandomBuf,
		int times);
    bool randomSingleCheck(unsigned long randomCheckLen);
    //SM2 interface
	//Get Private key access right
	unsigned long getPrivateKeyRight(
		unsigned int uiKeyIndex,
		unsigned char *pucPassword = nullptr,
		unsigned int uiPwdLength = 0);
	//Release Private key access right
	unsigned long releasePrivateKeyRight(
		unsigned int uiKeyIndex);
	std::pair<unsigned char*, int> getECCSyncTablePubKey(unsigned char* publicKeyTemp);
	std::pair<unsigned char*, int> getECCNodeVerifyPubKey(unsigned char* publicKeyTemp, int keyIndex);
    //Generate Publick&Secret Key
    unsigned long SM2GenECCKeyPair(
        std::vector<unsigned char>& publicKey,
        std::vector<unsigned char>& privateKey,
        bool isRoot,
        unsigned long ulAlias,
        unsigned long ulKeyUse,
        unsigned long ulModulusLen);
    bool generatePubFromPri(
        const unsigned char* pPriUC,
        int priLen,
        std::vector<unsigned char>& publicKey);
    //SM2 Sign&Verify
	unsigned long SM2ECCSign(
		std::pair<int, int> pri4SignInfo,
		std::pair<unsigned char*, int>& pri4Sign,
		unsigned char *pInData,
		unsigned long ulInDataLen,
		std::vector<unsigned char>& signedDataV);
    unsigned long SM2ECCVerify(
        std::pair<unsigned char*, int>& pub4Verify,
        unsigned char *pInData,
        unsigned long ulInDataLen,
        unsigned char *pSignValue,
        unsigned long ulSignValueLen);
    //SM2 Encrypt&Decrypt
    unsigned long SM2ECCEncrypt(
        std::pair<unsigned char*, int>& pub4Encrypt,
        unsigned char * pPlainData,
        unsigned long ulPlainDataLen,
        std::vector<unsigned char>& cipherDataV);
    unsigned long SM2ECCDecrypt(
		std::pair<int, int> pri4DecryptInfo,
        std::pair<unsigned char*, int>& pri4Decrypt,
        unsigned char *pCipherData,
        unsigned long ulCipherDataLen,
        std::vector<unsigned char>& plainDataV,
		bool isSymmertryKey,
        void* sm4Handle);
	unsigned long SM2ECCInterDecryptSyncKey(
		int pri4DecryptIndex,
		unsigned char *pCipherData,
		unsigned long ulCipherDataLen,
		void *hSM4Key
		);
    //SM3 interface
    unsigned long SM3HashTotal(
        unsigned char *pInData,
        unsigned long ulInDataLen,
        unsigned char *pHashData,
        unsigned long *pulHashDataLen);
    unsigned long SM3HashInit(SGD_HANDLE *phSM3Handle);
    unsigned long SM3HashFinal(SGD_HANDLE phSM3Handle, unsigned char *pHashData, unsigned long *pulHashDataLen);
    void operator()(SGD_HANDLE phSM3Handle, void const* data, std::size_t size) noexcept;
    //SM4 Symetry Encrypt&Decrypt
	unsigned long SM4SymEncrypt(
		unsigned int  uiAlgMode,
		unsigned char *pSessionKey,
		unsigned long pSessionKeyLen,
		unsigned char *pPlainData,
		unsigned long ulPlainDataLen,
		unsigned char *pCipherData,
		unsigned long *pulCipherDataLen,
		int secKeyType);
	unsigned long SM4SymDecrypt(
		unsigned int  uiAlgMode,
		unsigned char *pSessionKey,
		unsigned long pSessionKeyLen,
		unsigned char *pCipherData,
		unsigned long ulCipherDataLen,
		unsigned char *pPlainData,
		unsigned long *pulPlainDataLen,
		int secKeyType);
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
    void standPubToSM2Pub(unsigned char* standPub, int standPubLen, ECCrefPublicKey&);
    void standPriToSM2Pri(unsigned char* standPri, int standPriLen, ECCrefPrivateKey&);
    bool getPublicKey(ECCrefPublicKey&, std::vector<unsigned char> pubKeyV);
    bool getPrivateKey(ECCrefPrivateKey&, std::vector<unsigned char> priKeyV);
    void c1c2c3ToCardCipher(unsigned char* pCardCipher, unsigned long cardCipherLen, unsigned char* pCipher, unsigned long cipherLen);
	unsigned long SM2ECCExternalSign(
		std::pair<unsigned char*, int>& pri4Sign,
		unsigned char *pInData,
		unsigned long ulInDataLen,
		std::vector<unsigned char>& signedDataV);
	unsigned long SM2ECCInternalSign(
		int pri4SignIndex,
		unsigned char *pInData,
		unsigned long ulInDataLen,
		std::vector<unsigned char>& signedDataV);
	unsigned long SM2ECCExternalDecrypt(
		std::pair<unsigned char*, int>& pri4Decrypt,
		unsigned char *pCipherData,
		unsigned long ulCipherDataLen,
		std::vector<unsigned char>& plainDataV);
	unsigned long SM2ECCInternalDecrypt(
		int pri4DecryptIndex,
		unsigned char *pCipherData,
		unsigned long ulCipherDataLen,
		std::vector<unsigned char>& plainDataV);
	
	unsigned long generateIV(unsigned int uiAlgMode, unsigned char* pIV);
	unsigned long SM4ExternalSymEncrypt(
		unsigned int uiAlgMode,
		unsigned char *pSessionKey,
		unsigned long pSessionKeyLen,
		unsigned char *pPlainData,
		unsigned long ulPlainDataLen,
		unsigned char *pCipherData,
		unsigned long *pulCipherDataLen);
	unsigned long SM4ExternalSymDecrypt(
		unsigned int uiAlgMode,
		unsigned char *pSessionKey,
		unsigned long pSessionKeyLen,
		unsigned char *pCipherData,
		unsigned long ulCipherDataLen,
		unsigned char *pPlainData,
		unsigned long *pulPlainDataLen);
	//use inner sm4 handle
	unsigned long SM4InternalSymEncrypt(
		unsigned int uiAlgMode,
		void *hSM4Handle,
		unsigned char *pPlainData,
		unsigned long ulPlainDataLen,
		unsigned char *pCipherData,
		unsigned long *pulCipherDataLen);
	unsigned long SM4InternalSymDecrypt(
		unsigned int uiAlgMode,
		void *hSM4Handle,
		unsigned char *pCipherData,
		unsigned long ulCipherDataLen,
		unsigned char *pPlainData,
		unsigned long *pulPlainDataLen);

private:
    // ECCrefPublicKey pubKeyUserExt_;
    // ECCrefPrivateKey priKeyUserExt_;
    SGD_HANDLE      hSessionKeyHandle_;
    // unsigned char pubKeyUser_[PUBLIC_KEY_EXT_LEN];
};
#endif
#endif
