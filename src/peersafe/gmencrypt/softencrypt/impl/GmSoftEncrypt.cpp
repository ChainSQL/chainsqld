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
    mergePublicXYkey(pubKeyUser_, pubKeyUserExt_);
    return std::make_pair(pubKeyUser_,sizeof(pubKeyUser_));
}
std::pair<unsigned char*, int> SoftEncrypt::getPrivateKey()
{
    priAndPubKey_->pkey.ec->pub_key;
    return std::make_pair(priKeyUserExt_.D,sizeof(priKeyUserExt_.D));
}
void SoftEncrypt::mergePublicXYkey(unsigned char* publickey, ECCrefPublicKey& originalPublicKey)
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
unsigned long SoftEncrypt::SM2GenECCKeyPair(
    unsigned long ulAlias,
    unsigned long ulKeyUse,
    unsigned long ulModulusLen)
{
    int ok = 0;
    EVP_PKEY_CTX *pkctx = NULL;

    if (pkctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL))
    {
        if (EVP_PKEY_keygen_init(pkctx))
        {
            if (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(pkctx, NID_sm2p256v1))
            {
                if (EVP_PKEY_keygen(pkctx, &priAndPubKey_))
                {
                    DebugPrint("SM2GenECCKeyPair-EVP_PKEY_keygen() successful!");
                    EVP_PKEY_CTX_free(pkctx);
                    return 0;
                }
                else
                {
                    DebugPrint("SM2GenECCKeyPair-EVP_PKEY_keygen() failed!");
                    if (priAndPubKey_)
                    {
                        EVP_PKEY_free(priAndPubKey_);
                        priAndPubKey_ = NULL;
                    }
                }
            }
            else DebugPrint("SM2GenECCKeyPair-EVP_PKEY_CTX_set_ec_paramgen_curve_nid() failed!");
        }
        else DebugPrint("SM2GenECCKeyPair-EVP_PKEY_keygen_init() failed!");
    }
    else DebugPrint("SM2GenECCKeyPair-EVP_PKEY_CTX_new_id() failed!");

    EVP_PKEY_CTX_free(pkctx);
    return -1;
}
//SM2 Sign&Verify
unsigned long SoftEncrypt::SM2ECCSign(
    std::pair<unsigned char*, int>& pri4Sign,
    unsigned char *pInData,
    unsigned long ulInDataLen,
    unsigned char *pSignValue,
    unsigned long *pulSignValueLen,
    unsigned long ulAlias,
    unsigned long ulKeyUse)
{
    unsigned char dgest[64];
    size_t dgest_len, sig_len = 0;
    EVP_PKEY_CTX *pkctx = NULL;

    sig_len = (size_t)EVP_PKEY_size(pkey);
    pkctx = EVP_PKEY_CTX_new(pkey, NULL);
    SM2_compute_message_digest(md_, md_, pInData, ulInDataLen, g_signId, strlen(g_signId), dgest, &dgest_len, EVP_PKEY_get0_EC_KEY(EVP_PKEY_CTX_get0_pkey(pkctx)));

    if (EVP_PKEY_sign_init(pkctx))
    {
        if (EVP_PKEY_sign(pkctx, pSignValue, (size_t*)pulSignValueLen, dgest, dgest_len))
        {
            DebugPrint("SM2soft sign successful!");
            return 0;
        }
        else
        {
            DebugPrint("SM2 sign-EVP_PKEY_sign failed!");
        }
    }
    else
    {
        DebugPrint("SM2 sign-EVP_PKEY_sign_init failed!");
    }

    EVP_PKEY_CTX_free(pkctx);
    return -1;
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
    unsigned char dgest[64];
    size_t dgest_len;
    EVP_PKEY_CTX *pkctx = NULL;
    int ret = 0;

    pkctx = EVP_PKEY_CTX_new(pkey, NULL);
    SM2_compute_message_digest(md_, md_, pInData, ulInDataLen, g_signId, strlen(g_signId), dgest, &dgest_len, EVP_PKEY_get0_EC_KEY(EVP_PKEY_CTX_get0_pkey(pkctx)));

    if (EVP_PKEY_verify_init(pkctx))
    {
        if ((ret = EVP_PKEY_verify(pkctx, pSignValue, ulSignValueLen, dgest, dgest_len)) != 1)
        {
            DebugPrint("SM2soft sig and verify success!");
            return 0;
        }
        else
        {
            DebugPrint("SM2soft EVP_PKEY_verify() failed!");
        }
    }
    else
    {
        DebugPrint("SM2soft EVP_PKEY_verify_init() failed!");
    }

    EVP_PKEY_CTX_free(pkctx);
    return -1;
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
    EVP_PKEY_CTX *pkctx = NULL;

    if (pkctx = EVP_PKEY_CTX_new(pkey, NULL))
    {
        if (EVP_PKEY_encrypt_init(pkctx))
        {
            if (EVP_PKEY_encrypt(pkctx, pCipherData, (size_t*)pulCipherDataLen, pPlainData, ulPlainDataLen))
            {
                DebugPrint("SM2soft encrypt successfully!");
                return 0;
            }
            else DebugPrint("SM2soft encrypt-EVP_PKEY_encrypt() failed!");
        }
        else DebugPrint("SM2soft encrypt-EVP_PKEY_encrypt_init() failed!");
    }
    else DebugPrint("SM2soft encrypt-EVP_PKEY_CTX_new() failed!");

    EVP_PKEY_CTX_free(pkctx);
    return -1;
}
unsigned long SoftEncrypt::SM2ECCDecrypt(
    std::pair<unsigned char*, int>& pri4Decrypt,
    unsigned char *pCipherData,
    unsigned long ulCipherDataLen,
    unsigned char *pPlainData,
    unsigned long *pulPlainDataLen,
    unsigned long ulAlias,
    unsigned long ulKeyUse)
{
    EVP_PKEY_CTX *pkctx = NULL;
    int ret = 0;

    if (pkctx = EVP_PKEY_CTX_new(pkey, NULL))
    {
        if (EVP_PKEY_decrypt_init(pkctx))
        {
            if (ret = EVP_PKEY_decrypt(pkctx, pPlainData, (size_t*)pulPlainDataLen, pCipherData, ulCipherDataLen))
            {
                DebugPrint("SM2soft decrypt successfully!");
                return 0;
            }
            else DebugPrint("SM2soft decrypt-EVP_PKEY_decrypt() failed!");
        }
        else DebugPrint("SM2soft decrypt-EVP_PKEY_decrypt_init() failed!");
    }
    else DebugPrint("SM2soft decrypt-EVP_PKEY_CTX_new() failed!");

    EVP_PKEY_CTX_free(pkctx);
    return -1;
}
//SM3 interface
unsigned long SoftEncrypt::SM3HashTotal(
    unsigned char *pInData,
    unsigned long ulInDataLen,
    unsigned char *pHashData,
    unsigned long *pulHashDataLen)
{
    int rv;
    hSessionHandle_ = EVP_MD_CTX_new();
    rv = EVP_DigestInit_ex((EVP_MD_CTX*)hSessionHandle_, md_, sm_engine_);
    if (rv == 1)
    {
        rv = EVP_DigestUpdate((EVP_MD_CTX*)hSessionHandle_, pInData, ulInDataLen);
        if (rv == 1)
        {
            rv = EVP_DigestFinal_ex((EVP_MD_CTX*)hSessionHandle_, pHashData, (unsigned int*)pulHashDataLen);
            if (rv == 1)
            {
                DebugPrint("SM3soft HashTotal successful!");
            }
            else DebugPrint("SM3soft HashTotal failed!");
        }
        else DebugPrint("SM3soft HashTotal update failed!");
    }
    else DebugPrint("SM3soft HashTotal initiate failed!");

    EVP_MD_CTX_free((EVP_MD_CTX*)&hSessionHandle_);
    return rv;
}
unsigned long SoftEncrypt::SM3HashInit(EVP_MD_CTX *phSM3Handle)
{
    int rv;
    //OpenSession(hEkey_, phSM3Handle);
    
    phSM3Handle = EVP_MD_CTX_new();
    rv = EVP_DigestInit_ex(phSM3Handle, md_, sm_engine_);//(*phSM3Handle, SGD_SM3, NULL, NULL, 0);
    if (rv)
    {
        DebugPrint("EVP_DigestInit_ex() OK!");
    }
    else
    {
        DebugPrint("EVP_DigestInit_ex() failed!");
    } 
    return !rv;
}

unsigned long SoftEncrypt::SM3HashFinal(void* phSM3Handle, unsigned char *pHashData, unsigned long *pulHashDataLen)
{
    int rv;
    if (nullptr != phSM3Handle)
    {
        rv = EVP_DigestFinal_ex((EVP_MD_CTX*)&phSM3Handle, pHashData, (unsigned int*)pulHashDataLen);
        //rv = SDF_HashFinal(phSM3Handle, pHashData, (SGD_UINT32*)pulHashDataLen);
        if (rv)
        {
            DebugPrint("SM3soft Hash success!");
        }
        else
        {
            DebugPrint("SM3soft Hash failed!");
        }
        EVP_MD_CTX_free((EVP_MD_CTX*)&phSM3Handle);
        //ENGINE_finish(sm_engine);
        //ENGINE_free(sm_engine);
        return !rv;
    }
    else
    {
        DebugPrint("SessionHandle is null, please check!");
        return -1;
    }
}
void SoftEncrypt::operator()(void* phSM3Handle, void const* data, std::size_t size) noexcept
{
    int rv;
    if (nullptr != phSM3Handle)
    {
        rv = EVP_DigestUpdate((EVP_MD_CTX*)&phSM3Handle, data, size);
        //rv = SDF_HashUpdate(phSM3Handle, (SGD_UCHAR*)data, size);
        if (rv)
        {
            DebugPrint("SM3soft EVP_DigestUpdate() success!");
        }
        else
        {
            DebugPrint("SM3soft EVP_DigestUpdate() failed!");
        }
    }
    else
    {
        DebugPrint("Context is null, please check!");
    }
}

//SM4 Symetry Encrypt&Decrypt
unsigned long SoftEncrypt::SM4SymEncrypt(
    unsigned char *pSessionKey,
    unsigned long pSessionKeyLen,
    unsigned char *pPlainData,
    unsigned long ulPlainDataLen,
    unsigned char *pCipherData,
    unsigned long *pulCipherDataLen)
{
    int rv;  //Candidate algorithm:SGD_SMS4_ECB,SGD_SMS4_CBC,SGD_SM1_ECB,SGD_SM1_CBC
    int tmplen = 0, clen = 0;

    const EVP_CIPHER *cipher = ENGINE_get_cipher(sm_engine_, NID_sm4_ecb);
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, cipher, sm_engine_, key, NULL);
    EVP_EncryptUpdate(ctx, pCipherData, (int*)pulCipherDataLen, pPlainData, ulPlainDataLen);
    EVP_EncryptFinal_ex(ctx, pCipherData + *pulCipherDataLen, &tmplen);
    *pulCipherDataLen += tmplen;
    EVP_CIPHER_CTX_free(ctx);
    //return clen;
}
unsigned long SoftEncrypt::SM4SymDecrypt(
    unsigned char *pSessionKey,
    unsigned long pSessionKeyLen,
    unsigned char *pCipherData,
    unsigned long ulCipherDataLen,
    unsigned char *pPlainData,
    unsigned long *pulPlainDataLen)
{
    int rv;
    
    int tmplen = 0, plen = 0;

    const EVP_CIPHER *cipher = ENGINE_get_cipher(sm_engine_, NID_sm4_ecb);
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, cipher, sm_engine_, key, NULL);
    EVP_DecryptUpdate(ctx, pPlainData, (int*)pulPlainDataLen, pCipherData, ulCipherDataLen);
    EVP_DecryptFinal_ex(ctx, pPlainData + *pulPlainDataLen, &tmplen);
    pulPlainDataLen += tmplen;
    EVP_CIPHER_CTX_free(ctx);
    //return plen;
}

unsigned long SoftEncrypt::SM4GenerateSessionKey(
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

unsigned long SoftEncrypt::SM4SymEncryptEx(
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

void SoftEncrypt::standPubToSM2Pub(unsigned char* standPub, int standPubLen, ECCrefPublicKey& sm2Publickey)
{
    //sm2Publickey.bits = standPubLen;
    sm2Publickey.bits = PUBLIC_KEY_BIT_LEN; //must be 256;
    memcpy(sm2Publickey.x, standPub + 1, 32);
    memcpy(sm2Publickey.y, standPub + 33, 32);
}

void SoftEncrypt::standPriToSM2Pri(unsigned char* standPri, int standPriLen, ECCrefPrivateKey& sm2Privatekey)
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
#endif
