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
#include <ripple/basics/contract.h>
#include <iostream>

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

bool SoftEncrypt::getPublicKey(EC_KEY *sm2Keypair, std::vector<unsigned char>& pubKey)
{
    size_t pubLen = i2o_ECPublicKey(sm2Keypair, NULL);
    if(pubLen != 0)
    {
        pubKey.resize(pubLen);
        unsigned char * pubKeyUserTemp = pubKey.data();
        pubLen = i2o_ECPublicKey(sm2Keypair, &pubKeyUserTemp);
        DebugPrint("pubLen: %d", pubLen);
        pubKey[0] = GM_ALG_MARK;
        return true;
    }
    else return false;
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

// std::pair<unsigned char*, int> SoftEncrypt::getPrivateKey(EC_KEY *sm2Keypair, std::vector<unsigned char>& priKey)
bool SoftEncrypt::getPrivateKey(EC_KEY *sm2Keypair, std::vector<unsigned char>& priKey)
{
    int priLen = BN_num_bytes(EC_KEY_get0_private_key(sm2Keypair));
    // int priLen = i2d_ECPrivateKey(sm2Keypair_, NULL);

    if(priLen != 0)
    {
        DebugPrint("private key Len: %d", priLen);
        priKey.resize(priLen);
        int priLen = BN_bn2bin(EC_KEY_get0_private_key(sm2Keypair), priKey.data());
        (void)priLen;
        return true;
    }
    else return false;
}

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
    std::vector<unsigned char>& publicKey,
    std::vector<unsigned char>& privateKey,
    bool isRoot,
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
        EC_KEY *sm2Keypair = EC_KEY_new_by_curve_name(NID_sm2p256v1);
        if (NULL == sm2Keypair)
        {
            DebugPrint("SM2GenECCKeyPair-EC_KEY_new_by_curve_name() failed!");
            return -1;
        }

        int genRet = EC_KEY_generate_key(sm2Keypair);
        if (0 == genRet)
        {
            DebugPrint("SM2GenECCKeyPair-EC_KEY_generate_key() failed!");
            ret = -1;
        }
        else
        {
            if (!getPublicKey(sm2Keypair, publicKey) || !getPrivateKey(sm2Keypair, privateKey))
                ret = -1;
        }
        EC_KEY_free(sm2Keypair);
    }

    return ret;
}

bool SoftEncrypt::generatePubFromPri(
    const unsigned char* pPriUC,
    int priLen,
    std::vector<unsigned char>& publicKey)
{
    EC_POINT *pub_key = NULL;
    BIGNUM* bn = BN_bin2bn(pPriUC, priLen, nullptr);
    if (bn == nullptr)
    {
        DebugPrint("generatePubFromPri: BN_bin2bn failed");
        return false;
    }

    EC_KEY* pEcKey = EC_KEY_new_by_curve_name(NID_sm2p256v1);
    const bool ok = EC_KEY_set_private_key(pEcKey, bn);
    BN_clear_free(bn);

    if (!ok)
    {
        DebugPrint("generatePubFromPri: EC_KEY_set_private_key failed");
        EC_KEY_free(pEcKey);
        return false;
    }

    const EC_GROUP* pEcGroup = EC_KEY_get0_group(pEcKey);
    const BIGNUM* priv_key = EC_KEY_get0_private_key(pEcKey);
    if ((pub_key = EC_POINT_new(pEcGroup)) == NULL)
        return false;

    BN_CTX *ctx = BN_CTX_new();
    if(ctx == NULL) return false;
    if (!EC_POINT_mul(pEcGroup, pub_key, priv_key, NULL, NULL, ctx))
        return false;
    
    if (!EC_KEY_set_public_key(pEcKey, pub_key))
        return false;

    getPublicKey(pEcKey, publicKey);
    return true;
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
    std::vector<unsigned char>& signedDataV)
{
    int ret = 1;
    if (SeckeyType::gmOutCard != pri4SignInfo.first && pInData == nullptr)
	{
        return ret;
	}

    BIGNUM* bn = BN_bin2bn((const unsigned char *)(pri4Sign.first), (pri4Sign.second), nullptr);
	if (bn == nullptr) {
		DebugPrint("SM2ECCSign: BN_bin2bn failed");
		return ret;
	}

	EC_KEY* ec_key = EC_KEY_new_by_curve_name(NID_sm2p256v1);
	const bool ok = EC_KEY_set_private_key(ec_key, bn);
	BN_clear_free(bn);

	if (!ok) 
	{
		DebugPrint("SM2ECCSign: EC_KEY_set_private_key failed");
		EC_KEY_free(ec_key);
		return ret;
	}

	// if(!setPubfromPri(ec_key)) return 1;

    /* sign */
    unsigned char pSignValue[512] = { 0 };
    unsigned int uiSignedLen = 0;
	if (!SM2_sign(NID_undef, pInData, ulInDataLen, pSignValue, &uiSignedLen, ec_key))
	{
		DebugPrint("SM2ECCSign: SM2_sign failed!");
		EC_KEY_free(ec_key);
		return ret;
	}

	const unsigned char *pSignedBuf = pSignValue;
	ECDSA_SIG *sm2sig = d2i_ECDSA_SIG(NULL, &pSignedBuf, uiSignedLen);

	if (sm2sig == NULL) 
	{
		DebugPrint("SM2ECCSign: SM2_sign failed!");
		EC_KEY_free(ec_key);
		return ret;
	}

    const BIGNUM *sig_r, *sig_s;
    ECDSA_SIG_get0(sm2sig, &sig_r, &sig_s);
	unsigned int rSize = BN_num_bytes(sig_r);
	unsigned int sSize = BN_num_bytes(sig_s);
	if (rSize > 32 || sSize > 32) {	
		ECDSA_SIG_free(sm2sig);
		EC_KEY_free(ec_key);
		ripple::Throw <std::runtime_error>("The length of raw signature is wrong");
	}
						
	// left pad 0x0
	unsigned char signedTmp[64] = { 0 };
	unsigned char rTmp[32] = { 0 };
	unsigned char sTmp[32] = { 0 };

	rSize = BN_bn2bin(sig_r, rTmp);
	sSize = BN_bn2bin(sig_s, sTmp);

	memcpy(signedTmp + (32 - rSize), rTmp, rSize);
	memcpy(signedTmp + (64 - sSize), sTmp, sSize);
	// memcpy(pSignValue, signedTmp, 64);
    signedDataV.insert(signedDataV.end(), signedTmp, signedTmp+64);
	
    ret = 0;
    DebugPrint("SM2ECCSign: SM2 secret key sign successful!");
    
	ECDSA_SIG_free(sm2sig);
    EC_KEY_free(ec_key);
	return ret;
}
unsigned long SoftEncrypt::SM2ECCVerify(
    std::pair<unsigned char*, int>& pub4Verify,
    unsigned char *pInData,
    unsigned long ulInDataLen,
    unsigned char *pSignValue,
    unsigned long ulSignValueLen)
{
    int ret = 1;
    if(pInData == nullptr || pSignValue == nullptr)
    {
        return ret;
    }
	EC_KEY* pubkey = standPubToSM2Pub(pub4Verify.first, pub4Verify.second);
	if (pubkey == nullptr)
    {
        return ret;
	}

    if (ulSignValueLen == 64)
    {
        ECDSA_SIG *sm2sig = ECDSA_SIG_new();
        BIGNUM *sig_r = BN_new();
        BIGNUM *sig_s = BN_new();
        BN_bin2bn(pSignValue, 32, sig_r);
        BN_bin2bn(pSignValue + 32, 32, sig_s);
        ECDSA_SIG_set0(sm2sig, sig_r, sig_s);

        int derSigLen = i2d_ECDSA_SIG(sm2sig, NULL);
        unsigned char *derSigTemp = new unsigned char[derSigLen];
        unsigned char *derlSig = derSigTemp;
        derSigLen = i2d_ECDSA_SIG(sm2sig, &derSigTemp);

        /* verify */
        int verifyRet = SM2_verify(NID_undef, pInData, ulInDataLen, derlSig, derSigLen, pubkey);
        if (verifyRet != SM2_VERIFY_SUCCESS)
        {
            DebugPrint("SM2ECCSign: SM2_verify failed");
        }
        else
        {
            ret = 0;
            DebugPrint("SM2ECCSign: SM2 secret key verify successful!");
        }
        
        ECDSA_SIG_free(sm2sig);
        delete[] derlSig;
    }
    EC_KEY_free(pubkey);
    return ret;
}
//SM2 Encrypt&Decrypt
unsigned long SoftEncrypt::SM2ECCEncrypt(
    std::pair<unsigned char*, int>& pub4Encrypt,
    unsigned char * pPlainData,
    unsigned long ulPlainDataLen,
    std::vector<unsigned char>& cipherDataV)
{
    unsigned long ret = 1;
    if(pPlainData == nullptr)
    {
        return ret;
    }
    
    EC_KEY* pubkey = standPubToSM2Pub(pub4Encrypt.first, pub4Encrypt.second);
	if (pubkey == nullptr)
    {
        return ret;
	}

    size_t cipherDataTempLen;
    if (!SM2_encrypt_with_recommended((const unsigned char *)pPlainData, ulPlainDataLen,
                NULL, &cipherDataTempLen, pubkey)) 
    {
		DebugPrint("SM2ECCEncrypt: SM2_encrypt_with_recommended failed");
        EC_KEY_free(pubkey);
		return ret;
	}
    unsigned char* pCipherDataTemp = new unsigned char[cipherDataTempLen];
	if (!SM2_encrypt_with_recommended((const unsigned char *)pPlainData, ulPlainDataLen,
        pCipherDataTemp, &cipherDataTempLen, pubkey))
    {
		DebugPrint("SM2ECCEncrypt: SM2_encrypt_with_recommended failed");
        delete [] pCipherDataTemp;
        EC_KEY_free(pubkey);
		return ret;
	}
    else
    {
        std::vector<unsigned char> cipherDataVTemp(pCipherDataTemp, (pCipherDataTemp + cipherDataTempLen));
        cipherDataV.assign(cipherDataVTemp.begin(), cipherDataVTemp.end());
        // cipherReEncode(pCipherData, *pulCipherDataLen);
        ret = 0;
        delete [] pCipherDataTemp;
        DebugPrint("SM2ECCEncrypt: SM2_encrypt_with_recommended successfully");
        EC_KEY_free(pubkey);
		return ret;
    }
}
unsigned long SoftEncrypt::SM2ECCDecrypt(
    std::pair<int, int> pri4DecryptInfo,
    std::pair<unsigned char*, int>& pri4Decrypt,
    unsigned char *pCipherData,
    unsigned long ulCipherDataLen,
    std::vector<unsigned char>& plainDataV,
    bool isSymmertryKey,
    void* sm4Handle)
{
    unsigned long ret = 1;
    if (SeckeyType::gmOutCard != pri4DecryptInfo.first && pCipherData == nullptr)
    {
        return ret;
    }

	BIGNUM* bn = BN_bin2bn((const unsigned char *)(pri4Decrypt.first), (pri4Decrypt.second), nullptr);
	if (bn == nullptr) {
		DebugPrint("SM2ECCDecrypt: BN_bin2bn failed");
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

	// cipherReDecode(pCipherData, ulCipherDataLen);

	size_t msglen;
    if (!SM2_decrypt_with_recommended(pCipherData, ulCipherDataLen, NULL, &msglen, ec_key)) {
		
		DebugPrint("SM2ECCDecrypt: SM2_decrypt_with_recommended failed");
		EC_KEY_free(ec_key);
		return 1;
	}

	// if (msglen > tmpPlainDataLen) {
	// 	DebugPrint("msglen > tmpPlainDataLen");
	// 	EC_KEY_free(ec_key);
	// 	delete[] pCipherDataTemp;
	// 	return 1;
	// }

    unsigned char* pPlainData = new unsigned char[msglen];
    if (!SM2_decrypt_with_recommended(pCipherData, ulCipherDataLen, pPlainData, &msglen, ec_key))
	{
		DebugPrint("SM2ECCDecrypt2: SM2_decrypt_with_recommended failed");
		EC_KEY_free(ec_key);
        delete[] pPlainData;
		return 1;
	}
	else
	{
        std::vector<unsigned char> plainDataVTemp(pPlainData, pPlainData + msglen);
        plainDataV.assign(plainDataVTemp.begin(), plainDataVTemp.end());
		DebugPrint("SM2ECCDecrypt: SM2_decrypt_with_recommended successfully");
		EC_KEY_free(ec_key);
        delete[] pPlainData;
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
    sm3_ctx_t* psm3_ctx_tmp = new sm3_ctx_t;
    // sm3_init((sm3_ctx_t*)*phSM3Handle);
    sm3_init(psm3_ctx_tmp);
    // *phSM3Handle = &ctx;
    if(phSM3Handle != nullptr)
    {
        // *phSM3Handle = &sm3_ctx_;
        *phSM3Handle = psm3_ctx_tmp;
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
        // sm3_final(&sm3_ctx_, pHashData);
        sm3_final((sm3_ctx_t*)phSM3Handle, pHashData);
        if (pHashData != nullptr)
        {
            *pulHashDataLen = SM3_DIGEST_LENGTH;
            // memset(&sm3_ctx_, 0, sizeof(sm3_ctx_t));
            // memset(phSM3Handle, 0, sizeof(sm3_ctx_t));
            delete (sm3_ctx_t*)phSM3Handle;
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
        // sm3_update(&sm3_ctx_, (const unsigned char*)data, size);
        sm3_update((sm3_ctx_t*)phSM3Handle, (const unsigned char*)data, size);
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
        
        unsigned long nInlen = (ulPlainDataLen/16 + 1) * 16;
        unsigned char* pIndata = new unsigned char[nInlen];
        pkcs5Padding(pPlainData, ulPlainDataLen, pIndata, &nInlen);
		if (SM4AlgType::ECB == uiAlgMode)
		{
            int loopTimes = nInlen/16;
            // unsigned char* pCipherTemp = new unsigned char[nInlen];
            for (int i = 0; i<loopTimes; i++)
            {
                int offset = i*16;
                sms4_ecb_encrypt(pIndata + offset, pCipherData + offset, &key, true);
            }
            // memcpy(pCipherData, pCipherTemp, ulPlainDataLen);
            *pulCipherDataLen = nInlen;

			// rv = SM4ExternalSymEncrypt(SGD_SMS4_ECB, pSessionKey, pSessionKeyLen, pPlainData, ulPlainDataLen, pCipherData, pulCipherDataLen);
		}
		else if (SM4AlgType::CBC == uiAlgMode)
		{
            unsigned char pIv[16];
	        generateIV(uiAlgMode, pIv);
            sms4_cbc_encrypt(pIndata, pCipherData, nInlen, &key, pIv, true);
			// rv = SM4ExternalSymEncrypt(SGD_SMS4_CBC, pSessionKey, pSessionKeyLen, pPlainData, ulPlainDataLen, pCipherData, pulCipherDataLen);
		}
        delete [] pIndata;
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
    try
    {
        if (SeckeyType::gmInCard == secKeyType)
        {
            DebugPrint("soft sm4 decrypt, secKeyType must be gmOutCard, please check!");
            return -1;
        }
        else
        {
            if (0 != ulCipherDataLen % 16)
            {
                ripple::Throw <std::runtime_error>("The length of symmetry cipher is wrong");
            }
            sms4_key_t key;
            sms4_set_decrypt_key(&key, pSessionKey);
            unsigned char *pOutdata = new unsigned char[ulCipherDataLen];
            if (SM4AlgType::ECB == uiAlgMode)
            {
                int loopTimes = ulCipherDataLen / 16;
                for (int i = 0; i < loopTimes; i++)
                {
                    int offset = i * 16;
                    sms4_ecb_encrypt(pCipherData + offset, pOutdata + offset, &key, false);
                }
            }
            else if (SM4AlgType::CBC == uiAlgMode)
            {
                unsigned char pIv[16];
                generateIV(uiAlgMode, pIv);
                sms4_cbc_encrypt(pCipherData, pPlainData, ulCipherDataLen, &key, pIv, false);
                // rv = SM4ExternalSymEncrypt(SGD_SMS4_CBC, pSessionKey, pSessionKeyLen, pPlainData, ulPlainDataLen, pCipherData, pulCipherDataLen);
            }
            dePkcs5Padding(pOutdata, ulCipherDataLen, pPlainData, pulPlainDataLen);
            delete[] pOutdata;
            DebugPrint("SM4 symmetry decrypt successful!");
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
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

EC_KEY* SoftEncrypt::standPubToSM2Pub(unsigned char* standPub, int standPubLen)
{
    // unsigned char pubKeyUserTemp[PUBLIC_KEY_EXT_LEN] = { 0 };
    unsigned char *pubKeyUserTemp = new unsigned char[PUBLIC_KEY_EXT_LEN];
    unsigned char* pubKeyUserTempBack = pubKeyUserTemp;
    pubKeyUserTemp[0] = 4;
    memcpy(pubKeyUserTemp+1, standPub + 1, 64);

    EC_KEY* ecKey = EC_KEY_new_by_curve_name(NID_sm2p256v1);
	if (o2i_ECPublicKey(&ecKey, (const unsigned char**)&pubKeyUserTemp, PUBLIC_KEY_EXT_LEN) != nullptr){

		EC_KEY_set_conv_form(ecKey, POINT_CONVERSION_COMPRESSED);
	}
	else {
		EC_KEY_free(ecKey);
	}

    delete [] pubKeyUserTempBack; //cause addr of pubKeyUserTemp has been changed
    return ecKey;
}

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
    // const EVP_MD *id_md = EVP_sm3();
    // const EVP_MD *msg_md = EVP_sm3();
    // if (!SM2_compute_id_digest(id_md, dgst, dgstLen, ec_key)) {
	// 	return 1;
	// }
    // if (!SM2_compute_message_digest(id_md, msg_md, pInData, ulInDataLen, dgst, dgstLen, ec_key)) {
	// 	return 1;
	// }
    return 0;
}

void SoftEncrypt::cipherReEncode(unsigned char* pCipher, unsigned long cipherLen)
{
    unsigned long realCipherLen = cipherLen - 96;
    unsigned char* pRealCipher = new unsigned char[realCipherLen];
    memcpy(pRealCipher, pCipher + 64, realCipherLen);
    unsigned char* pC3 = new unsigned char[32];
    memcpy(pC3, pCipher + 64 + realCipherLen, 32);
    
    memcpy(pCipher + 64, pC3, 32);
    memcpy(pCipher + 96, pRealCipher, realCipherLen);
    delete []pRealCipher;
    delete []pC3;
}
void SoftEncrypt::cipherReDecode(unsigned char* pCipher, unsigned long cipherLen)
{
    unsigned long realCipherLen = cipherLen - 96;
    unsigned char* pRealCipher = new unsigned char[realCipherLen];
    memcpy(pRealCipher, pCipher + 96, realCipherLen);
    unsigned char* pC3 = new unsigned char[32];
    memcpy(pC3, pCipher + 64, 32);
    
    memcpy(pCipher + 64, pRealCipher, realCipherLen);
    memcpy(pCipher + 64 + realCipherLen, pC3, 32);
    delete []pRealCipher;
    delete []pC3;
}

#endif
