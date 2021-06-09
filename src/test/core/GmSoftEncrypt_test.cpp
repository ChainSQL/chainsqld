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

#include <ripple/beast/utility/temp_dir.h>
#include <ripple/crypto/csprng.h>
#include <boost/filesystem.hpp>
#include <fstream>
#include <iostream>
#include <peersafe/gmencrypt/softencrypt/GmSoftEncrypt.h>
#include <streambuf>
#include <test/jtx/Env.h>

namespace ripple {

class GmSoftEncrypt_test : public beast::unit_test::suite
{
    //

    const std::string publicSV1 =
        "6766232B284CDFD30CA92805A6894679C4251BD24F53E6599A63491792E5F8DBAE93C8"
        "965FA38F6356E7B0E92BAA8BA41DD75800DB24FF74D8B69CB51CB5748F";
    const std::string privateSV1 =
        "0927066C51E035640D9F2AFD9CE772ED37021C434D0F3FF3FC21751CB4955B37";
    const std::string plainSV1 =
        "8D80124A4CF8CEEA2F4735F5870DB929EF086F52D288B8612D2320BD96A986DE";
    const std::string signedDataSV1 =
        "44A24904CFE164BBD17F39577D7E4C016F75EDA62942731719B77CEA3193B0AB0E9406"
        "AE94A4181DE75AEFC27FB89C646D7686E430BBFCBDB96970A50C6D9B95";

    const std::string publicSV2 =
        "6766232B284CDFD30CA92805A6894679C4251BD24F53E6599A63491792E5F8DBAE93C8"
        "965FA38F6356E7B0E92BAA8BA41DD75800DB24FF74D8B69CB51CB5748F";
    const std::string privateSV2 =
        "0927066C51E035640D9F2AFD9CE772ED37021C434D0F3FF3FC21751CB4955B37";
    const std::string plainSV2 =
        "8D80124A4CF8CEEA2F4735F5870DB929EF086F52D288B8612D2320BD96A986DE";
    const std::string signedDataSV2 =
        "44A24904CFE164BBD17F39577D7E4C016F75EDA62942731719B77CEA3193B0AB0E9406"
        "AE94A4181DE75AEFC27FB89C646D7686E430BBFCBDB96970A50C6D9B95";

    const std::string publicSV3 =
        "688834A2835193137C978340BDAA4F8DA9E0782FB49135E7A535EEE1BE678D0BEE131C"
        "763CB33D1B59AA894A2DCA075EC48F58F9E467EC0F5E2EA626B3C25459";
    const std::string privateSV3 =
        "DF8826D764DA1D9206351B160878C4D4C7D96A880882CD1598C5D4D90C5AB0A0";
    const std::string plainSV3 =
        "0ACED8EA979482360D64BFB819C34E431737F3988DB41C7EF857C315CC579667";
    const std::string signedDataSV3 =
        "F87988AFE53689960A3742F8A5F2F55F8E71C72DCB4877F82A9F3CBDE7D5351EB717E0"
        "3AC346229BAE098659831E65D9828CEC57B78BC74F15C5847C53624F73";

    const std::string publicED1 =
        "F4A5E131B246F3D884A64EE0FF105A73D240E2BDD5F2133AAAFFF5F346CFAEAA27C3AD"
        "6D91B8610C65152EFA1986C90FF455A56202CA5448661D7DA821A671FA";
    const std::string privateED1 =
        "32bbdc4cf266bf6d408c2a24354c7283c2b778cef491c60dfc27dd2ae2145681";
    const std::string plainED1 = "50ADA1717506D471868AAE03CC4A9CE5";
    const std::string cipherD1 =
        "321F58FCEEA341BD4639BC2E0A2519379A5ED490A41DF8CCE93AA1769C947E6A077ABB"
        "AA7C1385F04FF153E697E768AD639318A19F800DBF56C02660946BDC831917CD3545C1"
        "7A4852ECADC1A8A1A26A3F28AC50BAAEF5F3A88BC886B39E06B29E3EC66BDB84761F9A"
        "498E3C7ED81986";

    void
    testSign()
    {
        testcase("sm2 sign");
        try
        {
            SoftEncrypt softGM;

            auto tempPri = *(ripple::strUnHex(privateSV1));
            auto tempPub = *(ripple::strUnHex(publicSV1));
            tempPub.insert(tempPub.begin(), 0x47);

            SecretKey sk(Slice(tempPri.data(), tempPri.size()));
            PublicKey pk(Slice(tempPub.data(), tempPub.size()));

            std::pair<int, int> pri4SignInfo = std::make_pair(1, 1);
            std::pair<unsigned char*, int> pri4Sign =
                std::make_pair((unsigned char*)sk.data(), sk.size());


            std::pair<unsigned char*, int> pub4Verify = std::make_pair((unsigned char*)&tempPub[0], tempPub.size());

            auto tmpPlain = *(ripple::strUnHex(plainSV1));

            for (int i = 0; i < 1000; i++)
            {
                std::vector<unsigned char> signedBufV;
                auto rs = softGM.SM2ECCSign(pri4SignInfo, pri4Sign, tmpPlain.data(), tmpPlain.size(), signedBufV);
                assert(rs == 0);

                rs = softGM.SM2ECCVerify(pub4Verify, tmpPlain.data(), tmpPlain.size(), signedBufV.data(),signedBufV.size());
                assert(rs == 0);

            }
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
            	SoftEncrypt softGM;

                auto tempPri = *(ripple::strUnHex(privateSV1));
                auto tempPub = *(ripple::strUnHex(publicSV1));
                tempPub.insert(tempPub.begin(), 0x47);

                SecretKey sk(Slice(tempPri.data(), tempPri.size()));
                PublicKey pk(Slice(tempPub.data(), tempPub.size()));

                std::pair<int, int> pri4SignInfo = std::make_pair(1, 1);
                std::pair<unsigned char*, int> pri4Sign =
                    std::make_pair((unsigned char*)sk.data(), sk.size());

                std::pair<unsigned char*, int> pub4Verify =
                    std::make_pair((unsigned char*)&tempPub[0], tempPub.size());

                auto tmpPlain = *(ripple::strUnHex(plainSV1));


                std::vector<unsigned char> cipherBufV;
                auto rs = softGM.SM2ECCEncrypt(pub4Verify, (unsigned char*)&tmpPlain[0], tmpPlain.size(), cipherBufV);
                assert(rs == 0);

                std::vector<unsigned char> resultVec;
                rs = softGM.SM2ECCDecrypt(pri4SignInfo, pri4Sign, cipherBufV.data(), cipherBufV.size(), resultVec,false,nullptr);
                assert(rs == 0);
                rs = memcmp(resultVec.data(), tmpPlain.data(), tmpPlain.size());
                assert(rs == 0);
  
            pass();
        }
        catch (std::exception&)
        {
            fail();
        }
    }


    void
    testSM3()
    {
        testcase("sm3");
        try
        {
            SoftEncrypt softGM;

            unsigned char hashData[32] = {0};
            unsigned long hashDataLen = 32;

            const std::string sm3Plain = "50ADA1717506D471868AAE03CC4A9CE5";
            const std::string sm3Hash  = "BB716360AB09526158FBB0D2BBC9978EE6048BCC9FB7BE0C8131C6EB4855379B";

            auto tmpPlain = *(ripple::strUnHex(sm3Plain));

            auto rs = softGM.SM3HashTotal(
                tmpPlain.data(), tmpPlain.size(), hashData, &hashDataLen);
            assert(rs == 0);

            auto sm3Hex = ripple::strHex(Blob(hashData, hashData + hashDataLen));

             /*std::cout << "sm3Hex:" << sm3Hex << std::endl;*/
            assert( sm3Hash.compare(sm3Hash) == 0);

            pass();
        }
        catch (std::exception&)
        {
            fail();
        }
    }


    void
    testSM4EncryptAndDecrypt()
    {
        testcase("sm4 encrypt and decrypt");
        try
        {
            const std::string sm4ECBPWD1 = "195DFB107315BB593FFE57BAB9F295F2";
            const std::string sm4ECBPlain1 =
                "7AB7711EF976C0CE516F135E45EADE848171DA25223CA76D79EDB61E0C7E66"
                "28";
            const std::string sm4ECBCipher1 =
                "A0C8EC1BA6A6F9E6E7134037170964927BB47695602C9C16126F980FFD6655"
                "FE6522B3CCE93DA7D8A4D5579C52298648";

            SoftEncrypt softGM;
            auto passBlob = *(ripple::strUnHex(sm4ECBPWD1));
            auto raw_blob = *(ripple::strUnHex(sm4ECBPlain1));

            const int plainPaddingMaxLen = 16;
            unsigned char* pCipherData =
                new unsigned char[raw_blob.size() + plainPaddingMaxLen];
            unsigned long cipherDataLen = raw_blob.size() + plainPaddingMaxLen;

            auto res = softGM.SM4SymEncrypt(
                GmEncrypt::ECB,
                passBlob.data(),
                passBlob.size(),
                raw_blob.data(),
                raw_blob.size(),
                pCipherData,
                &cipherDataLen,
                1);
            assert(res == 0);

            auto cipherHex = ripple::strHex(Blob(pCipherData, pCipherData + cipherDataLen));
          
            //std::cout << "sm4 cipher data :"
            //          << cipherHex
            //          << std::endl;
            assert( cipherHex.compare(sm4ECBCipher1) == 0 );


            auto testCipherBlob = *(ripple::strUnHex(sm4ECBCipher1));
            unsigned long plainLen = 0;
            res = softGM.SM4SymDecrypt(
                GmEncrypt::ECB,
                passBlob.data(),
                passBlob.size(),
                testCipherBlob.data(),
                testCipherBlob.size(),
                raw_blob.data(),
                &plainLen,
                1);
            assert(res == 0);

            auto plainHex = ripple::strHex(
                Blob(raw_blob.data(), raw_blob.data() + plainLen));

            //std::cout << "sm4 plain data :"
            //          << plainHex
            //          << std::endl;
            assert(plainHex.compare(sm4ECBPlain1) == 0);


            pass();
        }
        catch (std::exception&)
        {
            fail();
        }
    }




public:
    void
    run()
    {
        testSign();
        testSM2EncryptAndDecrypt();
        testSM3();
        testSM4EncryptAndDecrypt();
    }
};

BEAST_DEFINE_TESTSUITE(GmSoftEncrypt, core, ripple);

}  // namespace ripple
