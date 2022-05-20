//------------------------------------------------------------------------------
/*
This file is part of rippled: https://github.com/ripple/rippled
Copyright (c) 2012, 2013 Ripple Labs Inc.

Permission to use, copy, modify, and/or distribute this software for any
purpose  with  or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include <algorithm>
#include <fstream>
#include <sstream>   

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/x509_vfy.h>
#include <openssl/ec.h>

#include <ripple/crypto/impl/openssl.h>
#include <ripple/crypto/GenerateDeterministicKey.h>
#include <peersafe/crypto/X509.h>


#include <boost/format.hpp> // boost::format

namespace ripple {

	std::string readFileIntoString(const char * filename)
	{
		std::ifstream ifile(filename);
		std::ostringstream buf;
		char ch;
		while (ifile.get(ch))
			buf.put(ch);

		return buf.str();
	}

	X509* readCertFromString(std::string const& cert)
	{
        X509* pCert = nullptr;
		BIO *bio_mem = BIO_new(BIO_s_mem());
		BIO_puts(bio_mem, cert.c_str());
        pCert = PEM_read_bio_X509(bio_mem, NULL, NULL, NULL);
        BIO_free_all(bio_mem);
        return pCert;
	}

	X509* readCertFromFile(const char* filename)
	{
		std::string clientCrt = readFileIntoString(filename);
		return readCertFromString(clientCrt);
	}

	//
	//  certStr  root certificate chain 
	//
	bool verifyCert(std::vector<std::string> const& vecRootCert, std::string const& certStr, std::string & exception)
	{
		//  OpenSSL_add_all_algorithms is not thread safe
		//static std::mutex m;
		//std::lock_guard lock{ m };

		//OpenSSL_add_all_algorithms();

		// verify vecRootCert validation
		bool bValidation = false;

		std::vector<X509*> vecX509RootCert;
		for (auto certChainStr : vecRootCert) {
			X509* x509Ca = readCertFromString(certChainStr);
			if (x509Ca) {
				vecX509RootCert.push_back(x509Ca);
				bValidation = true;
			}
		}

		if (!bValidation) {
			exception = "All of the rootCert  is  invalidation ";
			return false;
		}

		X509 * x509Client = readCertFromString(certStr);
		if (x509Client == nullptr) {
			exception = "certStr  is  invalidation ";
			return false;
		}

		int ret = 0;

		//cert chain context
		X509_STORE_CTX *ctx = NULL;
		X509_STORE *  certChain = X509_STORE_new();
		//init context
		ctx = X509_STORE_CTX_new();

		for (auto certChainStr : vecX509RootCert) {
			ret = X509_STORE_add_cert(certChain, certChainStr);
			if (1 != ret) {
				auto errID = ::ERR_get_error();
				char buf[256];
				::ERR_error_string_n(errID, buf, sizeof(buf));
				::ERR_clear_error();
				exception = (boost::format("X509_STORE_add_cert err_msg: %s , err_code:  %ld") % buf % errID).str();
				goto EXIT;
			}
		}

		//x509Client is the certificate to be verified
		ret = X509_STORE_CTX_init(ctx, certChain, x509Client, NULL);
		if (1 != ret)
		{
			exception = (boost::format("X509_STORE_CTX_init fail, ret = %d") % ret).str();
			goto EXIT;
		}

		ret = X509_verify_cert(ctx);
		if (1 != ret)
		{
			long nCode = X509_STORE_CTX_get_error(ctx);
			const char * pChError = X509_verify_cert_error_string(nCode);
			exception = (boost::format("X509 err_msg: %s , err_code:  %ld") % pChError %nCode).str();
			goto EXIT;
		}
	EXIT:
		X509_free(x509Client);

		for (auto certChainStr : vecX509RootCert) {
			X509_free(certChainStr);
		}

		X509_STORE_CTX_cleanup(ctx);
		X509_STORE_CTX_free(ctx);

		X509_STORE_free(certChain);

		return ret == 1;
	}


	Blob getBlob(const EC_POINT  *  ecPoint) 
	{
        if(CommonKey::chainAlgTypeG == KeyType::gmalg)
        {
            Blob result;
#ifdef SOFTENCRYPT
            EC_KEY* ecKey = EC_KEY_new_by_curve_name(NID_sm2p256v1);
            if (ecKey == nullptr)  Throw<std::runtime_error>("EC_KEY_new_by_curve_name(NID_sm2p256v1) failed");
            EC_KEY_set_conv_form(ecKey, POINT_CONVERSION_UNCOMPRESSED);
            if (EC_KEY_set_public_key(ecKey, ecPoint) <= 0)
                Throw<std::runtime_error>("EC_KEY_set_public_key() failed");

            size_t pubLen = i2o_ECPublicKey(ecKey, NULL);
            if(pubLen != 0)
            {
                result.resize(pubLen);
                unsigned char* ptr = &result[0];
                pubLen = i2o_ECPublicKey(ecKey, &ptr);
                result[0] = GM_ALG_MARK;
            }
            EC_KEY_free(ecKey);
#endif
            return result;
        }
        else
        {
            EC_KEY* key1 = EC_KEY_new_by_curve_name(NID_secp256k1);
            if (key1 == nullptr)  Throw<std::runtime_error>("EC_KEY_new_by_curve_name(NID_secp256k1) failed");
            EC_KEY_set_conv_form(key1, POINT_CONVERSION_COMPRESSED);

            openssl::ec_key key = openssl::ec_key((openssl::ec_key::pointer_t) key1);

            if (EC_KEY_set_public_key((EC_KEY*)key.get(), ecPoint) <= 0)
                Throw<std::runtime_error>("EC_KEY_set_public_key() failed");

            Blob result(33);
            std::uint8_t* ptr = &result[0];
            int const size = i2o_ECPublicKey((EC_KEY*)key.get(), &ptr);
            if(size <= 33) Throw<std::runtime_error>("get public key len is not 33");
            
            return result;
        }
	}

	PublicKey getPublicKeyFromX509(std::string const& certificate)
	{

		PublicKey  publicKey;
		EVP_PKEY * pkey = NULL;
		EC_KEY*   pEcKey = NULL;

		X509* x509Ca = readCertFromString(certificate);
		if (x509Ca) {

			pkey = X509_get_pubkey(x509Ca);
			if (pkey == nullptr) {
				X509_free(x509Ca);
				Throw<std::runtime_error>("X509_get_pubkey() failed");
			}

			pEcKey = EVP_PKEY_get1_EC_KEY(pkey);
			if (pEcKey == nullptr) {

				EVP_PKEY_free(pkey);
				X509_free(x509Ca);
				Throw<std::runtime_error>("EVP_PKEY_get1_EC_KEY() failed");
			}

			const EC_POINT*  ecPoint = EC_KEY_get0_public_key(pEcKey);
			Blob blob = getBlob(ecPoint);
			// PublicKey
			publicKey = PublicKey(makeSlice(blob));

		}
        
		EC_KEY_free(pEcKey);
		EVP_PKEY_free(pkey);
		X509_free(x509Ca);

		return publicKey;
	}

	std::string getSerialNumber(std::string const& certificate)
    {
        std::string ret;
        X509* x509Ca = readCertFromString(certificate);
        if (x509Ca)
        {
            ASN1_INTEGER* bs = NULL;
            char* res = NULL;
            BIGNUM* bn = NULL;
            bs = X509_get_serialNumber(x509Ca);
            if (bs->length == 0)
            {
                Throw<std::runtime_error>("X509_get_serialNumber() failed length=0");
                return ret;
            }
            bn = ASN1_INTEGER_to_BN(bs, NULL);
            res = BN_bn2hex(bn);
            ret = std::string(res);
            OPENSSL_free(res);
            res = NULL;
            BN_free(bn);
            bn = NULL;
            X509_free(x509Ca);
        }
		return ret;
    }

	bool genCsr(KeyType keyType, std::string const& seedStr, x509_subject const& sub, std::string const& reqPath, std::string & exception)
	{
		//  OpenSSL_add_all_algorithms is not thread safe
		//static std::mutex m;
		//std::lock_guard lock{ m };

		///* ---------------------------------------------------------- *
		//* These function calls initialize openssl for correct work.  *
		//* ---------------------------------------------------------- */
		//OpenSSL_add_all_algorithms();
		ERR_load_BIO_strings();
		//ERR_load_crypto_strings();
        
        EVP_PKEY        *pkey = NULL;
        int             ret = 0;
        int             nVersion = 1;
        X509_REQ        *x509_req  = NULL;
        X509_name_st    *x509_name = NULL;
        BIO             *out = NULL;
        
        EC_POINT *pub_key = NULL;
        BIGNUM* bn = NULL;
        EC_KEY* pEcKey = NULL;
        const EC_GROUP* pEcGroup = NULL;
        const BIGNUM* priv_key = NULL;
        BN_CTX *bnCtx = NULL;

        if (keyType == KeyType::gmalg)
        {
#ifdef SOFTENCRYPT
            std::string priKeyStrDe58 = decodeBase58Token(seedStr, TokenType::AccountSecret);
            if (priKeyStrDe58.empty())
            {
                goto free_all;
            }
            bn = BN_bin2bn((unsigned char*)priKeyStrDe58.c_str(), priKeyStrDe58.size(), nullptr);
            if (bn == nullptr)
            {
                goto free_all;
            }

            pEcKey = EC_KEY_new_by_curve_name(NID_sm2p256v1);
            if (!EC_KEY_set_private_key(pEcKey, bn))
            {
                goto free_all;
            }

            pEcGroup = EC_KEY_get0_group(pEcKey);
            priv_key = EC_KEY_get0_private_key(pEcKey);
            if ((pub_key = EC_POINT_new(pEcGroup)) == NULL)
                goto free_all;

            bnCtx = BN_CTX_new();
            if(bnCtx == NULL) goto free_all;
            if (!EC_POINT_mul(pEcGroup, pub_key, priv_key, NULL, NULL, bnCtx))
            {
                goto free_all;
            }
            if (!EC_KEY_set_public_key(pEcKey, pub_key))
                goto free_all;
            
            /* -------------------------------------------------------- *
            * Converting the EC key into a PKEY structure let us       *
            * handle the key just like any other key pair.             *
            * ---------------------------------------------------------*/
            pkey = EVP_PKEY_new();
            if (!EVP_PKEY_assign_EC_KEY(pkey, pEcKey))
            {
                exception = "Error assigning ECC key to EVP_PKEY structure.";
                goto free_all;
            }
#else
            goto free_all;
#endif
        }
        else
        {
            auto oSeed = parseBase58<Seed>(seedStr);
            if (!oSeed) {
                return  false;
            }
            uint128 ui;
            std::memcpy(ui.data(), (*oSeed).data(), (*oSeed).size());
            auto privateKey = generateECPrivateKey(ui);
            auto publicKey = generateECPublicKey(ui);
            
            EC_KEY   *myecc = EC_KEY_new();
            EC_GROUP *group = EC_GROUP_new_by_curve_name(NID_secp256k1);
            EC_KEY_set_group(myecc, group);
            EC_GROUP_free(group);
            
            EC_KEY_set_private_key(myecc, privateKey.get());
            EC_KEY_set_public_key(myecc, publicKey.get());
            
            /* -------------------------------------------------------- *
            * For cert signing, we use  the OPENSSL_EC_NAMED_CURVE flag*
            * ---------------------------------------------------------*/
            EC_KEY_set_asn1_flag(myecc, OPENSSL_EC_NAMED_CURVE);
            
            /* -------------------------------------------------------- *
            * Converting the EC key into a PKEY structure let us       *
            * handle the key just like any other key pair.             *
            * ---------------------------------------------------------*/
            pkey = EVP_PKEY_new();
            if (!EVP_PKEY_assign_EC_KEY(pkey, myecc))
            {
                exception = "Error assigning ECC key to EVP_PKEY structure.";
                goto free_all;
            }
        }
        
        //int eccgrp;

		/* ---------------------------------------------------------- *
		* Create the Input/Output BIO's.                             *
		* ---------------------------------------------------------- */
		//outbio = BIO_new(BIO_s_file());
		//outbio = BIO_new_fp(stdout, BIO_NOCLOSE);

		///* ---------------------------------------------------------- *
		//* Create a EC key structure, setting the group type from NID  *
		//* ---------------------------------------------------------- */
		//eccgrp = OBJ_txt2nid("secp256k1");
		//myecc = EC_KEY_new_by_curve_name(eccgrp);

		///* -------------------------------------------------------- *
		//* Create the public/private EC key pair here               *
		//* ---------------------------------------------------------*/
		//if (!(EC_KEY_generate_key(myecc)))
		//	BIO_printf(outbio, "Error generating the ECC key.");

		///* -------------------------------------------------------- *
		//* Now we show how to extract EC-specifics from the key     *
		//* ---------------------------------------------------------*/
//		myecc = EVP_PKEY_get1_EC_KEY(pkey);
		//const EC_GROUP *ecgrp = EC_KEY_get0_group(myecc);

		///* ---------------------------------------------------------- *
		//* Here we print the key length, and extract the curve type.  *
		//* ---------------------------------------------------------- */
		//BIO_printf(outbio, "ECC Key size: %d bit\n", EVP_PKEY_bits(pkey));
		//BIO_printf(outbio, "ECC Key type: %s\n", OBJ_nid2sn(EC_GROUP_get_curve_name(ecgrp)));

		///* ---------------------------------------------------------- *
		//* Here we print the private/public key data in PEM format.   *
		//* ---------------------------------------------------------- */
		//if (!PEM_write_bio_PrivateKey(outbio, pkey, NULL, NULL, 0, 0, NULL))
		//	BIO_printf(outbio, "Error writing private key data in PEM format");

		//if (!PEM_write_bio_PUBKEY(outbio, pkey))
		//	BIO_printf(outbio, "Error writing public key data in PEM format");

		// 2. set version of x509 req
		x509_req = X509_REQ_new();
		ret = X509_REQ_set_version(x509_req, nVersion);
		if (ret != 1) {
            exception = (boost::format("X509_REQ_set_version faild ret = %d") % ret).str();
			goto free_all;
		}

		// 3. set subject of x509 req
		x509_name = X509_REQ_get_subject_name(x509_req);

		ret = X509_NAME_add_entry_by_txt(x509_name, "C", MBSTRING_ASC, (const unsigned char*)sub.country.c_str(), -1, -1, 0);
		if (ret != 1) {
			exception = (boost::format("X509_NAME_add_entry_by_txt failed,ret = %d") % ret).str();
			goto free_all;
		}

		ret = X509_NAME_add_entry_by_txt(x509_name, "ST", MBSTRING_ASC, (const unsigned char*)sub.province.c_str(), -1, -1, 0);
		if (ret != 1) {
			goto free_all;
		}

		ret = X509_NAME_add_entry_by_txt(x509_name, "L", MBSTRING_ASC, (const unsigned char*)sub.city.c_str(), -1, -1, 0);
		if (ret != 1) {
			exception = (boost::format("X509_NAME_add_entry_by_txt failed,ret = %d") % ret).str();
			goto free_all;
		}

		ret = X509_NAME_add_entry_by_txt(x509_name, "O", MBSTRING_ASC, (const unsigned char*)sub.organization.c_str(), -1, -1, 0);
		if (ret != 1) {
			exception = (boost::format("X509_NAME_add_entry_by_txt failed,ret = %d") % ret).str();
			goto free_all;
		}

		ret = X509_NAME_add_entry_by_txt(x509_name, "CN", MBSTRING_ASC, (const unsigned char*)sub.common.c_str(), -1, -1, 0);
		if (ret != 1) {
			exception = (boost::format("X509_NAME_add_entry_by_txt failed,ret = %d") % ret).str();
			goto free_all;
		}

		ret = X509_REQ_set_pubkey(x509_req, pkey);
		if (ret != 1) {
			exception = (boost::format("X509_REQ_set_pubkey failed,ret = %d") % ret).str();
			goto free_all;
		}

		// 5. set sign key of x509 req
        // return x509_req->signature->length
        ret = X509_REQ_sign(x509_req, pkey, keyType == KeyType::gmalg ? EVP_sm3() : EVP_sha256());
		if (ret <= 0) {
			exception = (boost::format("X509_REQ_sign failed,ret = %d") % ret).str();
			goto free_all;
		}

		out = BIO_new_file(reqPath.c_str(), "w");
		if (out == nullptr)
		{
			exception = "BIO_new_file failed,reqPath invalid:" + reqPath;
			goto free_all;
		}
		ret = PEM_write_bio_X509_REQ(out, x509_req);
		if (ret != 1)
		{
			exception = (boost::format("PEM_write_bio_X509_REQ failed,ret = %d") % ret).str();
			goto free_all;
		}
		/* ---------------------------------------------------------- *
		* Free up all structures                                     *
		* ---------------------------------------------------------- */	// 6. free
	free_all:
        EC_POINT_clear_free(pub_key);
        BN_clear_free(bn);
        BN_CTX_free(bnCtx);
        
		X509_REQ_free(x509_req);
		EVP_PKEY_free(pkey);
		BIO_free_all(out);
		return ret == 1;
	}
}
