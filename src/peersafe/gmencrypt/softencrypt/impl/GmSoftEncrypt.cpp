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

#include <peersafe/gmencrypt/softencrypt/GmSoftEncrypt.h>


#ifdef GM_ALG_PROCESS
#ifdef SOFTENCRYPT

unsigned long  SoftEncrypt::OpenDevice()
{
    DebugPrint("SoftEncrypt do not need OpenDevice!");
    return 0;
}
unsigned long  SoftEncrypt::CloseDevice()
{
    DebugPrint("SoftEncrypt do not need CloseDevice!");
    return -1;
}

std::pair<unsigned char*, int> SoftEncrypt::getPublicKey()
{
    unsigned char* pubKeyUserTemp = NULL;
    size_t pubLen = i2o_ECPublicKey(sm2Keypair_, &pubKeyUserTemp);
    if(pubLen != 0)
    {
        DebugPrint("pubLen: %d", pubLen);
        pubKeyUser_[0] = GM_ALG_MARK;
        memcpy(pubKeyUser_+1, pubKeyUserTemp, pubLen);
        return std::make_pair(pubKeyUser_, pubLen+1);
    }
    else return std::make_pair(nullptr, 0);
}
size_t SoftEncrypt::EC_KEY_key2buf(const EC_KEY *key, unsigned char **pbuf)
{
    if (key == NULL || EC_KEY_get0_public_key(key) == NULL || EC_KEY_get0_group(key) == NULL)
        return 0;
    
    size_t len;
    unsigned char *buf;
    len = EC_POINT_point2oct(EC_KEY_get0_group(key), EC_KEY_get0_public_key(key), EC_KEY_get_conv_form(key), NULL, 0, NULL);
    if (len == 0)
        return 0;
    buf = (unsigned char*)OPENSSL_malloc(len);
    if (buf == NULL)
        return 0;
    len = EC_POINT_point2oct(EC_KEY_get0_group(key), EC_KEY_get0_public_key(key), EC_KEY_get_conv_form(key), buf, len, NULL);
    if (len == 0) {
        OPENSSL_free(buf);
        return 0;
    }
    *pbuf = buf;
    return len;
    return 0;
}

std::pair<unsigned char*, int> SoftEncrypt::getPrivateKey()
{
    unsigned char* priKeyUserTemp = NULL;
    int priLen = BN_bn2bin(EC_KEY_get0_private_key(sm2Keypair_), priKeyUserTemp);
    DebugPrint("private key Len: %d", priLen);
    if(0 == priLen)
    {
        return std::make_pair(nullptr, 0);
    }
    else
    {
        return std::make_pair(priKeyUserTemp, priLen);    
    }
}
// void SoftEncrypt::mergePublicXYkey(unsigned char* publickey, ECCrefPublicKey& originalPublicKey)
// {
//     if (GM_ALG_MARK == publickey[0])
//         return;
//     else
//     {
//         publickey[0] = GM_ALG_MARK;
//         memcpy(publickey + 1, originalPublicKey.x, sizeof(originalPublicKey.x));
//         memcpy(publickey + 33, originalPublicKey.y, sizeof(originalPublicKey.y));
//     }
// }
unsigned long SoftEncrypt::GenerateRandom(unsigned int uiLength, unsigned char * pucRandomBuf)
{}
unsigned long SoftEncrypt::GenerateRandom2File(unsigned int uiLength, unsigned char * pucRandomBuf,int times)
{}
bool SoftEncrypt::randomSingleCheck(unsigned long randomCheckLen)
{
    DebugPrint("call softencrypt randomSingleCheck");
    return true;
}
unsigned long SoftEncrypt::getPrivateKeyRight(unsigned int uiKeyIndex, unsigned char * pucPassword, unsigned int uiPwdLength)
{
    return 0;
}
unsigned long SoftEncrypt::releasePrivateKeyRight(unsigned int uiKeyIndex)
{
    return 0;
}
std::pair<unsigned char*, int> SoftEncrypt::getECCSyncTablePubKey(unsigned char* publicKeyTemp)
{}
std::pair<unsigned char*, int> SoftEncrypt::getECCNodeVerifyPubKey(unsigned char* publicKeyTemp, int keyIndex)
{}
//SM2 interface
//Generate Publick&Secret Key
unsigned long SoftEncrypt::SM2GenECCKeyPair(
    unsigned long ulAlias,
    unsigned long ulKeyUse,
    unsigned long ulModulusLen)
{
    int ok = 0;
    sm2Keypair_ = EC_KEY_new_by_curve_name(NID_sm2p256v1);
    if (NULL == sm2Keypair_)
    {
        DebugPrint("SM2GenECCKeyPair-EC_KEY_new_by_curve_name() failed!")
    }

    int genRet = EC_KEY_generate_key(sm2Keypair_);
    if (0 == genRet) {
        DebugPrint("SM2GenECCKeyPair-EC_KEY_generate_key() failed!");
        return -1;
    }
    else return ok;
}
//SM2 Sign&Verify
/*

pri4Sign: private key

		std::pair<unsigned char*, int> pri4Sign = std::make_pair((unsigned char*)sk.data(), sk.size());
*/
unsigned long SoftEncrypt::SM2ECCSign(
    std::pair<int, int> pri4SignInfo,
    std::pair<unsigned char*, int>& pri4Sign,
    unsigned char *pInData,
    unsigned long ulInDataLen,
    unsigned char *pSignValue,
    unsigned long *pulSignValueLen,
    unsigned long ulAlias,
    unsigned long ulKeyUse)
{
    int rv;
    if (SeckeyType::gmOutCard != pri4SignInfo.first)
	{
        return 1;
	}

    BIGNUM* bn = BN_bin2bn((const unsigned char *)(pri4Sign.first), (pri4Sign.second), nullptr);
	if (bn == nullptr) {
		DebugPrint("SM2ECCSign: BN_bin2bn failed");
		return 1;
	}
	
	EC_KEY* ec_key = EC_KEY_new();
	const bool ok = EC_KEY_set_private_key(ec_key, bn);
	BN_clear_free(bn);
	if (!ok) {
		DebugPrint("SM2ECCSign: EC_KEY_set_private_key failed");
		EC_KEY_free(ec_key);
		return 1;
	}

	int type = NID_undef;
	/* sign */
	if (!SM2_sign(type, pInData, ulInDataLen, pSignValue, (unsigned int*)pulSignValueLen, ec_key))
    {
        EC_KEY_free(ec_key);
		DebugPrint("SM2ECCSign: SM2_sign failed!");
		return 1;
	}
    else
    {
        EC_KEY_free(ec_key);
        DebugPrint("SM2ECCSign: SM2 secret key sign successful!");
        return 0;
    }
}
unsigned long SoftEncrypt::SM2ECCVerify(
    std::pair<unsigned char*, int>& pub4Verify,
    unsigned char *pInData,
    unsigned long ulInDataLen,
    unsigned char *pSignValue,
    unsigned long ulSignValueLen,
    unsigned long ulAlias,
    unsigned long ulKeyUse)
{
	EC_KEY* pubkey = EC_KEY_new();
	if (o2i_ECPublicKey(&pubkey, (const unsigned char**)&(pub4Verify.first), pub4Verify.second) != nullptr){

		EC_KEY_set_conv_form(pubkey, POINT_CONVERSION_COMPRESSED);	
	}
	else {
		EC_KEY_free(pubkey);
		return 1;
	}

	int type     = NID_undef;
	/* verify */
	if (!SM2_verify(type, pInData, ulInDataLen, pSignValue, ulSignValueLen, pubkey)) {
		DebugPrint("SM2ECCSign: SM2_sign failed");
        EC_KEY_free(pubkey);
		return 1;
	} 
    else
    {
        EC_KEY_free(pubkey);
        DebugPrint("SM2ECCSign: SM2 secret key sign successful!");
        return 0;
    }
}
//SM2 Encrypt&Decrypt
unsigned long SoftEncrypt::SM2ECCEncrypt(
    std::pair<unsigned char*, int>& pub4Encrypt,
    unsigned char * pPlainData,
    unsigned long ulPlainDataLen,
    unsigned char * pCipherData,
    unsigned long * pulCipherDataLen,
    unsigned long ulAlias,
    unsigned long ulKeyUse)
{
	EC_KEY* pubkey = EC_KEY_new();
	if (o2i_ECPublicKey(&pubkey, (const unsigned char**)&(pub4Encrypt.first), pub4Encrypt.second) != nullptr) {

		EC_KEY_set_conv_form(pubkey, POINT_CONVERSION_COMPRESSED);
	}
	else {
		EC_KEY_free(pubkey);
		return 1;
	}

	if (!SM2_encrypt_with_recommended(pCipherData, (size_t*)pulCipherDataLen,
		(const unsigned char *)pPlainData, ulPlainDataLen, pubkey)) 
    {
		
		DebugPrint("SM2ECCEncrypt: SM2_encrypt_with_recommended failed");
        EC_KEY_free(pubkey);
		return 1;
	}
    else
    {
        DebugPrint("SM2ECCEncrypt: SM2_encrypt_with_recommended successfully");
        EC_KEY_free(pubkey);
		return 0;
    }
}
unsigned long SoftEncrypt::SM2ECCDecrypt(
    std::pair<int, int> pri4DecryptInfo,
    std::pair<unsigned char*, int>& pri4Decrypt,
    unsigned char *pCipherData,
    unsigned long ulCipherDataLen,
    unsigned char *pPlainData,
    unsigned long *pulPlainDataLen,
    bool isSymmertryKey,
    unsigned long ulAlias,
    unsigned long ulKeyUse)
{
    if (SeckeyType::gmOutCard != pri4DecryptInfo.first)
    {
        return 1;
    }
	BIGNUM* bn = BN_bin2bn((const unsigned char *)(pri4Decrypt.first), (pri4Decrypt.second), nullptr);
	if (bn == nullptr) {
		DebugPrint("SM2ECCDecrypt: BN_bin2bn failed");
		return 1;
	}

	EC_KEY* ec_key = EC_KEY_new();
	const bool ok = EC_KEY_set_private_key(ec_key, bn);
	BN_clear_free(bn);

	if (!ok) {
		DebugPrint("SM2ECCSign: EC_KEY_set_private_key failed");
		EC_KEY_free(ec_key);
		return 1;
	}

	size_t outlen = 0;
	if (!SM2_decrypt_with_recommended(pPlainData, (size_t*)pulPlainDataLen, pCipherData, ulCipherDataLen, ec_key)) 
    {
		DebugPrint("SM2ECCDecrypt: SM2_decrypt_with_recommended failed");
		EC_KEY_free(ec_key);
		return 1;
	}
    else
    {
        DebugPrint("SM2ECCDecrypt: SM2_decrypt_with_recommended successfully");
		EC_KEY_free(ec_key);
		return 0;
    }
}
//SM3 interface
unsigned long SoftEncrypt::SM3HashTotal(
    unsigned char *pInData,
    unsigned long ulInDataLen,
    unsigned char *pHashData,
    unsigned long *pulHashDataLen)
{
    // int rv;
    // hSessionHandle_ = EVP_MD_CTX_new();
    // rv = EVP_DigestInit_ex((EVP_MD_CTX*)hSessionHandle_, md_, sm_engine_);
    // if (rv == 1)
    // {
    //     rv = EVP_DigestUpdate((EVP_MD_CTX*)hSessionHandle_, pInData, ulInDataLen);
    //     if (rv == 1)
    //     {
    //         rv = EVP_DigestFinal_ex((EVP_MD_CTX*)hSessionHandle_, pHashData, (unsigned int*)pulHashDataLen);
    //         if (rv == 1)
    //         {
    //             DebugPrint("SM3soft HashTotal successful!");
    //         }
    //         else DebugPrint("SM3soft HashTotal failed!");
    //     }
    //     else DebugPrint("SM3soft HashTotal update failed!");
    // }
    // else DebugPrint("SM3soft HashTotal initiate failed!");

    // EVP_MD_CTX_free((EVP_MD_CTX*)&hSessionHandle_);
    // return rv;

	// unsigned char dgst[SM3_DIGEST_LENGTH];
	// memset(dgst, 0, sizeof(dgst));
	// sm3(pInData, ulInDataLen, dgst);
	// memcpy(pHashData, dgst, SM3_DIGEST_LENGTH);
	// *pulHashDataLen = SM3_DIGEST_LENGTH;
    // return 1;
}
unsigned long SoftEncrypt::SM3HashInit(HANDLE *phSM3Handle)
{
    // int rv;
    // //OpenSession(hEkey_, phSM3Handle);
    
    // phSM3Handle = EVP_MD_CTX_new();
    // rv = EVP_DigestInit_ex(phSM3Handle, md_, sm_engine_);//(*phSM3Handle, SGD_SM3, NULL, NULL, 0);
    // if (rv)
    // {
    //     DebugPrint("EVP_DigestInit_ex() OK!");
    // }
    // else
    // {
    //     DebugPrint("EVP_DigestInit_ex() failed!");
    // } 
    // return !rv;

	// EVP_MD_CTX_init(phSM3Handle);
    // int  rv = EVP_DigestInit_ex(phSM3Handle, md_, sm_engine_);//(*phSM3Handle, SGD_SM3, NULL, NULL, 0);
    // if (rv)
    // {
    //     DebugPrint("EVP_DigestInit_ex() OK!");
    // }
    // else
    // {
    //     DebugPrint("EVP_DigestInit_ex() failed!");
    // } 
    // return !rv;
}

unsigned long SoftEncrypt::SM3HashFinal(void* phSM3Handle, unsigned char *pHashData, unsigned long *pulHashDataLen)
{
    // int rv;
    // if (nullptr != phSM3Handle)
    // {
    //     rv = EVP_DigestFinal_ex((EVP_MD_CTX*)&phSM3Handle, pHashData, (unsigned int*)pulHashDataLen);
    //     //rv = SDF_HashFinal(phSM3Handle, pHashData, (SGD_UINT32*)pulHashDataLen);
    //     if (rv)
    //     {
    //         DebugPrint("SM3soft Hash success!");
    //     }
    //     else
    //     {
    //         DebugPrint("SM3soft Hash failed!");
    //     }
    //     EVP_MD_CTX_free((EVP_MD_CTX*)&phSM3Handle);
    //     //ENGINE_finish(sm_engine);
    //     //ENGINE_free(sm_engine);
    //     return !rv;
    // }
    // else
    // {
    //     DebugPrint("SessionHandle is null, please check!");
    //     return -1;
    // }

    // int rv;
    // if (nullptr != phSM3Handle)
    // {
    //     rv = EVP_DigestFinal_ex((EVP_MD_CTX*)&phSM3Handle, pHashData, (unsigned int*)pulHashDataLen);
    //     //rv = SDF_HashFinal(phSM3Handle, pHashData, (SGD_UINT32*)pulHashDataLen);
    //     if (rv)
    //     {
    //         DebugPrint("SM3soft Hash success!");
    //     }
    //     else
    //     {
    //         DebugPrint("SM3soft Hash failed!");
    //     }

	// 	EVP_MD_CTX_cleanup((EVP_MD_CTX*)&phSM3Handle);
    //     //ENGINE_finish(sm_engine);
    //     //ENGINE_free(sm_engine);
    //     return !rv;
    // }
    // else
    // {
    //     DebugPrint("SessionHandle is null, please check!");
    //     return -1;
    // }
}
void SoftEncrypt::operator()(void* phSM3Handle, void const* data, std::size_t size) noexcept
{
    // int rv;
    // if (nullptr != phSM3Handle)
    // {
    //     rv = EVP_DigestUpdate((EVP_MD_CTX*)&phSM3Handle, data, size);
    //     //rv = SDF_HashUpdate(phSM3Handle, (SGD_UCHAR*)data, size);
    //     if (rv)
    //     {
    //         DebugPrint("SM3soft EVP_DigestUpdate() success!");
    //     }
    //     else
    //     {
    //         DebugPrint("SM3soft EVP_DigestUpdate() failed!");
    //     }
    // }
    // else
    // {
    //     DebugPrint("Context is null, please check!");
    // }
}

//SM4 Symetry Encrypt&Decrypt
unsigned long SoftEncrypt::SM4SymEncrypt(
    unsigned int  uiAlgMode,
	unsigned char *pSessionKey,
	unsigned long pSessionKeyLen,
	unsigned char *pPlainData,
	unsigned long ulPlainDataLen,
	unsigned char *pCipherData,
	unsigned long *pulCipherDataLen,
	int secKeyType)
{
    // int rv;  //Candidate algorithm:SGD_SMS4_ECB,SGD_SMS4_CBC,SGD_SM1_ECB,SGD_SM1_CBC
    // int tmplen = 0, clen = 0;

    // const EVP_CIPHER *cipher = ENGINE_get_cipher(sm_engine_, NID_sm4_ecb);
    // EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    // EVP_EncryptInit_ex(ctx, cipher, sm_engine_, key, NULL);
    // EVP_EncryptUpdate(ctx, pCipherData, (int*)pulCipherDataLen, pPlainData, ulPlainDataLen);
    // EVP_EncryptFinal_ex(ctx, pCipherData + *pulCipherDataLen, &tmplen);
    // *pulCipherDataLen += tmplen;
    // EVP_CIPHER_CTX_free(ctx);
    // //return clen;
}
unsigned long SoftEncrypt::SM4SymDecrypt(
    unsigned int  uiAlgMode,
	unsigned char *pSessionKey,
	unsigned long pSessionKeyLen,
	unsigned char *pCipherData,
	unsigned long ulCipherDataLen,
	unsigned char *pPlainData,
	unsigned long *pulPlainDataLen,
	int secKeyType)
{
    // int rv;
    
    // int tmplen = 0, plen = 0;

    // const EVP_CIPHER *cipher = ENGINE_get_cipher(sm_engine_, NID_sm4_ecb);
    // EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    // EVP_DecryptInit_ex(ctx, cipher, sm_engine_, key, NULL);
    // EVP_DecryptUpdate(ctx, pPlainData, (int*)pulPlainDataLen, pCipherData, ulCipherDataLen);
    // EVP_DecryptFinal_ex(ctx, pPlainData + *pulPlainDataLen, &tmplen);
    // pulPlainDataLen += tmplen;
    // EVP_CIPHER_CTX_free(ctx);
    // //return plen;
}

unsigned long SoftEncrypt::SM4GenerateSessionKey(
    unsigned char *pSessionKey,
    unsigned long *pSessionKeyLen)
{
    // int rv;
    // SGD_HANDLE hSM4GeHandle;
    // SGD_HANDLE hSessionKeyGeHandle;
    // OpenSession(hEkey_, &hSM4GeHandle);
    // rv = SDF_GenerateKeyWithKEK(hSM4GeHandle, SESSION_KEY_LEN * 8, SGD_SMS4_ECB, SESSION_KEY_INDEX, pSessionKey, (SGD_UINT32*)pSessionKeyLen, &hSessionKeyGeHandle);
    // if (rv != SDR_OK)
    // {
    //     if (hSessionKeyGeHandle != NULL)
    //     {
    //         hSessionKeyGeHandle = NULL;
    //     }
    //     DebugPrint("Generating SymmetryKey is failed, failed number:[0x%08x]", rv);
    // }
    // else
    // {
    //     DebugPrint("Generate SymmetryKey successfully!");
    // }
    // rv = SDF_DestroyKey(hSM4GeHandle, hSessionKeyGeHandle);
    // if (rv != SDR_OK)
    // {
    //     DebugPrint("Destroy SessionKey failed, failed number:[0x%08x]", rv);
    // }
    // else
    // {
    //     hSessionKeyGeHandle = NULL;
    //     DebugPrint("Destroy SessionKey successfully!");
    //     CloseSession(hSM4GeHandle);
    // }
    // return rv;
}

unsigned long SoftEncrypt::SM4SymEncryptEx(
    unsigned char *pPlainData,
    unsigned long ulPlainDataLen,
    unsigned char *pSessionKey,
    unsigned long *pSessionKeyLen,
    unsigned char *pCipherData,
    unsigned long *pulCipherDataLen
)
{
    // int rv;  //Candidate algorithm:SGD_SMS4_ECB,SGD_SMS4_CBC,SGD_SM1_ECB,SGD_SM1_CBC
    // SGD_HANDLE hSM4EnxHandle;
    // SGD_HANDLE hSessionKeyEnxHandle;
    // OpenSession(hEkey_, &hSM4EnxHandle);
    // rv = SDF_GenerateKeyWithKEK(hSM4EnxHandle, SESSION_KEY_LEN * 8, SGD_SMS4_ECB, SESSION_KEY_INDEX, pSessionKey, (SGD_UINT32*)pSessionKeyLen, &hSessionKeyEnxHandle);
    // if (rv == SDR_OK)//if (nullptr != hSessionKeyHandle)
    // {
    //     unsigned char pIv[16];
    //     memset(pIv, 0, 16);
    //     rv = SDF_Encrypt(hSM4EnxHandle, hSessionKeyEnxHandle, SGD_SMS4_ECB, pIv, pPlainData, ulPlainDataLen, pCipherData, (SGD_UINT32*)pulCipherDataLen);
    //     if (rv == SDR_OK)
    //     {
    //         DebugPrint("SM4 symetry encrypt successful!");
    //     }
    //     else
    //     {
    //         DebugPrint("SM4 symetry encrypt failed, failed number:[0x%08x]", rv);
    //     }
    // }
    // else
    // {
    //     DebugPrint("Importing SymmetryKey is failed, failed number:[0x%08x]", rv);
    //     //DebugPrint("SessionKey handle is unavailable,please check whether it was generated or imported.");
    // }

    // rv = SDF_DestroyKey(hSM4EnxHandle, hSessionKeyEnxHandle);
    // if (rv != SDR_OK)
    // {
    //     DebugPrint("Destroy SessionKey failed, failed number:[0x%08x]", rv);
    // }
    // else
    // {
    //     hSessionKeyEnxHandle = NULL;
    //     DebugPrint("Destroy SessionKey successfully!");
    //     CloseSession(hSM4EnxHandle);
    // }
    // return rv;
}

// void SoftEncrypt::standPubToSM2Pub(unsigned char* standPub, int standPubLen, ECCrefPublicKey& sm2Publickey)
// {
//     //sm2Publickey.bits = standPubLen;
//     sm2Publickey.bits = PUBLIC_KEY_BIT_LEN; //must be 256;
//     memcpy(sm2Publickey.x, standPub + 1, 32);
//     memcpy(sm2Publickey.y, standPub + 33, 32);
// }

// void SoftEncrypt::standPriToSM2Pri(unsigned char* standPri, int standPriLen, ECCrefPrivateKey& sm2Privatekey)
// {
//     if (standPriLen > 32)
//         return;
//     else
//     {
//         sm2Privatekey.bits = standPriLen*8;
//         memcpy(sm2Privatekey.D, standPri, standPriLen);
//     }
// }

#endif
#endif
