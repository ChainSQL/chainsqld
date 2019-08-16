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

#include <BeastConfig.h>
#include <ripple/app/misc/ValidatorKeys.h>
#include <ripple/protocol/Seed.h>
#include <ripple/beast/unit_test.h>
#include <peersafe/crypto/X509.h>



namespace ripple {
	class X509_test : public beast::unit_test::suite
	{
		void testCreateCSR()
		{
			testcase("CreateCSR operations");
			auto seed = generateSeed("masterpassphrase");

			x509_subject sub = {
				"CN",
				"BeiJing",
				"BJ",
				"Dynamsoft",
				"localhost"
			};
			std::string strExcept;
			bool ret = genCsr(seed, sub, "x509Req.pem", strExcept);

			BEAST_EXPECT(ret);
		}

		void testVerifyCert()
		{
			testcase("VerifyCert operations");

			std::string certCA = 
				"-----BEGIN CERTIFICATE-----\n" \
				"MIICTTCCAfKgAwIBAgIJAIiphalrJ32sMAoGCCqGSM49BAMCMIGDMQswCQYDVQQG\n" \
				"EwJDTjEQMA4GA1UECAwHQmVpSmluZzELMAkGA1UEBwwCQkoxETAPBgNVBAoMCFBl\n" \
				"ZXJzYWZlMQswCQYDVQQLDAJQUzESMBAGA1UEAwwJbHVqaW5nbGVpMSEwHwYJKoZI\n" \
				"hvcNAQkBFhJsdWxlaWdyZWF0QDE2My5jb20wHhcNMTkwNjIwMDI0OTUwWhcNMjAw\n" \
				"NjE5MDI0OTUwWjCBgzELMAkGA1UEBhMCQ04xEDAOBgNVBAgMB0JlaUppbmcxCzAJ\n" \
				"BgNVBAcMAkJKMREwDwYDVQQKDAhQZWVyc2FmZTELMAkGA1UECwwCUFMxEjAQBgNV\n" \
				"BAMMCWx1amluZ2xlaTEhMB8GCSqGSIb3DQEJARYSbHVsZWlncmVhdEAxNjMuY29t\n" \
				"MFYwEAYHKoZIzj0CAQYFK4EEAAoDQgAEAcNcRTim1e0stbNOe2cWdvoVRPPKwlbB\n" \
				"3yR+oNlkUwRGCTbrjYAxBwwWn2528jRQ1RYRefsuvqTlLCPjGI8OKqNQME4wHQYD\n" \
				"VR0OBBYEFAJixvzALC1cApoBy/Khm0y4b964MB8GA1UdIwQYMBaAFAJixvzALC1c\n" \
				"ApoBy/Khm0y4b964MAwGA1UdEwQFMAMBAf8wCgYIKoZIzj0EAwIDSQAwRgIhAM60\n" \
				"j1yoNfXJIAmfcGHB2d2fFicicnnAoSD8HmW70jsZAiEAu3rRsORqtL8I0vk3gEy5\n" \
				"MPYmxVGbTMIIV6ur6TY4+80=\n" \
				"-----END CERTIFICATE-----";

			std::string certUser =
				"-----BEGIN CERTIFICATE-----\n" \
				"MIICPzCCAScCCQCfrMP2woqkuzANBgkqhkiG9w0BAQsFADBKMQswCQYDVQQGEwJD\n" \
				"TjELMAkGA1UECAwCQkoxCzAJBgNVBAcMAkJKMSEwHwYDVQQKDBhJbnRlcm5ldCBX\n" \
				"aWRnaXRzIFB0eSBMdGQwHhcNMTkwNzA0MTAyNTI3WhcNMTkwODAzMTAyNTI3WjBH\n" \
				"MQswCQYDVQQGEwJDTjELMAkGA1UECAwCQkoxCzAJBgNVBAcMAkJKMREwDwYDVQQK\n" \
				"DAhQZWVyc2FmZTELMAkGA1UEAwwCUkMwVjAQBgcqhkjOPQIBBgUrgQQACgNCAARX\n" \
				"pcbxeBum/eJRYfe1DKuwOB7+IXopv1OQpCNVXMilOUDScvo3H48IQQKvl0gcU2fe\n" \
				"RylZeuQd/tbvWw8FO59GMA0GCSqGSIb3DQEBCwUAA4IBAQAgEuPG/kbKKYV+9bVJ\n" \
				"T2dGRgFvElmJjnMp7fuxZSydu6P03Cg6wZ3iQf+VlFVkO0TZ65i4IFFx7vg2ln5Z\n" \
				"Bo2t/I/zM9DzMcsN7j/93WUTqCHtQa6N5YRn0gDoLe5cnmNo0BV+dggPn/s5d9J+\n" \
				"OsOWnNPfxP9/OCpX+kjL0BxHepDPK6OGwHA8Lcd2jfoz8TsHf0IU8wl6dqUENOaA\n" \
				"FBXuqf1R+SHSI92PRlitGGkQ+JmE/+Xe+2Bro5tIm+qnVJdRivzkbmYvMPJI4jSS\n" \
				"SJBfTI454wVsNqcYcoLbTdj9FmfaOcaenWZTPBPLDWJBIKu5UVW5HgJqHtxpys2K\n" \
				"chJy\n" \
				"-----END CERTIFICATE-----";

			std::vector<std::string> vecRootCa = { certCA };
                                    
			auto const seedSecretKey =
				generateSecretKey(KeyType::secp256k1, generateSeed("masterpassphrase"));
			auto const seedPublicKey =
				derivePublicKey(KeyType::secp256k1, seedSecretKey);
			auto retPB = toBase58(TokenType::TOKEN_ACCOUNT_PUBLIC, seedPublicKey);

			// compare  public keys from CA and sign
			PublicKey pubKey = getPublicKeyFromX509(certCA);
			BEAST_EXPECT(pubKey == seedPublicKey);

			std::string strExcept;
			bool ret = verifyCert(vecRootCa,certUser,strExcept);		
			BEAST_EXPECT(ret);

			//auto retPB2 = toBase58(TokenType::TOKEN_ACCOUNT_PUBLIC, pubKeyFromFile);
			//BEAST_EXPECT(pubKeyFromFile == seedPublicKey);
			//std::cout << "public key "<< strHex (seedPublicKey) << "  " << strHex(pubKeyFromFile) <<std::endl;

			std::string rootCert1;
			std::ifstream ifsPath1("root1.cert");
			rootCert1.assign(
				std::istreambuf_iterator<char>(ifsPath1),
				std::istreambuf_iterator<char>());

			std::string userCert1;
			std::ifstream ifsPathUser1("user1.cert");
			userCert1.assign(
				std::istreambuf_iterator<char>(ifsPathUser1),
				std::istreambuf_iterator<char>());

			BEAST_EXPECT(!rootCert1.empty());
			BEAST_EXPECT(!userCert1.empty());

			std::vector<std::string> vecRootCertFromFile = { rootCert1 };
			PublicKey pubKeyFromFile = getPublicKeyFromX509(userCert1);
			BEAST_EXPECT(pubKey == pubKeyFromFile);

			ret = verifyCert(vecRootCertFromFile, userCert1, strExcept);
			BEAST_EXPECT(ret);

		}

		void run() override
		{
			testCreateCSR();
			testVerifyCert();
		}
	};

	BEAST_DEFINE_TESTSUITE(X509, protocol, ripple);
}