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

#include <ripple/crypto/Base58Data.h>
#include <algorithm>
#include <fstream>
#include <sstream>   
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include "openssl.h"
#include <openssl/x509_vfy.h>
#include <openssl/ec.h>
#include "ripple/crypto/X509.h"
#include <ripple/crypto/GenerateDeterministicKey.h>

namespace ripple {
	
	//从文件读入到string里
	std::string readFileIntoString(const char * filename)
	{
		std::ifstream ifile(filename);
		std::ostringstream buf;
		char ch;
		while (ifile.get(ch))
			buf.put(ch);
		//返回与流对象buf关联的字符串
		return buf.str();
	}

	X509* readCertFromString(std::string const& cert)
	{
		BIO *bio_mem = BIO_new(BIO_s_mem());
		BIO_puts(bio_mem, cert.c_str());
		return PEM_read_bio_X509(bio_mem, NULL, NULL, NULL);
	}

	X509* readCertFromFile(const char* filename)
	{
		std::string clientCrt = readFileIntoString(filename);
		return readCertFromString(clientCrt);
	}

	bool verifyCert(std::string const& rootCert, std::string const& certStr,std::string & exception)
	{
		OpenSSL_add_all_algorithms();

		int ret = 0;
		X509_STORE * certChain = NULL;
		//cert chain context
		X509_STORE_CTX *ctx = NULL;
		
		X509_STORE *cerChain = X509_STORE_new();

		X509 * x509Client = readCertFromString(certStr);
		X509* x509Ca = readCertFromString(rootCert);

		certChain = X509_STORE_new();

		//we can add a lot of ca to the chain
		//for (int i = 0; i < nTrustCount; i++)
		//{
		//	X509 * rootCert = GetTrustCert();//这个函数openssl中没有，用来读取可以被信任的证书
		//	X509_STORE_add_cert(certChain, rootCert);
		//}
		ret = X509_STORE_add_cert(certChain, x509Ca);
		if (1 != ret)
		{
			exception = "X509_STORE_add_cert fail, ret = " + ret;
			goto EXIT;
		}
		//init context
		ctx = X509_STORE_CTX_new();
		//x509Client is the certificate to be verified
		ret = X509_STORE_CTX_init(ctx, certChain, x509Client, NULL);
		if (1 != ret)
		{
			exception = "X509_STORE_CTX_init fail, ret = " + ret;
			goto EXIT;
		}

		int nX509Verify = X509_verify_cert(ctx);
		if (1 != nX509Verify)
		{
			long nCode = X509_STORE_CTX_get_error(ctx);
			const char * pChError = X509_verify_cert_error_string(nCode);
			exception = pChError;
			goto EXIT;
		}
	EXIT:
		X509_free(x509Client);
		X509_free(x509Ca);

		X509_STORE_CTX_cleanup(ctx);
		X509_STORE_CTX_free(ctx);

		X509_STORE_free(certChain);

		return ret == 1;
	}

	bool genCsr(Seed const& seed, x509_subject const& sub,std::string const& reqPath, std::string & exception)
	{
		/* ---------------------------------------------------------- *
		 * These function calls initialize openssl for correct work.  *
		 * ---------------------------------------------------------- */
		OpenSSL_add_all_algorithms();
		ERR_load_BIO_strings();
		//ERR_load_crypto_strings();

		uint128 ui;
		std::memcpy(ui.data(), seed.data(), seed.size());

		auto privateKey = generateECPrivateKey(ui);
		auto publicKey = generateECPublicKey(ui);

		BIO               *outbio = NULL;
		EC_KEY            *myecc = EC_KEY_new();;
		EVP_PKEY          *pkey = NULL;
		//int               eccgrp;
		int             ret = 0;
		int             nVersion = 1;
		X509_REQ        *x509_req = NULL;
		X509_NAME       *x509_name = NULL;
		RSA             *tem = NULL;
		BIO             *out = NULL, *bio_err = NULL;


		EC_GROUP *group = EC_GROUP_new_by_curve_name(NID_secp256k1);
		EC_KEY_set_group(myecc, group);
		EC_GROUP_free(group);

		EC_KEY_set_private_key(myecc, privateKey.get());
		EC_KEY_set_public_key(myecc, publicKey.get());

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

		/* -------------------------------------------------------- *
		* For cert signing, we use  the OPENSSL_EC_NAMED_CURVE flag*
		* ---------------------------------------------------------*/
		EC_KEY_set_asn1_flag(myecc, OPENSSL_EC_NAMED_CURVE);

		///* -------------------------------------------------------- *
		//* Create the public/private EC key pair here               *
		//* ---------------------------------------------------------*/
		//if (!(EC_KEY_generate_key(myecc)))
		//	BIO_printf(outbio, "Error generating the ECC key.");

		/* -------------------------------------------------------- *
		* Converting the EC key into a PKEY structure let us       *
		* handle the key just like any other key pair.             *
		* ---------------------------------------------------------*/
		pkey = EVP_PKEY_new();
		if (!EVP_PKEY_assign_EC_KEY(pkey, myecc))
		{
			exception = "Error assigning ECC key to EVP_PKEY structure.";
			return false;
		}

		///* -------------------------------------------------------- *
		//* Now we show how to extract EC-specifics from the key     *
		//* ---------------------------------------------------------*/
		myecc = EVP_PKEY_get1_EC_KEY(pkey);
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
			exception = "X509_REQ_set_version faild ret = " + ret;
			goto free_all;
		}

		// 3. set subject of x509 req
		x509_name = X509_REQ_get_subject_name(x509_req);

		ret = X509_NAME_add_entry_by_txt(x509_name, "C", MBSTRING_ASC, (const unsigned char*)sub.country.c_str(), -1, -1, 0);
		if (ret != 1) {
			exception = "X509_NAME_add_entry_by_txt failed,ret=" + ret;
			goto free_all;
		}

		ret = X509_NAME_add_entry_by_txt(x509_name, "ST", MBSTRING_ASC, (const unsigned char*)sub.province.c_str(), -1, -1, 0);
		if (ret != 1) {
			goto free_all;
		}

		ret = X509_NAME_add_entry_by_txt(x509_name, "L", MBSTRING_ASC, (const unsigned char*)sub.city.c_str(), -1, -1, 0);
		if (ret != 1) {
			exception = "X509_NAME_add_entry_by_txt failed,ret=" + ret;
			goto free_all;
		}

		ret = X509_NAME_add_entry_by_txt(x509_name, "O", MBSTRING_ASC, (const unsigned char*)sub.organization.c_str(), -1, -1, 0);
		if (ret != 1) {
			exception = "X509_NAME_add_entry_by_txt failed,ret=" + ret;
			goto free_all;
		}

		ret = X509_NAME_add_entry_by_txt(x509_name, "CN", MBSTRING_ASC, (const unsigned char*)sub.common.c_str(), -1, -1, 0);
		if (ret != 1) {
			exception = "X509_NAME_add_entry_by_txt failed,ret=" + ret;
			goto free_all;
		}

		ret = X509_REQ_set_pubkey(x509_req, pkey);
		if (ret != 1) {
			exception = "X509_REQ_set_pubkey failed,ret=" + ret;
			goto free_all;
		}

		// 5. set sign key of x509 req
		ret = X509_REQ_sign(x509_req, pkey, EVP_sha256());    // return x509_req->signature->length
		if (ret <= 0) {
			exception = "X509_REQ_sign failed,ret=" + ret;
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
			exception = "PEM_write_bio_X509_REQ failed,ret=" + ret;
			goto free_all;
		}
		/* ---------------------------------------------------------- *
		* Free up all structures                                     *
		* ---------------------------------------------------------- */	// 6. free
	free_all:
		X509_REQ_free(x509_req);
		EVP_PKEY_free(pkey);
		EC_KEY_free(myecc);
		//BIO_free_all(outbio);
		return ret == 1;
	}
}