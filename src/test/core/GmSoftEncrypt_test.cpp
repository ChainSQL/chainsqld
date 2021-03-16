//------------------------------------------------------------------------------
/*
	This file is part of chainsqld: https://github.com/ChainSQL/chainsqld
	Copyright (c) 2014-2020 peersafe Inc.

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

#include <test/jtx/Env.h>
#include <ripple/beast/utility/temp_dir.h>
#include <ripple/crypto/csprng.h>
#include <boost/filesystem.hpp>
#include <peersafe/gmencrypt/softencrypt/GmSoftEncrypt.h>
#include <iostream>
#include <fstream>
#include <streambuf>




namespace ripple {

	class GmSoftEncrypt_test : public beast::unit_test::suite
	{
		//
		
		const std::string publicSV1 = "6766232B284CDFD30CA92805A6894679C4251BD24F53E6599A63491792E5F8DBAE93C8965FA38F6356E7B0E92BAA8BA41DD75800DB24FF74D8B69CB51CB5748F";
		const std::string privateSV1 = "0927066C51E035640D9F2AFD9CE772ED37021C434D0F3FF3FC21751CB4955B37";
		const std::string plainSV1 = "8D80124A4CF8CEEA2F4735F5870DB929EF086F52D288B8612D2320BD96A986DE";
		const std::string signedDataSV1 = "44A24904CFE164BBD17F39577D7E4C016F75EDA62942731719B77CEA3193B0AB0E9406AE94A4181DE75AEFC27FB89C646D7686E430BBFCBDB96970A50C6D9B95";

		const std::string publicSV2 = "6766232B284CDFD30CA92805A6894679C4251BD24F53E6599A63491792E5F8DBAE93C8965FA38F6356E7B0E92BAA8BA41DD75800DB24FF74D8B69CB51CB5748F";
		const std::string privateSV2 = "0927066C51E035640D9F2AFD9CE772ED37021C434D0F3FF3FC21751CB4955B37";
		const std::string plainSV2 = "8D80124A4CF8CEEA2F4735F5870DB929EF086F52D288B8612D2320BD96A986DE";
		const std::string signedDataSV2 = "44A24904CFE164BBD17F39577D7E4C016F75EDA62942731719B77CEA3193B0AB0E9406AE94A4181DE75AEFC27FB89C646D7686E430BBFCBDB96970A50C6D9B95";

		const std::string publicSV3 = "688834A2835193137C978340BDAA4F8DA9E0782FB49135E7A535EEE1BE678D0BEE131C763CB33D1B59AA894A2DCA075EC48F58F9E467EC0F5E2EA626B3C25459";
		const std::string privateSV3 = "DF8826D764DA1D9206351B160878C4D4C7D96A880882CD1598C5D4D90C5AB0A0";
		const std::string plainSV3 = "0ACED8EA979482360D64BFB819C34E431737F3988DB41C7EF857C315CC579667";
		const std::string signedDataSV3 = "F87988AFE53689960A3742F8A5F2F55F8E71C72DCB4877F82A9F3CBDE7D5351EB717E03AC346229BAE098659831E65D9828CEC57B78BC74F15C5847C53624F73";


		const std::string publicED1  = "F4A5E131B246F3D884A64EE0FF105A73D240E2BDD5F2133AAAFFF5F346CFAEAA27C3AD6D91B8610C65152EFA1986C90FF455A56202CA5448661D7DA821A671FA";
		const std::string privateED1 = "32bbdc4cf266bf6d408c2a24354c7283c2b778cef491c60dfc27dd2ae2145681";
		const std::string plainED1   = "50ADA1717506D471868AAE03CC4A9CE5";
		const std::string cipherD1   = "321F58FCEEA341BD4639BC2E0A2519379A5ED490A41DF8CCE93AA1769C947E6A077ABBAA7C1385F04FF153E697E768AD639318A19F800DBF56C02660946BDC831917CD3545C17A4852ECADC1A8A1A26A3F28AC50BAAEF5F3A88BC886B39E06B29E3EC66BDB84761F9A498E3C7ED81986";
		

	//  ˽Կ16����: b845f330499c8a76b666c04f49988b020191d735119be7a454b5b15732dc18da
	//	��Կ16���� : 47a3198b768f336d7d70d7dc18fa4bdaa6054649e900af81c7de42a1f1773988fc719c7233bd6e22c79873b36067e6bde348534e50b9ef37d5f0b326efb0163289
	//	ǩ���������Ϊ : 50BCA5B46DFF36D487FF8F3672E05D78371898203D97D4F58AF87AD85DAF7FAED861BEA9800779596A254891CF2A71CDF994474F764134C98F428040D8B550D3

		void
			testSign()
		{
			testcase("sm2 sign");
			try
			{

				//SoftEncrypt softGM;

				////  sk 45F4FFFF06FF1095868565F7F2F688F8FFFFF7F581307011FF32F3F1FFFFF640
				////	pub4Verify 4711C19CD97B3219CF7D0D256F78C05BAF04642316BAF6ACDC32FBC41A4F63EDE3BF74954A88FF4CD7D642D220A763F6FD5C81B5BEE437E2125D474C839D5195B8
				////	digest FC41B0FC835338B989AD9CB951B805CA776006FD26D5A0B431360387D082DBC5
				////	sig 9D672E83DB1AAC5BDDA4FF38A686BB2111EECBBFF51190A5E0F5EB8B3C8D935F05369984914C80B7963EFAC3206792E113FEC7452DDDB8DED4A78141A8A8F638
				////	secretKey SM2ECCVerify ERR rv = 1

				//auto const publicKeyDe58 = parseBase58<PublicKey>(TOKEN_NODE_PUBLIC, "pEnUJumfgaBTYkDySCdBJPT8NbEq44LVvqpDBG1zqmpZE2tm8fZWCPYW3Dnd1ioNW9hgFzbsn1ywp5yHy7cnqkG9zeFj88E6");
				//std::cout << strHex(publicKeyDe58->slice()) << std::endl;
	

				//std::string privateKeyStrDe58 = decodeBase58Token("pc5uHtWptaUCf7KnupEbrDoM3EbM85xuj3wWf7c4Fsx9Jf5TiZU", TOKEN_NODE_PRIVATE);
				////std::cout << strHex(privateKeyStrDe58) << std::endl;

				//auto tempPri = ripple::strUnHex(privateSV1).first;
				//auto tempPub = ripple::strUnHex(publicSV1).first;
				//tempPub.insert(tempPub.begin(), 0x47);

				/*softGM.SM2GenECCKeyPair(0, 1, 256);
				auto tempPublickey = softGM.getPublicKey();
				auto tempPrivatekey = softGM.getPrivateKey();

				SecretKey sk(Slice(tempPri.data(), tempPri.size()));
				PublicKey pk(Slice(tempPub.data(), tempPub.size()));

				std::pair<int, int> pri4SignInfo = std::make_pair(1, 1);
				std::pair<unsigned char*, int> pri4Sign = std::make_pair((unsigned char*)sk.data(), sk.size());
				auto tmpPlain = ripple::strUnHex(plainSV1).first;

				for (int i = 0; i < 1000; i++) {
					unsigned char sig[256] = { 0 };
					size_t len = sizeof(sig);
					auto rs = softGM.SM2ECCSign(pri4SignInfo, pri4Sign, tmpPlain.data(), tmpPlain.size(), sig, (unsigned long*)&len, 0, 0);
					assert(rs == 0);

					std::pair<unsigned char*, int> pubVerify = std::make_pair((unsigned char*)pk.data(), pk.size());
					rs = softGM.SM2ECCVerify(pubVerify, tmpPlain.data(), tmpPlain.size(), sig, len, 0, 0);
					assert(rs == 0);

// 					std::cout << "pk:" << ripple::strHex(pk) << std::endl;
// 					std::cout << "sk:" << sk.to_string() << std::endl;
// 					std::cout << ripple::strHex(Blob(sig, sig + len)) << std::endl;
// 					auto ggSigned = ripple::strUnHex(signedDataSV1).first;
// 					auto tmpSigned = ripple::strUnHex("15E286185BAB5B1C25281FF5119A301AE12884F9237A5A97A8C278D0BF5926484296A74412AB4EF2DEFA16F012674917522B3558CF9C0821EC28202773BCDFD6").first;
				//	rs = softGM.SM2ECCVerify(pubVerify, tmpPlain.data(), tmpPlain.size(), ggSigned.data(), ggSigned.size(), 0, 0);
				//  rs = softGM.SM2ECCVerify(pubVerify, tmpPlain.data(), tmpPlain.size(), tmpSigned.data(), tmpSigned.size(), 0, 0);			
				}*/
				pass();
			}
			catch (std::exception&)
			{
				fail();
			}
		}


		void
			testSM2EncryptAndDecrypt()
		{
			testcase("sm2 encrypt and decrypt");
			try
			{

			/*	SoftEncrypt softGM;

				auto tempPri = ripple::strUnHex(privateED1).first;
				auto tempPub = ripple::strUnHex(publicED1).first;
				tempPub.insert(tempPub.begin(), 0x47);

				SecretKey sk(Slice(tempPri.data(), tempPri.size()));
				PublicKey pk(Slice(tempPub.data(), tempPub.size()));

				std::pair<int, int> pri4SignInfo = std::make_pair(1, 1);
				std::pair<unsigned char*, int> pri4Sign = std::make_pair((unsigned char*)sk.data(), sk.size());
				auto tmpPlain = ripple::strUnHex(plainSV1).first;

				std::pair<unsigned char*, int> pubVerify = std::make_pair((unsigned char*)pk.data(), pk.size());*/

				/*for (int i = 0; i < 1000; i++) {

					unsigned long recommendedPlainLen = 512;
					unsigned char *plain = new unsigned char[recommendedPlainLen];
					memset(plain, 0, recommendedPlainLen);
					auto cipher = ripple::strUnHex(cipherD1).first;
			
					auto rs = softGM.SM2ECCDecrypt(pri4SignInfo, pri4Sign, 
						(unsigned char*)&cipher[0], cipher.size(),
						plain, &recommendedPlainLen,false,0,0);

					std::cout << ripple::strHex(Blob(plain, plain + recommendedPlainLen)) << std::endl;
					assert(rs == 0);
				}*/

				pass();
			}
			catch (std::exception&)
			{
				fail();
			}
		}


		void testSM4EncryptAndDecrypt()
		{
			testcase("sm4 encrypt and decrypt");
			try
			{

			/*	const std::string sm4ECBPWD1   = "195DFB107315BB593FFE57BAB9F295F2";
				const std::string sm4ECBPlain1 = "7AB7711EF976C0CE516F135E45EADE848171DA25223CA76D79EDB61E0C7E6628";
				const std::string sm4ECBCipher1 = "A0C8EC1BA6A6F9E6E7134037170964927BB47695602C9C16126F980FFD6655FE6522B3CCE93DA7D8A4D5579C52298648";

				SoftEncrypt softGM;
				auto passBlob = ripple::strUnHex(sm4ECBPWD1).first;
				auto raw_blob = ripple::strUnHex(sm4ECBPlain1).first;
	
				const int plainPaddingMaxLen = 16;
				unsigned char* pCipherData = new unsigned char[raw_blob.size() + plainPaddingMaxLen];
				unsigned long cipherDataLen = raw_blob.size() + plainPaddingMaxLen;

				auto res = softGM.SM4SymEncrypt(GmEncrypt::ECB, passBlob.data(), passBlob.size(), raw_blob.data(), raw_blob.size(), pCipherData, &cipherDataLen, 1);
				assert(res == 0);

				std::cout << "sm4 cipher data :" << ripple::strHex(Blob(pCipherData, pCipherData + cipherDataLen)) << std::endl;

				auto testCipherBlob = ripple::strUnHex(sm4ECBCipher1).first;
				unsigned long plainLen = 0;	
				res = softGM.SM4SymDecrypt(GmEncrypt::ECB, passBlob.data(), passBlob.size(), testCipherBlob.data(), testCipherBlob.size(), raw_blob.data(), &plainLen, 1);
				assert(res == 0);
				std::cout << "sm4 plain data :" << ripple::strHex(Blob(raw_blob.data(), raw_blob.data() + plainLen)) << std::endl;*/
				pass();
			}
			catch (std::exception&)
			{
				fail();
			}
		}


	public:
		void run()
		{
			testSign();
			//testSM2EncryptAndDecrypt();
			//testSM4EncryptAndDecrypt();
		}
	};

	BEAST_DEFINE_TESTSUITE(GmSoftEncrypt, core, ripple);

}  // ripple
