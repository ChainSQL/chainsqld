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

#include <BeastConfig.h>
#include <ripple/basics/contract.h>
#include <openssl/pem.h>
#include <peersafe/crypto/AES.h>

namespace ripple {

	// AES uses AES crypto algorithm.

	// A random IV is used to encrypt the message. If you need timestamps or need to tell the recipient
	// which key to use (his, yours, or ephemeral) you must add that data.
	// (Obviously, key information can't go in the encrypted portion anyway.)

	// Our ciphertext is all encrypted except the IV. The encrypted data decodes as follows:
	// 1) IV (unencrypted)
	// 2) Encrypted: Original plaintext
	// 3) Encrypted: Rest of block/padding

	// AES operations throw on any error such as a corrupt message or incorrect
	// key. They *must* be called in try/catch blocks.

	// Algorithmic choices:
#define ECIES_ENC_KEY_SIZE  (256/8)             // Encryption key size
#define ECIES_ENC_BLK_SIZE  (128/8)             // Encryption block size
#define ECIES_ENC_IV_TYPE   uint128             // Type used to hold IV


	//Blob encryptAES(Blob const& key, Blob const& plaintext)
	//{
	//	if (plaintext.size() == 0)
	//		Throw<std::runtime_error>("plaintext too short");

	//	ECIES_ENC_IV_TYPE iv;
	//	memcpy(iv.begin(), &(key.front()), ECIES_ENC_BLK_SIZE);

	//	EVP_CIPHER_CTX ctx;
	//	EVP_CIPHER_CTX_init(&ctx);

	//	if (EVP_EncryptInit_ex(&ctx, ECIES_ENC_ALGO, nullptr, &(key.front()), iv.begin()) != 1)
	//	{
	//		EVP_CIPHER_CTX_cleanup(&ctx);
	//		Throw<std::runtime_error>("init cipher ctx");
	//	}

	//	Blob out(plaintext.size() + ECIES_ENC_KEY_SIZE + ECIES_ENC_BLK_SIZE, 0);
	//	int len = 0, bytesWritten;

	//	// output IV
	//	memcpy(&(out.front()), iv.begin(), ECIES_ENC_BLK_SIZE);
	//	len = ECIES_ENC_BLK_SIZE;

	//	// encrypt/output plaintext
	//	bytesWritten = out.capacity() - len;
	//	assert(bytesWritten > 0);

	//	if (EVP_EncryptUpdate(&ctx, &(out.front()) + len, &bytesWritten, &(plaintext.front()), plaintext.size()) < 0)
	//	{
	//		EVP_CIPHER_CTX_cleanup(&ctx);
	//		Throw<std::runtime_error>("");
	//	}

	//	len += bytesWritten;

	//	// finalize
	//	bytesWritten = out.capacity() - len;

	//	if (EVP_EncryptFinal_ex(&ctx, &(out.front()) + len, &bytesWritten) < 0)
	//	{
	//		EVP_CIPHER_CTX_cleanup(&ctx);
	//		Throw<std::runtime_error>("encryption error");
	//	}

	//	len += bytesWritten;

	//	// Output contains: IV, encrypted data, encrypted padding
	//	assert(len <= (plaintext.size() + (2 * ECIES_ENC_BLK_SIZE)));
	//	assert(len >= (plaintext.size() + ECIES_ENC_BLK_SIZE)); // IV, data
	//	out.resize(len);
	//	EVP_CIPHER_CTX_cleanup(&ctx);
	//	return out;
	//}

	//Blob decryptAES(Blob const& key, Blob const& ciphertext)
	//{
	//	// minimum ciphertext = IV + HMAC + 1 block
	//	if (ciphertext.size() < ((2 * ECIES_ENC_BLK_SIZE)))
	//		Throw<std::runtime_error>("ciphertext too short");

	//	// extract IV
	//	ECIES_ENC_IV_TYPE iv;
	//	memcpy(iv.begin(), &(key.front()), ECIES_ENC_BLK_SIZE);

	//	int outlen = 0;
	//	// begin decrypting
	//	EVP_CIPHER_CTX ctx;
	//	EVP_CIPHER_CTX_init(&ctx);

	//	if (EVP_DecryptInit_ex(&ctx, ECIES_ENC_ALGO, nullptr, &(key.front()), iv.begin()) != 1)
	//	{
	//		EVP_CIPHER_CTX_cleanup(&ctx);
	//		Throw<std::runtime_error>("unable to init cipher");
	//	}

	//	// decrypt plaintext (after IV)
	//	Blob plaintext(ciphertext.size() - ECIES_ENC_BLK_SIZE);
	//	outlen = plaintext.size();

	//	if (EVP_DecryptUpdate(&ctx, &(plaintext.front()), &outlen,
	//		&(ciphertext.front()) + ECIES_ENC_BLK_SIZE,
	//		ciphertext.size() - ECIES_ENC_BLK_SIZE) != 1)
	//	{
	//		EVP_CIPHER_CTX_cleanup(&ctx);
	//		Throw<std::runtime_error>("unable to extract plaintext");
	//	}

	//	// decrypt padding
	//	int flen = 0;

	//	if (EVP_DecryptFinal(&ctx, &(plaintext.front()) + outlen, &flen) != 1)
	//	{
	//		EVP_CIPHER_CTX_cleanup(&ctx);
	//		Throw<std::runtime_error>("plaintext had bad padding");
	//	}

	//	plaintext.resize(flen + outlen);

	//	EVP_CIPHER_CTX_cleanup(&ctx);
	//	return plaintext;
	//}

	Blob encryptAES(Blob const& key, Blob const& plaintext)
	{
		if (plaintext.size() == 0)
			Throw<std::runtime_error>("plaintext too short");
		if (key.size() < ECIES_ENC_BLK_SIZE)
			Throw<std::runtime_error>("key too short");

		ECIES_ENC_IV_TYPE iv;
		memcpy(iv.begin(), &(key.front()), ECIES_ENC_BLK_SIZE);

		EVP_CIPHER_CTX ctx;
		EVP_CIPHER_CTX_init(&ctx);

		if (EVP_EncryptInit_ex(&ctx, EVP_aes_128_cbc(), nullptr, &(key.front()), iv.begin()) != 1)
		{
			EVP_CIPHER_CTX_cleanup(&ctx);
			Throw<std::runtime_error>("init cipher ctx");
		}

		Blob out(plaintext.size() + ECIES_ENC_KEY_SIZE + ECIES_ENC_BLK_SIZE, 0);
		int len = 0, bytesWritten;

		if (EVP_EncryptUpdate(&ctx, &(out.front()), &bytesWritten, &(plaintext.front()), plaintext.size()) < 0)
		{
			EVP_CIPHER_CTX_cleanup(&ctx);
			Throw<std::runtime_error>("");
		}

		len = bytesWritten;


		if (EVP_EncryptFinal_ex(&ctx, &(out.front()) + len, &bytesWritten) < 0)
		{
			EVP_CIPHER_CTX_cleanup(&ctx);
			Throw<std::runtime_error>("encryption error");
		}

		len += bytesWritten;

		out.resize(len);
		EVP_CIPHER_CTX_cleanup(&ctx);
		return out;
	}

	Blob decryptAES(Blob const& key, Blob const& ciphertext)
	{
		if (ciphertext.size() == 0)
			Throw<std::runtime_error>("ciphertext is empty");
		if (key.size() < ECIES_ENC_BLK_SIZE)
			Throw<std::runtime_error>("key too short");
		// extract IV
		ECIES_ENC_IV_TYPE iv;
		memcpy(iv.begin(), &(key.front()), ECIES_ENC_BLK_SIZE);

		int outlen = 0;
		// begin decrypting
		EVP_CIPHER_CTX ctx;
		EVP_CIPHER_CTX_init(&ctx);

		if (EVP_DecryptInit_ex(&ctx, EVP_aes_128_cbc(), nullptr, &(key.front()), iv.begin()) != 1)
		{
			EVP_CIPHER_CTX_cleanup(&ctx);
			Throw<std::runtime_error>("unable to init cipher");
		}

		Blob plaintext(ciphertext.size());

		if (EVP_DecryptUpdate(&ctx, &(plaintext.front()), &outlen,
			&(ciphertext.front()),ciphertext.size()) != 1)
		{
			EVP_CIPHER_CTX_cleanup(&ctx);
			Throw<std::runtime_error>("unable to extract plaintext");
		}

		// decrypt padding
		int flen = 0;

		if (EVP_DecryptFinal(&ctx, &(plaintext.front()) + outlen, &flen) != 1)
		{
			EVP_CIPHER_CTX_cleanup(&ctx);
			Throw<std::runtime_error>("plaintext had bad padding");
		}

		plaintext.resize(flen + outlen);

		EVP_CIPHER_CTX_cleanup(&ctx);
		return plaintext;
	}
} // ripple
