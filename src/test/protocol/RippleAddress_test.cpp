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
#include <ripple/protocol/digest.h>
#include <ripple/protocol/RippleAddress.h>
#include <ripple/protocol/RipplePublicKey.h>
#include <ripple/protocol/Serializer.h>
#include <ripple/protocol/types.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/Seed.h>
#include <ripple/basics/Log.h>
#include <test/jtx/TestSuite.h>
#include <ripple/protocol/PublicKey.h>
#include <ripple/protocol/SecretKey.h>
#include <ripple/beast/utility/rngfill.h>
#include <ripple/crypto/csprng.h>

namespace ripple {

class RippleAddress_test : public ripple::TestSuite
{
public:
	void testAes()
	{
		//Test AES
		uint256 digest;
		beast::rngfill(
			digest.data(),
			digest.size(),
			crypto_prng());
		Blob randomBlob;
		randomBlob.resize(digest.size());
		memcpy(&(randomBlob.front()), digest.data(), digest.size());

		std::string pass = "abcdefghijklmnopqrsthelloworldaa";
		Blob passBlob;
		passBlob.assign(pass.begin(), pass.end());
		Blob textBlob;
		std::string text = "hello,world";
		textBlob.assign(text.begin(), text.end());
		auto testStr = RippleAddress::encryptAES(passBlob, textBlob);
		auto testDes = RippleAddress::decryptAES(passBlob, testStr);
		std::cout << strHex(testStr) << std::endl;

		pass = "abcdefg";
		for (int i = 0; i < 32; i++)
		{
			if (i < pass.size())
				passBlob[i] = pass[i];
			else
				passBlob[i] = 32 - pass.size();
		}
		testStr = RippleAddress::encryptAES(passBlob, textBlob);
		std::cout << strHex(testStr) << std::endl;


		// Check account encryption.
		Blob vucTextSrc = strCopy("Hello, nurse,how are you!");
		auto vucTextCipher = RippleAddress::encryptAES(randomBlob, vucTextSrc);
		auto vucTextRecovered = RippleAddress::decryptAES(randomBlob, vucTextCipher);
		expect(vucTextSrc == vucTextRecovered, "Encrypt-decrypt failed.");

		beast::rngfill(
			digest.data(),
			digest.size(),
			crypto_prng());
		Blob fakeBlob;
		fakeBlob.resize(digest.size());
		memcpy(&(fakeBlob.front()), digest.data(), digest.size());
		vucTextRecovered = RippleAddress::decryptAES(fakeBlob, vucTextCipher);
		expect(vucTextSrc != vucTextRecovered, "Encrypt-decrypt failed.");
	}

    void run()
    {
		testAes();
        unsigned char temp[4] = {'t','e','s','t' };
        unsigned char sessionKey[16] = { '1','2','3','4','5','6','7','8','9','0','1','2','3','4','5','6'};
        unsigned char result1[512] = { 0 }, resultPlain[512] = {0};
        unsigned long resultLen = 512,resultPlainLen = 512;
        Blob passBlob = strCopy("test");
		HardEncrypt* hEObj = HardEncryptObj::getInstance();
		PublicKey rootPub(Slice(hEObj->getRootPublicKey().first, hEObj->getRootPublicKey().second));
		SecretKey rootSec(Slice(hEObj->getRootPrivateKey().first, hEObj->getRootPrivateKey().second));
        auto cipher = strUnHex(std::string("04000000C9E4BB38847C760D13C54EBC7A10EAB3206CF13278AB9134ABD85DFF8A2C90F289A60DDF074DAF310EF0ADB1DD284FE155311B48C7D33F696DFC6F02683976A1C697030900000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000061ED16716F8E91CE795ACCE7BA4F74DB3A5A7D0D34C49FF330C08DA30FE7FCBE"));
        //hEObj->SM4SymEncrypt(sessionKey,16,temp,4,result1,&resultLen);
        //hEObj->SM4SymDecrypt(sessionKey, 16, result1, resultLen, resultPlain, &resultPlainLen);
        std::string decryptRev = strHex(ripple::decrypt(cipher.first, rootSec));
        std::string result = strHex(ripple::encrypt(passBlob, rootPub));
        // Construct a seed.
        RippleAddress naSeed;

        expect (naSeed.setSeedGeneric ("masterpassphrase"));
        expect (naSeed.humanSeed () == "snoPBrXtMeMyMHUVTgbuqAfg1SUTb", naSeed.humanSeed ());

        // Create node public/private key pair
        RippleAddress naNodePublic    = RippleAddress::createNodePublic (naSeed);
        RippleAddress naNodePrivate   = RippleAddress::createNodePrivate (naSeed);

        expect (naNodePublic.humanNodePublic () == "n94a1u4jAz288pZLtw6yFWVbi89YamiC6JBXPVUj5zmExe5fTVg9", naNodePublic.humanNodePublic ());
        expect (naNodePrivate.humanNodePrivate () == "pnen77YEeUd4fFKG7iycBWcwKpTaeFRkW2WFostaATy1DSupwXe", naNodePrivate.humanNodePrivate ());

        // Check node signing.
        Blob vucTextSrc = strCopy ("Hello, nurse!");
        uint256 uHash   = sha512Half(makeSlice(vucTextSrc));
        Blob vucTextSig;

        naNodePrivate.signNodePrivate (uHash, vucTextSig);
        expect (naNodePublic.verifyNodePublic (uHash, vucTextSig, ECDSA::strict), "Verify failed.");

        // Construct a public generator from the seed.
        RippleAddress   generator     = RippleAddress::createGeneratorPublic (naSeed);

        expect (generator.humanGenerator () == "fhuJKrhSDzV2SkjLn9qbwm5AaRmrxDPfFsHDCP6yfDZWcxDFz4mt", generator.humanGenerator ());

        // Create ed25519 account public/private key pair.
        KeyPair keys = generateKeysFromSeed (KeyType::ed25519, naSeed);
        expectEquals (keys.publicKey.humanAccountPublic(), "aKGheSBjmCsKJVuLNKRAKpZXT6wpk2FCuEZAXJupXgdAxX5THCqR");

        // Check ed25519 account signing.
        vucTextSig = keys.secretKey.accountPrivateSign (vucTextSrc);

        expect (!vucTextSig.empty(), "ed25519 signing failed.");
        expect (keys.publicKey.accountPublicVerify (vucTextSrc, vucTextSig, ECDSA()), "ed25519 verify failed.");

        // Create account #0 public/private key pair.
        RippleAddress   naAccountPublic0    = RippleAddress::createAccountPublic (generator, 0);
        RippleAddress   naAccountPrivate0   = RippleAddress::createAccountPrivate (generator, naSeed, 0);

        expect (toBase58(calcAccountID(naAccountPublic0)) == "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh");
        expect (naAccountPublic0.humanAccountPublic () == "aBQG8RQAzjs1eTKFEAQXr2gS4utcDiEC9wmi7pfUPTi27VCahwgw", naAccountPublic0.humanAccountPublic ());

        // Create account #1 public/private key pair.
        RippleAddress   naAccountPublic1    = RippleAddress::createAccountPublic (generator, 1);
        RippleAddress   naAccountPrivate1   = RippleAddress::createAccountPrivate (generator, naSeed, 1);

        expect (toBase58(calcAccountID(naAccountPublic1)) == "r4bYF7SLUMD7QgSLLpgJx38WJSY12ViRjP");
        expect (naAccountPublic1.humanAccountPublic () == "aBPXpTfuLy1Bhk3HnGTTAqnovpKWQ23NpFMNkAF6F1Atg5vDyPrw", naAccountPublic1.humanAccountPublic ());

        // Check account signing.
        vucTextSig = naAccountPrivate0.accountPrivateSign (vucTextSrc);

        expect (!vucTextSig.empty(), "Signing failed.");
        expect (naAccountPublic0.accountPublicVerify (vucTextSrc, vucTextSig, ECDSA::strict), "Verify failed.");
        expect (!naAccountPublic1.accountPublicVerify (vucTextSrc, vucTextSig, ECDSA::not_strict), "Anti-verify failed.");
        expect (!naAccountPublic1.accountPublicVerify (vucTextSrc, vucTextSig, ECDSA::strict), "Anti-verify failed.");

        vucTextSig = naAccountPrivate1.accountPrivateSign (vucTextSrc);

        expect (!vucTextSig.empty(), "Signing failed.");
        expect (naAccountPublic1.accountPublicVerify (vucTextSrc, vucTextSig, ECDSA::strict), "Verify failed.");
        expect (!naAccountPublic0.accountPublicVerify (vucTextSrc, vucTextSig, ECDSA::not_strict), "Anti-verify failed.");
        expect (!naAccountPublic0.accountPublicVerify (vucTextSrc, vucTextSig, ECDSA::strict), "Anti-verify failed.");

        // Check account encryption.
        Blob vucTextCipher
            = naAccountPrivate0.accountPrivateEncrypt (naAccountPublic1, vucTextSrc);
        Blob vucTextRecovered
            = naAccountPrivate1.accountPrivateDecrypt (naAccountPublic0, vucTextCipher);

        expect (vucTextSrc == vucTextRecovered, "Encrypt-decrypt failed.");

        {
            RippleAddress nSeed;
            uint128 seed1, seed2;
            seed1.SetHex ("71ED064155FFADFA38782C5E0158CB26");
            nSeed.setSeed (seed1);
            expect (nSeed.humanSeed() == "shHM53KPZ87Gwdqarm1bAmPeXg8Tn",
                "Incorrect human seed");
            expect (nSeed.humanSeed1751() == "MAD BODY ACE MINT OKAY HUB WHAT DATA SACK FLAT DANA MATH",
                "Incorrect 1751 seed");
        }

		std::string secret = "sp5fghtJtpUorTwvof1NpDXAzNwf5";
		//auto publicKey = RippleAddress::getPublicKey(secret);
        auto publicKey = ripple::getPublicKey(secret);
		if (publicKey)
		{
			std::string str = "test message";
			//auto passBlob = RippleAddress::getPasswordCipher(strCopy(str), *publicKey);
            auto passBlob = ripple::encrypt(strCopy(str), *publicKey);
			auto strPass = strHex(passBlob);
			//auto secretKey = RippleAddress::getSecretKey(secret);
            auto secretKey = ripple::getSecretKey(secret);

			//auto plainText = RippleAddress::decryptPassword(passBlob, *secretKey);
            auto plainText = ripple::decrypt(passBlob, *secretKey);
			expect(str == strCopy(plainText));

			std::string cipher = "03ee9773d57dec03c92af9b8cf5e1a5c04abb19323e32bc98ff3b459283db899a00b0fe703e5fda785cdf0222b2babd947daef41677d7a339731a0111ed92270ff3a6e2ec2d27f94d2f3e04bb3a0af27ae4e640850cc6b42686e9df2d5ec7ebadd";
			//auto plainText2 = RippleAddress::decryptPassword(strUnHex(cipher).first, *secretKey);
            auto plainText2 = ripple::decrypt(strUnHex(cipher).first, *secretKey);
		}

		//using a fake secret to decrypt
		RippleAddress   naAccountPublic2 = RippleAddress::createAccountPublic(generator, 2);
		RippleAddress   naAccountPrivate2 = RippleAddress::createAccountPrivate(generator, naSeed, 2);
		Blob vucTextRecovered2
			= naAccountPrivate2.accountPrivateDecrypt(naAccountPublic0, vucTextCipher);
		expect(vucTextSrc != vucTextRecovered2, "Encrypt-decrypt failed.");

		//Test AES
		uint256 digest;
		beast::rngfill(
			digest.data(),
			digest.size(),
			crypto_prng());
		Blob randomBlob;
		randomBlob.resize(digest.size());
		memcpy(&(randomBlob.front()), digest.data(), digest.size());

		//encrypt random seed
		Blob keyCipher = naAccountPrivate0.accountPrivateEncrypt(naAccountPublic1, randomBlob);
		Blob keyRecovered
			= naAccountPrivate1.accountPrivateDecrypt(naAccountPublic0, keyCipher);
		expect(randomBlob == keyRecovered, "encrypt random seed failed.");
    }
};

//------------------------------------------------------------------------------

class RippleIdentifier_test : public beast::unit_test::suite
{
public:
    void run ()
    {
        testcase ("Seed");
        RippleAddress seed;
        expect (seed.setSeedGeneric ("masterpassphrase"));
        expect (seed.humanSeed () == "snoPBrXtMeMyMHUVTgbuqAfg1SUTb", seed.humanSeed ());

        testcase ("RipplePublicKey");
        RippleAddress deprecatedPublicKey (RippleAddress::createNodePublic (seed));
        expect (deprecatedPublicKey.humanNodePublic () ==
            "n94a1u4jAz288pZLtw6yFWVbi89YamiC6JBXPVUj5zmExe5fTVg9",
                deprecatedPublicKey.humanNodePublic ());
        RipplePublicKey publicKey = deprecatedPublicKey.toPublicKey();
        expect (publicKey.to_string() == deprecatedPublicKey.humanNodePublic(),
            publicKey.to_string());

        testcase ("Generator");
        RippleAddress generator (RippleAddress::createGeneratorPublic (seed));
        expect (generator.humanGenerator () ==
            "fhuJKrhSDzV2SkjLn9qbwm5AaRmrxDPfFsHDCP6yfDZWcxDFz4mt",
                generator.humanGenerator ());
    }
};

BEAST_DEFINE_TESTSUITE(RippleAddress,ripple_data,ripple);
BEAST_DEFINE_TESTSUITE(RippleIdentifier,ripple_data,ripple);

} // ripple
