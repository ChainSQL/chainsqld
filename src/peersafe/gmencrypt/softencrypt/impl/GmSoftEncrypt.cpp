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
    size_t pubLen = i2o_ECPublicKey(sm2Keypair_, NULL);
    if(pubLen != 0)
    {
        unsigned char *pubKeyUserTemp = new unsigned char[pubLen];
        unsigned char *pubKeyUser = pubKeyUserTemp;
        pubLen = i2o_ECPublicKey(sm2Keypair_, &pubKeyUserTemp);
        // pubKeyUser = pubKeyUserTemp;
        DebugPrint("pubLen: %d", pubLen);
        pubKeyUser_[0] = GM_ALG_MARK;
        memcpy(pubKeyUser_+1, pubKeyUser+1, pubLen-1);
        delete [] pubKeyUser; //cause addr of pubKeyUserTemp has been changed
        return std::make_pair(pubKeyUser_, pubLen);
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
    int priLen = BN_num_bytes(EC_KEY_get0_private_key(sm2Keypair_));
    // int priLen = i2d_ECPrivateKey(sm2Keypair_, NULL);
    
    if(0 == priLen)
    {
        return std::make_pair(nullptr, 0);
    }
    else
    {
        DebugPrint("private key Len: %d", priLen);
        unsigned char* priKeyUserTemp = new unsigned char[priLen];
        // unsigned char* priKeyUser = priKeyUserTemp;
        int priLen = BN_bn2bin(EC_KEY_get0_private_key(sm2Keypair_), priKeyUserTemp);
        memcpy(priKeyUser_, priKeyUserTemp, priLen);
        delete [] priKeyUserTemp;
        return std::make_pair(priKeyUser_, priLen);    
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
{
    srand(time(0));
	for (int i = 0; i < uiLength; i++) {
		pucRandomBuf[i] = rand();
	}
    return 0;
}
unsigned long SoftEncrypt::GenerateRandom2File(unsigned int uiLength, unsigned char * pucRandomBuf,int times)
{
    return 0;
}
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
{
    return std::make_pair(nullptr, 0);
}
std::pair<unsigned char*, int> SoftEncrypt::getECCNodeVerifyPubKey(unsigned char* publicKeyTemp, int keyIndex)
{
    return std::make_pair(nullptr, 0);
}
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
    if (SeckeyType::gmOutCard != pri4SignInfo.first)
	{
        return 1;
	}
    
    BIGNUM* bn = BN_bin2bn((const unsigned char *)(pri4Sign.first), (pri4Sign.second), nullptr);
	if (bn == nullptr) {
		DebugPrint("SM2ECCSign: BN_bin2bn failed");
		return 1;
	}
	
	EC_KEY* ec_key = EC_KEY_new_by_curve_name(NID_sm2p256v1);
	const bool ok = EC_KEY_set_private_key(ec_key, bn);
	BN_clear_free(bn);
	if (!ok) {
		DebugPrint("SM2ECCSign: EC_KEY_set_private_key failed");
		EC_KEY_free(ec_key);
		return 1;
	}

    int genRet = EC_KEY_generate_key(ec_key);
    if (0 == genRet) {
        DebugPrint("SM2GenECCKeyPair-EC_KEY_generate_key() failed!");
        return -1;
    }

    unsigned char dgst[EVP_MAX_MD_SIZE];
	unsigned int dgstlen;
    if(computeDigestWithSm2(ec_key, pInData, ulInDataLen, dgst, &dgstlen))
    {
        return 1;
    }
    // const EVP_MD *id_md = EVP_sm3();
    // const EVP_MD *msg_md = EVP_sm3();
    // if (!SM2_compute_id_digest(id_md, dgst, &dgstlen, ec_key)) {
	// 	return 1;
	// }
    // if (!SM2_compute_message_digest(id_md, msg_md, pInData, ulInDataLen, dgst, &dgstlen, ec_key)) {
	// 	return 1;
	// }
    // int priLen = BN_num_bytes(EC_KEY_get0_private_key(ec_key));
    // unsigned char* priKeyUserTemp = new unsigned char[priLen];
    // int priLen2 = BN_bn2bin(EC_KEY_get0_private_key(ec_key), priKeyUserTemp);
    // memcpy(priKeyUser_, priKeyUserTemp, priLen2);
    // delete [] priKeyUserTemp;

	/* sign */
	if (!SM2_sign(NID_undef, dgst, dgstlen, pSignValue, (unsigned int*)pulSignValueLen, ec_key))
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

    unsigned char dgst[EVP_MAX_MD_SIZE];
	unsigned int dgstlen;
    if(computeDigestWithSm2(pubkey, pInData, ulInDataLen, dgst, &dgstlen))
    {
        return 1;
    }

	/* verify */
	if (!SM2_verify(NID_undef, dgst, dgstlen, pSignValue, ulSignValueLen, pubkey)) {
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
    if(pInData == nullptr || pHashData == nullptr)
    {
        return 1;
    }
    else
    {
        sm3(pInData, ulInDataLen, pHashData);
	    *pulHashDataLen = SM3_DIGEST_LENGTH;
        return 0;
    }
}

unsigned long SoftEncrypt::SM3HashInit(HANDLE *phSM3Handle)
{
    // sm3_init((sm3_ctx_t*)*phSM3Handle);
    sm3_init(&sm3_ctx_);
    // *phSM3Handle = &ctx;
    if(phSM3Handle != nullptr)
    {
        *phSM3Handle = &sm3_ctx_
        DebugPrint("SM3HashInit() OK!");
        return 0;
    }
    else
    {
        DebugPrint("SM3HashInit() failed!");
        return 1;
    }
}

unsigned long SoftEncrypt::SM3HashFinal(void* phSM3Handle, unsigned char *pHashData, unsigned long *pulHashDataLen)
{
    if (nullptr != phSM3Handle)
    {
        sm3_final(&sm3_ctx_, pHashData);
        if (pHashData != nullptr)
        {
            *pulHashDataLen = SM3_DIGEST_LENGTH;
            memset(&sm3_ctx_, 0, sizeof(sm3_ctx_t));
            DebugPrint("sm3_final Hash success!");
            return 0;
        }
        else
        {
            DebugPrint("sm3_final Hash failed!");
            return -1;
        }
    }
    else
    {
        DebugPrint("SessionHandle is null, please check!");
        return -1;
    }
}
void SoftEncrypt::operator()(void* phSM3Handle, void const* data, std::size_t size) noexcept
{
    if (nullptr != phSM3Handle)
    {
        sm3_update(&sm3_ctx_, (const unsigned char*)data, size);
        DebugPrint("SM3soft sm3_update() success!");
    }
    else
    {
        DebugPrint("SessionHandle is null, please check!");
    }
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
	if (SeckeyType::gmInCard == secKeyType)
	{
        DebugPrint("soft sm4 encrypt, secKeyType must be gmOutCard, please check!");
        return -1;
	}
	else
	{
        sms4_key_t key;
        sms4_set_encrypt_key(&key, pSessionKey);
        unsigned char pIndata[16384];
        unsigned long nInlen = 1024;
        pkcs5Padding(pPlainData, ulPlainDataLen, pIndata, &nInlen);
		if (SM4AlgType::ECB == uiAlgMode)
		{
            sms4_ecb_encrypt(pIndata, pCipherData, &key, true);
			// rv = SM4ExternalSymEncrypt(SGD_SMS4_ECB, pSessionKey, pSessionKeyLen, pPlainData, ulPlainDataLen, pCipherData, pulCipherDataLen);
		}
		else if (SM4AlgType::CBC == uiAlgMode)
		{
            unsigned char pIv[16];
	        generateIV(uiAlgMode, pIv);
            sms4_cbc_encrypt(pIndata, pCipherData, nInlen, &key, pIv, true);
			// rv = SM4ExternalSymEncrypt(SGD_SMS4_CBC, pSessionKey, pSessionKeyLen, pPlainData, ulPlainDataLen, pCipherData, pulCipherDataLen);
		}
	}
	
	return 0;
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
	if (SeckeyType::gmInCard == secKeyType)
	{
		DebugPrint("soft sm4 decrypt, secKeyType must be gmOutCard, please check!");
        return -1;
	}
	else
	{
        sms4_key_t key;
        sms4_set_encrypt_key(&key, pSessionKey);
		if (SM4AlgType::ECB == uiAlgMode)
		{
            sms4_ecb_encrypt(pPlainData, pCipherData, (const sms4_key_t *)pSessionKey, false);
			// rv = SM4ExternalSymEncrypt(SGD_SMS4_ECB, pSessionKey, pSessionKeyLen, pPlainData, ulPlainDataLen, pCipherData, pulCipherDataLen);
		}
		else if (SM4AlgType::CBC == uiAlgMode)
		{
            unsigned char pIv[16];
	        generateIV(uiAlgMode, pIv);
            sms4_cbc_encrypt(pPlainData, pCipherData, ulCipherDataLen, (const sms4_key_t *)pSessionKey, pIv, false);
			// rv = SM4ExternalSymEncrypt(SGD_SMS4_CBC, pSessionKey, pSessionKeyLen, pPlainData, ulPlainDataLen, pCipherData, pulCipherDataLen);
		}
        unsigned char pOutdata[16384];
        unsigned long nOutlen = 1024;
        dePkcs5Padding(pOutdata, nOutlen, pPlainData, pulPlainDataLen);
        DebugPrint("SM4 symmetry decrypt successful!");
	}
	return 0;
}

unsigned long SoftEncrypt::SM4GenerateSessionKey(
    unsigned char *pSessionKey,
    unsigned long *pSessionKeyLen)
{
    return 0;
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
    return 0;
}

unsigned long SoftEncrypt::generateIV(unsigned int uiAlgMode, unsigned char * pIV)
{
	int rv = 0;
	if (pIV != NULL)
	{
		switch (uiAlgMode)
		{
		case CBC:
			memset(pIV, 0x00, 16);
			rv = GenerateRandom(16, pIV);
			if (rv)
			{
				DebugPrint("Generate random failed\n");
			}
			break;
		case ECB:
		default:
			memset(pIV, 0, 16);
			break;
		}
	}
	else rv = -1;
	return rv;
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
EC_KEY* SoftEncrypt::CreateEC(unsigned char *key, int is_public) {
    EC_KEY *ec_key = NULL;
    BIO *keybio = NULL;
    keybio = BIO_new_mem_buf(key, -1);
 
    if (keybio == NULL) {
        DebugPrint("%s", "[BIO_new_mem_buf]->key len=%d,Failed to Get Key", strlen((char *) key));
        return NULL;
    }
 
    if (is_public) {
        ec_key = PEM_read_bio_EC_PUBKEY(keybio, NULL, NULL, NULL);
    } else {
        ec_key = PEM_read_bio_ECPrivateKey(keybio, NULL, NULL, NULL);
    }
 
    if (ec_key == NULL) {
        DebugPrint("Failed to Get Key");
        return NULL;
    }
 
    return ec_key;
}

int SoftEncrypt::computeDigestWithSm2(EC_KEY* ec_key, unsigned char* pInData, unsigned long ulInDataLen, 
                                        unsigned char* dgst, unsigned int*dgstLen) 
{
    const EVP_MD *id_md = EVP_sm3();
    const EVP_MD *msg_md = EVP_sm3();
    if (!SM2_compute_id_digest(id_md, dgst, dgstLen, ec_key)) {
		return 1;
	}
    if (!SM2_compute_message_digest(id_md, msg_md, pInData, ulInDataLen, dgst, dgstLen, ec_key)) {
		return 1;
	}
    return 0;
}

#endif
#endif
