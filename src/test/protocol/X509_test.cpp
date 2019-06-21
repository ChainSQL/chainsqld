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
#include <ripple/crypto/X509.h>
#include <ripple/beast/unit_test.h>


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
			bool ret = genCsr(seed, sub, "e:\\keystore\\x509Req.pem", strExcept);

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
				"MIIBxzCCAW0CCQCKCYVfCt37gDAKBggqhkjOPQQDAjCBgzELMAkGA1UEBhMCQ04x\n" \
				"EDAOBgNVBAgMB0JlaUppbmcxCzAJBgNVBAcMAkJKMREwDwYDVQQKDAhQZWVyc2Fm\n" \
				"ZTELMAkGA1UECwwCUFMxEjAQBgNVBAMMCWx1amluZ2xlaTEhMB8GCSqGSIb3DQEJ\n" \
				"ARYSbHVsZWlncmVhdEAxNjMuY29tMB4XDTE5MDYyMDAzNDExNVoXDTE5MDcyMDAz\n" \
				"NDExNVowVjELMAkGA1UEBhMCQ0ExCzAJBgNVBAgMAkJDMRIwEAYDVQQHDAlWYW5j\n" \
				"b3V2ZXIxEjAQBgNVBAoMCUR5bmFtc29mdDESMBAGA1UEAwwJbG9jYWxob3N0MFYw\n" \
				"EAYHKoZIzj0CAQYFK4EEAAoDQgAE1JxW4bGF8b6JmuZqAu/Bf3jqb8U6+F4P5Uxu\n" \
				"i3+McaiLOReyWD+JlC4tflDg/Gs9fk8URyVSo4QUNZlZq/qBGzAKBggqhkjOPQQD\n" \
				"AgNIADBFAiEA1lyu3mn4jQCMhVQ47cgyFh8vT6pVKumi3xSbPCQqJXECIHUpuVAx\n" \
				"TSdbHT9HNiTykF3MnaJxx3Qdy0pqSK13ZdUZ\n" \
				"-----END CERTIFICATE-----";

			std::string strExcept;
			bool ret = verifyCert(certCA,certUser,strExcept);
			
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