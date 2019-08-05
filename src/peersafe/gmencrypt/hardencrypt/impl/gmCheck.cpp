#include <peersafe/gmencrypt/hardencrypt/gmCheck.h>
#include <peersafe/gmencrypt/randomcheck/randCheck.h>
#include <string.h>
#ifdef _WIN32
#include <Windows.h>
#include <io.h>
#include <direct.h>
int access(const char* filename, int accessmode)
{
	return _access(filename, accessmode);
}
int mkdir(const char* pathname, unsigned int mode)
{
	return _mkdir(pathname);
}
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
void Sleep(int n)
{
	sleep(n/1000);
}
#endif

#ifdef GM_ALG_PROCESS

const int preDataSetCnt = 3;
const int randomTestDataLen = 1000000;// 1310720; //10^6bit ≈ 10*128k bit=1310720 bit
const int randomTestSetCnt = 50;

//加解密3组数据，明文长度 = 0x00000020
const std::string publicED1 = "895320226500D6FAFFA60CD9D59C083FBFF4923C97D71895D293511CF2AB47ED0C42ADB86EEED6F3019C0801CBCA05F7B41F2B1DBDC2A775BD93DF6D7104CF6D";
const std::string privateED1 = "FB250FEC83B9F7AF9BADA30A94C42E40442D23B8425FCC137CD0C1C4B9205354";
const std::string cipherED1 = "4F3467C5065F9713301689AAF4A3202830A5CDE26F69541677E9AD76517DC2C3EABF0BF7A76433845AE7F86CA7F38DB7FF04268D597659B82200C5FB69B60F3580AF4642CC74D54DFB6BEA105C631F8E10DACA1070A1FDC6CDB757BAC99C6143A41B790745F15A3CFD1FF532C23E698431730E97E9831F786E0F920ABCC47863";
const std::string plainED1 = "4E0D2175C6BB7D64B64AA9061242D6A175CAD30AFDE0A4D68C65C741FD3AA0CA";

const std::string publicED2 = "00E98784DF587226B05CF8E568C1515046482386BFDACDE5873240BCCD7AB10F4DB015DA9436511FB6EA0EB4E8A6A924B177C9DEE2B7779D5A395BF27B05FD6D";
const std::string privateED2 = "18D6BFD2067B191FD943AF378284E01C8AD9C0B9278E3A4C5DDFEE04952EF219";
const std::string cipherED2 = "589FECAA081ADB9214DE242049094641E57EAACA3C1EDCA6778EFD3CF691A8522017FC67023F773B14DAAFF4853288EEBE31A2343CFFA0B72648E659ED1404978D3D7B53A1A199FB265D96D48894A5252403CC4DF5DDC87D37133BA136B5707CB9F914118E3636F425292C64CF66A170ACCFA84D58971AA62BC519CE16D727F2";
const std::string plainED2 = "C37365BAF0FA79D70686FBAB9C6335A610C69EAD3160042FC327A6BC03A65790";

const std::string publicED3 = "2DCDEA35404D2500E4CEE1C2E345A3E3E205DF49FB16C57A4802A29EE0340F217958A41A5E8F3D652548CB4506966D5B874A5D46173F733AF05E55CAB7F4A60C";
const std::string privateED3 = "0EB22EC76ABE054CD3980340BAF8D31E0529AE277D6E57608179C38CA6034E0E";
const std::string cipherED3 = "54BF7371B1FC08F6BAAFDF0131B412D02AC0556108B3CEE7367DFA50B15F4121C77951F8A6948A4835E501EC1BD5EECD36BB3A0675DDFE84621A4F5F7972953F941257D260AB0BDD4E21435BBED27E90F0F0A555F76D7321925CDCCA29D07D70FF3F9C5E3A62F5C17A1469BA0F087E82A5195574415C6CE905A933F0B427D725";
const std::string plainED3 = "DA20EB667A395EC5AA6AA1105E30C4CDF2699598A4196DB2DA27AC1A0E1AB714";

//const std::string publicED4 = "D824172E7006A0EE4195A4CCF589D7627DD10A7190D96734D136BBCA874885BF4E0DAA163F2A103640F79CFC738C1E667DAEBDFCF8E3D396DF5CCA25C99E9E13";
//const std::string privateED4 = "9F50260C2478A7C2A751163DA1CB8C117FBEAE9EEE6E481A713A4051054A91C0";
//const std::string cipherED4 = "68F0D35E0A837DE3120558012A30166251F5851986BD24179E3B5B53FD96E24B714A0A7E429B631A8A3E89A8340DD10D71128A863B5DE8B270D720FCB14726CEA954AB99425A1BA7D4E56529DA398EE4C00389E2560DF22922632E063719D2C029179F170AAA2BF000F31402874433F4103E6E87D94656571966BCDD78798E54";
//const std::string plainED4 = "740F300378744B06BC08E1694DE07DE6493E2A2D6BFEF7F508C6B3D6FE77A035";
//
//const std::string publicED5 = "9FF2A206DEE9361E8983CDDFB6D84042A82B546D3DC558C5948D3B5F3FA7689FF28689D48E1F9C516BC2F8613B9C5E3F3C60F9A8B04155FBB2640C9A946F3F02";
//const std::string privateED5 = "8CDC08FF1302C32EDC0DE17A5D50C3A03DBAE7CE20A58D66E37F434B51952B05";
//const std::string cipherED5 = "4213F19AA1D3FFBC70347D9DA8944D4B78EFBEFB702A5ECA12598C2974B64A8A5F46E74CA227E93B9C1CB52E55DA793484D0A2541C3BDDCC630A6DF19002868A5FA7C4BEE2F59E49DC29E92003FC565309253BFE9C825EC9D6E2DE7D5F382EF14F6F7F60384A1A1A96AC49F24544CDE2BC2C93954B1785E0EB23480316A58095";
//const std::string plainED5 = "2B7B0C3073AB09E6C3D9DB58FFFA878093D5377EE9C35C096C958919C0E31802";
const std::string g_PublicED[] = { publicED1 ,publicED2 ,publicED3 };
const std::string g_PrivateED[] = { privateED1 ,privateED2 ,privateED3 };
const std::string g_CipherED[] = { cipherED1 ,cipherED2 ,cipherED3 };
const std::string g_PlainED[] = { plainED1 ,plainED2 ,plainED3 };

//签名验签3组数据，哈希值不带ID
//签名数据长度 = 
const std::string publicSV1 = "1B6954EAE1623ADEED7800D42FD11FE1B64EAF66C4721713DA377AB18B83B5E167B8B00369C5BE2EEF452DE91AD12C5D0C97471B0B4356AA88D2689C9BAB083B";
const std::string privateSV1 = "FA6D7BC51D19E38D9C05D758DBF9D240DD09BA975781F640D03CC60E4FDD2BC8";
const std::string plainSV1 = "27AAD2A5A9852934438C52BE5304B1E31A5C89790BE106B92A340EC49349BEA2";
const std::string signedDataSV1 = "786BEC03592206B3878DFD851AEAF065CA37591DB79D396A5580E76475295CDF2E36CF0A9B2EF3982F4525CC6B6EDF9EE50B6DDFAA46AD0DCC0B301B39AD5B47";

const std::string publicSV2 = "6766232B284CDFD30CA92805A6894679C4251BD24F53E6599A63491792E5F8DBAE93C8965FA38F6356E7B0E92BAA8BA41DD75800DB24FF74D8B69CB51CB5748F";
const std::string privateSV2 = "0927066C51E035640D9F2AFD9CE772ED37021C434D0F3FF3FC21751CB4955B37";
const std::string plainSV2 = "8D80124A4CF8CEEA2F4735F5870DB929EF086F52D288B8612D2320BD96A986DE";
const std::string signedDataSV2 = "44A24904CFE164BBD17F39577D7E4C016F75EDA62942731719B77CEA3193B0AB0E9406AE94A4181DE75AEFC27FB89C646D7686E430BBFCBDB96970A50C6D9B95";

const std::string publicSV3 = "688834A2835193137C978340BDAA4F8DA9E0782FB49135E7A535EEE1BE678D0BEE131C763CB33D1B59AA894A2DCA075EC48F58F9E467EC0F5E2EA626B3C25459";
const std::string privateSV3 = "DF8826D764DA1D9206351B160878C4D4C7D96A880882CD1598C5D4D90C5AB0A0";
const std::string plainSV3 = "0ACED8EA979482360D64BFB819C34E431737F3988DB41C7EF857C315CC579667";
const std::string signedDataSV3 = "F87988AFE53689960A3742F8A5F2F55F8E71C72DCB4877F82A9F3CBDE7D5351EB717E03AC346229BAE098659831E65D9828CEC57B78BC74F15C5847C53624F73";

//const std::string publicSV4 = "CF3FBC1FE946EF6B14E89168B92C4112E49EF4A98A9966EA8AE6AECA11EFD59109D693ED554C478DED80D7A2A183D62015071218C53DF9FBADAA1C6364BA3999";
//const std::string privateSV4 = "8ACCC9B07BCC89F963FECB4CC12D3F652A8AA9B5B14E38EEDFEC4542137CD6CB";
//const std::string plainSV4 = "9F08DAA7956DA578DF0A09914AE79B53885B20D69BCBDB7051B6AFFA8CBC5CEB";
//const std::string signedDataSV4 = "B61BA922B6764AC95C7DCA2A59FEB7C8853871779B160E058ED84067040BFC059E91B37C1E5C9B883A5F2F1FDBF12667FCC8E54E9F537363F065F95B43DAB5C4";
//
//const std::string publicSV5 = "10B6B31000619A63AE1C6EA72483BC5BB54577A4F9A4223772AB93FEBED4E992021094E7BBF2D945A613BE5E259BCBF82966FD971ED1EC29CB1C7DF9BB16E246";
//const std::string privateSV5 = "B873D868CEA70C3418EE3ED89060F6BEF8E2670F4A1B6CEEECF5FB019D155860";
//const std::string plainSV5 = "5CB284BA0288DC40F521A279CAA73052FDB71B9927982B3BA5D279E7E43A7C11";
//const std::string signedDataSV5 = "786D35599514247CCABDAD2E98F0E535090764A19D884E312D4FCB181F1FA8923D17B50E323B556A5CC7F78E73F364505727992EFAE45A79768388F60C299609";
const std::string g_PublicSV[] = { publicSV1 ,publicSV2 ,publicSV3 };
const std::string g_PrivateSV[] = { privateSV1 ,privateSV2 ,privateSV3 };
const std::string g_SignedDataSV[] = { signedDataSV1 ,signedDataSV2 ,signedDataSV3 };
const std::string g_PlainSV[] = { plainSV1 ,plainSV2 ,plainSV3 };

//SM3杂凑3组数据
//消息长度 = 00000010
const std::string sm3Plain1 = "11AD63D24656D0922A7647E25AC7EF5E";
const std::string sm3Hash1 = "9690C000D57A987671B3D5709D3CF26154AC198BA9DB012D457B2BB3E5BD9CB4";
//消息长度 = 00000020
const std::string sm3Plain2 = "CFEF220361F7164489FEBD596B2FE9DB14F0B48E005C1F7EDFF4F94D989103CB";
const std::string sm3Hash2 = "D5ABCDDD0DEEE4BBA22771FD9C98446D60DA480558F0EF11DD05E993A71BB786";
//消息长度 = 00000030
const std::string sm3Plain3 = "8F9E51C72483F3257C64D531830CE433312472440DA9DC5FF09D6ABB0CFC76582CF1D1D83D4A62910E2A10E054FDE3C0";
const std::string sm3Hash3 = "FBACF98ED65781B6D645FA3957E9337A604753BDBFFF28CD959B75234743AFC4";
////消息长度 = 00000040
//const std::string sm3plain4 = "f1b1659b76244f60e6d861d5bdee922f305269e0b733abb0c9b6360fe6eb2da9db00553c42b4bfa6480fa843b3509d752328e384972515a934710f6542bc0b17";
//const std::string sm3hash4  = "92b1c6538111ec4a01c56eb23ae23883a7afe8a841cb1264f9ab28664d888f4c";
////消息长度 = 00000050
//const std::string sm3plain5 = "78770ace7943765c9ea905acf60be0558bda056ee70ae9a5d7c3628b6f5ac5324e7f4bfa24b8baa8710e41b88f05a3f5f2d29846afc970cb3b15f92875aba321e781e26849fad4b247a5241085282e7d";
//const std::string sm3hash5  = "6dd5200346b10fa9fe7ac2c87f8768b028db25520aae7f8111ceeaa6a622ac03";
const std::string g_sm3Plain[] = { sm3Plain1 ,sm3Plain2 ,sm3Plain3 };
const std::string g_sm3Hash[] = { sm3Hash1 ,sm3Hash2 ,sm3Hash3 };

//SM4 CBC 3组数据
//明文长度 = 00000010
const std::string sm4CBCPWD1 = "B9652D9F5B9C6792E0337436CA2F92E7";
const std::string sm4CBCIV1 = "D8E512C0876DFEB1D754FED152917CF5";
const std::string sm4CBCPlain1 = "4BBA07816478F57E53394DEDE524E45B";
const std::string sm4CBCCipher1 = "317818B0CE355D57CCD1C3201DD6D285";
//明文长度 = 00000020
const std::string sm4CBCPWD2 = "4252C5B6743BD65F236EB980A286F1A0";
const std::string sm4CBCIV2 = "3059467577726AF959C2DCC0EC920D88";
const std::string sm4CBCPlain2 = "C3BCC212701BB93046408F206466E118EFA8B2229E720C2A7C36EA55E9605695";
const std::string sm4CBCCipher2 = "5E0F5AC4C6D18A85FD5A7262A524D1C893A2D1027C3CCC9EB168E87295918338";
//明文长度 = 00000030
const std::string sm4CBCPWD3 = "E28BCF7B83F5ED4CCA3048569E1B9584";
const std::string sm4CBCIV3 = "10111231E060253A43FD3F57E37607AB";
const std::string sm4CBCPlain3 = "2827B599B6B1BBDA37A8ABCC5A8C550D1BFB2F494624FB50367FA36CE3BC68F11CF93B1510376B02130F812A9FA169DB";
const std::string sm4CBCCipher3 = "08D8D07CA378E37ADC3D3568B97B352B9582A3A0DB752B5F28E6358BD10A5D726CE628999FB836F8D46599E7E35D2EB9";
////明文长度 = 00000040
//const std::string sm4CBCPWD4 = "99CED213D6A7D695DE24DE3CF01AC090";
//const std::string sm4CBCIV4  = "46672B97996F45EEE8CD921776421815";
//const std::string sm4CBCPlain4  = "12F1C1369ACF0031C255B770127859AF2EDA4CC65A4291C33D4AA63CE557ABE40CCEC55B9A85C04B6B78F8B051BE79775DBB654B5A9696AC9BED8541F7C86A6F";
//const std::string sm4CBCCipher4 = "14CBAAB46A6215A5157EB38D99F9E3F7F4508487D5FB1E724A5E490690AE5869741CC878CBAD1D296F63CDCC3A54BFAEB54C08132E6195B710F6A62CA4B1CA0F";
////明文长度 = 00000050
//const std::string sm4CBCPWD5 = "11CCA52ADDB25D0B5FF767A3B6D46910";
//const std::string sm4CBCIV5  = "5A703FCDA2559DCF8920F6CDB17D9DDF";
//const std::string sm4CBCPlain5  = "A8532B4A6B3C209A2838CDF248A271A1AD63A2F53865EE548F4BC3881D5F8C5B5ACB1D644C0D51204EA5F1451010D852DFFA556B26B0D16435D4B21C42547F0CAD9C441F890B38C457A49D421407E84D";
//const std::string sm4CBCCipher5 = "C1DB58287A9B9BBA5912E6C04AF36A91FCC904DF84FCAD25D1202F3B016B8D980F92381074699A6BE9F977EDD808D3EC395D5480B77C0D347320AB0DB11C5C8EB3EFCD0EB8F44BC50ACBB5B6127B0A38";
const std::string g_sm4CBCPWD[] = { sm4CBCPWD1 ,sm4CBCPWD2 ,sm4CBCPWD3 };
const std::string g_sm4CBCPlain[] = { sm4CBCPlain1 ,sm4CBCPlain2 ,sm4CBCPlain3 };
const std::string g_sm4CBCCipher[] = { sm4CBCCipher1 ,sm4CBCCipher2 ,sm4CBCCipher3 };

//SM4 ECB 3组数据
//明文长度 = 00000010
const std::string sm4ECBPWD1 = "C35242CC90CB75935A536F32149F5C35";
const std::string sm4ECBPlain1 = "ECB57EE6D15BD06632CFE9FD09B822AF";
const std::string sm4ECBCipher1 = "03D8C466A7C245971925E35540E9209F";
//明文长度 = 00000020
const std::string sm4ECBPWD2 = "195DFB107315BB593FFE57BAB9F295F2";
const std::string sm4ECBPlain2 = "7AB7711EF976C0CE516F135E45EADE848171DA25223CA76D79EDB61E0C7E6628";
const std::string sm4ECBCipher2 = "A0C8EC1BA6A6F9E6E7134037170964927BB47695602C9C16126F980FFD6655FE";
//明文长度 = 00000030
const std::string sm4ECBPWD3 = "DE776D7BF1647A190886196FB1C9D6E5";
const std::string sm4ECBPlain3 = "83F6A5B4A52A81F790875406152917FFA15B3AA5C00B466EE07CC1D7583B52FBA854256402C591210A33F818DBDCEF9E";
const std::string sm4ECBCipher3 = "B4FD5B2087E6B3BF9E09334AD19AE3348408760A8A026A9FDA6D3B43E9B3D530A42F0B2DD4FABB1394BC5BA01D01736A";
////明文长度 = 00000040
//const std::string sm4ECBPWD4 = "4ACD9F456E556BF560B9D43F3F2999ED";
//const std::string sm4ECBPlain4  = "77F322DE43F71E11715B6CFF6680372E61346603042933D810A5194E707CF4E4793C64C970A772EF4C667661BEFB38D66FF85586896EE43C78A95AADF21AAB08";
//const std::string sm4ECBCipher4 = "598035AAA657169FED3AF3E3B10875C381D2B2AB9618E3B291C34F95012DD87E1AAFA8FE18AD3AD4B81BF2E4DF8D96DFB68F41AD114E8C010E2A4747C85C2DF4";
////明文长度 = 00000050
//const std::string sm4ECBPWD5 = "3495B2CE8FBDD3E424BCDFE7EC3737BF";
//const std::string sm4ECBPlain5  = "FB803577050EC74B212B5D03CDED06803267D595AA208A1880C56E38F71A7F108D36CD7D80F0242D9295EBF909DC4C75FB1B96C5C8BADFB2E8E8EDFDE78E57F2AD81E74103FC430A534DCC37AFCEC70E";
//const std::string sm4ECBCipher5 = "B8FB9C38EEEE927423398EA4A9BAB7795DD54D5C978278FD5CE055012D517D80434486EFE2A395100F38067FC5CFA7CDEBBC7CDC01A23DDBF8679F84DAA9D97FCF118FED797C42CE303BCC68FF23ADB5";
const std::string g_sm4ECBPWD[] = { sm4ECBPWD1 ,sm4ECBPWD2 ,sm4ECBPWD3 };
const std::string g_sm4ECBPlain[] = { sm4ECBPlain1 ,sm4ECBPlain2 ,sm4ECBPlain3 };
const std::string g_sm4ECBCipher[] = { sm4ECBCipher1 ,sm4ECBCipher2 ,sm4ECBCipher3 };

GMCheck::GMCheck()
{
	hEObj = HardEncryptObj::getInstance();
	isRandomCycleCheckThread = false;
	isRandomGenerateThread = false;
}

//void GMCheck::setLogJournal(beast::Journal* journal)
//{
//	if (journal != nullptr && journal_ == nullptr)
//	{
//		journal_ = journal;
//	}
//}

void GMCheck::cipherConstruct(ripple::Blob &cipher)
{
	const std::string cipherStart = "20000000";
	const int zeroPaddingNum = 104;
	auto cipherStartBlob = ripple::strUnHex(cipherStart).first;
	ripple::Blob cParam(cipher.end()-32, cipher.end());
	cipher.erase(cipher.end() - 32, cipher.end());
	cipher.insert(cipher.end() - 32, cParam.begin(), cParam.end());
	cipher.insert(cipher.begin(), cipherStartBlob.begin(), cipherStartBlob.end());

	for (int i = 0; i < zeroPaddingNum; i++)
	{
		cipher.insert(cipher.end() - 32, 0);
	}
}

void GMCheck::cipher2GMStand(unsigned char* cardCipher, unsigned char* gmStdCipher, unsigned int plainDataLen)
{
	//max length of encryptCard for asymmetric encrypt cipherdata
	for (int i = 4; i<68; i++)
	{
		gmStdCipher[i - 4] = cardCipher[i];
	}
	for (int i = 204; i < 236; i++)
	{
		//begin from 64
		gmStdCipher[i-140] = cardCipher[i];
	}
	for (int i = 68; i < 68 + plainDataLen; i++)
	{
		//begin from 96
		gmStdCipher[i + 28] = cardCipher[i];
	}
}

bool GMCheck::sm2EncryptAndDecryptCheck(unsigned long plainDataLen)
{
	bool result = false;
	int rv = 0;
	printf("********************************\n");
	printf("Beging SM2 Encrypt&Decrypt Check\n");
	/*JLOG(journal_.info()) << "********************************";
	JLOG(journal_.info()) << "Beging SM2 Encrypt&Decrypt Check";*/
	for (int i = 0; i < preDataSetCnt; ++i)
	{
		auto tempPri = ripple::strUnHex(g_PrivateED[i]).first;
		auto tempPub = ripple::strUnHex(g_PublicED[i]).first;
		tempPub.insert(tempPub.begin(), 0x47);
		auto tempCipher = ripple::strUnHex(g_CipherED[i]).first;
		cipherConstruct(tempCipher);
		auto tempPlain = ripple::strUnHex(g_PlainED[i]).first;
		unsigned char deResult[1024];
		unsigned long deResultLen = 1024;
		memset(deResult, 0, strlen((char*)deResult));
		std::pair<int, int> pri4DecryptInfo = std::make_pair(hEObj->gmOutCard, 0);
		std::pair<unsigned char*, int> pri4Decrypt = std::make_pair(tempPri.data(), tempPri.size());
		rv = hEObj->SM2ECCDecrypt(pri4DecryptInfo, pri4Decrypt, tempCipher.data(), tempCipher.size(), deResult, &deResultLen);
		if (rv)
		{
			result = false;
			return result;
		}
		if (memcmp(deResult, tempPlain.data(), tempPlain.size()))
		{
			printf("SM2 decrypt result comparision failed\n");
			//JLOG(journal_.error()) << "SM2 decrypt result comparision failed" ;
			result = false;
			return result;
		}
		else
		{
			printf("SM2 decrypt result comparision successful\n");
			//JLOG(journal_.error()) << "SM2 decrypt result comparision successful";
			result = true;
		}

		unsigned char pCipherBuf[1024] = {0};
		unsigned long cipherLen = 1024;
		memset(deResult, 0, strlen((char*)deResult));
		std::pair<unsigned char*, int> pub4Encrypt = std::make_pair((unsigned char*)&tempPub[0], tempPub.size());
		rv = hEObj->SM2ECCEncrypt(pub4Encrypt, (unsigned char*)&tempPlain[0], tempPlain.size(), pCipherBuf, &cipherLen);
		if (rv)
		{
			result = false;
			return result;
		}
		memset(deResult, 0, strlen((char*)deResult));
		rv = hEObj->SM2ECCDecrypt(pri4DecryptInfo, pri4Decrypt, pCipherBuf, strlen((char*)pCipherBuf), deResult, &deResultLen);
		if (rv)
		{
			result = false;
			return result;
		}
		if (memcmp(deResult, tempPlain.data(), tempPlain.size()))
		{
			printf("SM2 encrypt&decrypt check failed in %d times\n",i+1);
			result = false;
			return result;
		}
		else
		{
			printf("SM2 encrypt&decrypt check successful in %d times\n",i+1);
			result = true;
		}
	}
	return result;
}

bool GMCheck::sm2SignedAndVerifyCheck()
{
	bool result = false;
	int rv = 0;
	printf("********************************\n");
	printf("Beging SM2 Signed&Verify Check\n");
	for (int i = 0; i < preDataSetCnt; ++i)
	{
		auto tempPri = ripple::strUnHex(g_PrivateSV[i]).first;
		auto tempPub = ripple::strUnHex(g_PublicSV[i]).first;
		tempPub.insert(tempPub.begin(), 0x47);
		auto tempSigned = ripple::strUnHex(g_SignedDataSV[i]).first;
		auto tempPlain = ripple::strUnHex(g_PlainSV[i]).first;
		std::pair<unsigned char*, int> pub4Verify = std::make_pair((unsigned char*)&tempPub[0], tempPub.size());
		rv = hEObj->SM2ECCVerify(pub4Verify, tempPlain.data(), tempPlain.size(),tempSigned.data(),tempSigned.size());
		if (rv)
		{
			printf("SM2 verify check failed in %d times!\n", i+1);
			result = false;
			return result;
		}
		printf("SM2 verify successful in %d times\n", i+1);

		unsigned char pSignedBuf[256] = { 0 };
		unsigned long signedLen = 256;
		std::pair<int, int> pri4SignInfo = std::make_pair(hEObj->gmOutCard, 0);
		std::pair<unsigned char*, int> pri4Sign = std::make_pair((unsigned char*)&tempPri[0], tempPri.size());
		rv = hEObj->SM2ECCSign(pri4SignInfo, pri4Sign, tempPlain.data(), tempPlain.size(), pSignedBuf, &signedLen);
		if (rv)
		{
			printf("SM2 sign&verify check failed in %d times!\n", i+1);
			result = false;
			return result;
		}
		rv = hEObj->SM2ECCVerify(pub4Verify, tempPlain.data(), tempPlain.size(), pSignedBuf, signedLen);
		if (rv)
		{
			printf("SM2 sign&verify check failed in %d times!\n", i+1);
			result = false;
			return result;
		}
		else
		{
			printf("SM2 sign&verify check successful in %d times!\n", i + 1);
			result = true;
		}
	}
	return result;
}

bool GMCheck::sm3HashCheck()
{
	bool result = false;
	printf("********************************\n");
	printf("Beging SM3hash Check\n");
	for (int i = 0; i < preDataSetCnt; ++i)
	{
		auto tempPlain = ripple::strUnHex(g_sm3Plain[i]).first;
		auto tempHash = ripple::strUnHex(g_sm3Hash[i]).first;
		HardEncrypt::SM3Hash objSM3(hEObj);
		unsigned char hashData[32] = { 0 };
		int HashDataLen = 0;
		objSM3.SM3HashInitFun();
		objSM3(tempPlain.data(), tempPlain.size());
		objSM3.SM3HashFinalFun(hashData, (unsigned long*)&HashDataLen);
		if (memcmp(hashData, tempHash.data(), tempHash.size()))
		{
			printf("SM3 hash result comparision failed in %d times.\n",i+1);
			result = false;
			return result;
		}
		else
		{
			printf("SM3 hash result comparision successful in %d times.\n",i+1);
			result = true;
		}
	}
	printf("Finish SM3hash Check\n");
	return result;
}

bool GMCheck::sm4EncryptAndDecryptCheck(unsigned long plainDataLen)
{
	bool result = false;
	printf("********************************\n");
	printf("Beging SM4 Encrypt&Decrypt Check\n");
	unsigned char pKey[16] = { 0 };
	unsigned long keyLen = 16;
	for (int i = 0; i < preDataSetCnt; ++i)
	{
		auto tempPwd = ripple::strUnHex(g_sm4ECBPWD[i]).first;
		auto tempPlain = ripple::strUnHex(g_sm4ECBPlain[i]).first;
		auto tempCipher = ripple::strUnHex(g_sm4ECBCipher[i]).first;
		unsigned long cipherLen = tempCipher.size();
		unsigned long resultLen = tempPlain.size();
		unsigned char* pCipherBuf = new unsigned char[2*cipherLen];
		unsigned char* pResultPlain = new unsigned char[resultLen];
		printf("Begin to encrypt data with SM4 for %d times\n",i+1);
		hEObj->SM4SymEncrypt(hEObj->ECB, tempPwd.data(), tempPwd.size(), tempPlain.data(), tempPlain.size(), pCipherBuf, &cipherLen);
		if (memcmp(pCipherBuf, tempCipher.data(), tempCipher.size()))
		{
			printf("SM4 encrypt result comparision failed in %d times\n",i+1);
			result = false;
			delete[] pCipherBuf;
			delete[] pResultPlain;
			return result;
		}
		else
		{
			printf("SM4 encrypt result comparision successful in %d times\n", i+1);
			result = true;
		}
		printf("Begin to decrypt data with SM4 in %d times\n",i+1);
		hEObj->SM4SymDecrypt(hEObj->ECB, tempPwd.data(), tempPwd.size(), pCipherBuf, cipherLen, pResultPlain, &resultLen);
		if (memcmp(pResultPlain, tempPlain.data(), tempPlain.size()))
		{
			printf("SM4 decrypt result comparision failed in %d times\n", i+1);
			result = false;
			delete[] pCipherBuf;
			delete[] pResultPlain;
			return result;
		}
		else
		{
			printf("SM4 encrypt&decrypt result comparision successful in %d times\n", i+1);
			result = true;
		}
		delete[] pCipherBuf;
		delete[] pResultPlain;
	}
	printf("SM4 encrypt&decrypt check successful\n");
	return result;
}

bool GMCheck::randomCheck(unsigned long dataLen,unsigned long cycleTimes)
{
	bool randomCheckRet = false;
	RandCheck* rcInstance = RandCheck::getInstance();
	//rcInstance->setLogJournal(journal_);
	randomCheckRet = rcInstance->RandTest(hEObj, randomTestSetCnt, randomTestDataLen);
	if (!randomCheckRet)
	{
		printf("first check failed, check again\n");
		randomCheckRet = rcInstance->RandTest(hEObj, randomTestSetCnt, randomTestDataLen);
	}
	if (randomCheckRet)
	{
		printf("Random check successfully!\n");
	}
	else printf("Random check failed!\n");
	
	return randomCheckRet;
}

void GMCheck::tryRandomCycleCheck(unsigned long ledgerSeq)
{
//#ifdef _WIN32
#ifndef _WIN32
	if (0 == ledgerSeq % randomCycheckLedgerNum)
	//if (0 == ledgerSeq % 20)
	{
		printf("current ledgerSeq : %d, begin to do randomCycleCheck.\n", ledgerSeq);
		parentTid = pthread_self();
		printf("tryRandomCycleCheck thread id:%u\n", parentTid);
		unsigned int pid = getpid();
		printf("tryRandomCycleCheck proc id:%u\n", pid);
		if (!isRandomCycleCheckThread)
		{
			pthread_t id;
			int ret = pthread_create(&id, NULL, randomCycheckThreadFun, (void*)this);
			if (ret) {
				printf("tryRandomCycleCheck create pthread error!");
			}
		}
	}
#endif
}
void* GMCheck::randomCycheckThreadFun(void *arg) {
//#ifdef _WIN32
#ifndef _WIN32
	GMCheck *ptrGMC = (GMCheck *)arg;
	ptrGMC->isRandomCycleCheckThread = true;
	bool randomCycleCheckRes = ptrGMC->randomCycleCheck();  //call randomCycleCheck func
	//randomCycleCheckRes = false;
	if (!randomCycleCheckRes)
	{
		printf("randomCycleCheck prepare to close chainsql!");
		//pthread_kill(ptrGMC->parentTid, SIGTERM);
		pthread_kill(ptrGMC->parentTid, SIGKILL);
	}
	ptrGMC->isRandomCycleCheckThread = false;
#endif
}

bool GMCheck::randomCycleCheck(unsigned long dataLen, unsigned long cycleTimes)
{
//#ifdef _WIN32
#ifndef _WIN32
	pthread_t tid;
	tid = pthread_self();
	printf("randomCycleCheck thread id:%u\n", tid);
	unsigned int pid = getpid();
	printf("randomCycleCheck proc id:%u\n", pid);
	bool randomCycleCheckRet = false;
	RandCheck* rcInstance = RandCheck::getInstance();
	//rcInstance->setLogJournal(journal_);
	const int randomCycleCheckLen = 20480; //2*10^4bit ≈ 20*1024 bit
	const int randomCycleCheckSetCnt = 20;
	randomCycleCheckRet = rcInstance->RandTest(hEObj, randomCycleCheckSetCnt, randomCycleCheckLen, true);
	if (!randomCycleCheckRet)
	{
		printf("first cycle check failed, check again\n");
		randomCycleCheckRet = rcInstance->RandTest(hEObj, randomCycleCheckSetCnt, randomCycleCheckLen, true);
	}
	if (randomCycleCheckRet)
	{
		printf("Random cycle check successfully!\n");
	}
	else printf("Random cycle check failed!\n");

	return randomCycleCheckRet;
#else
	return true;
#endif
}

bool GMCheck::randomSingleCheck(unsigned long dataLen)
{
	bool randomCheckRet = false;
	RandCheck* rcInstance = RandCheck::getInstance();
	//rcInstance->setLogJournal(journal_);
	unsigned char* pRandomBuf = new unsigned char[dataLen];
	unsigned long randomLen = dataLen;
	if (hEObj->GenerateRandom(randomLen, pRandomBuf))
	{
		return false;
	}
	randomCheckRet = !rcInstance->RandomnessSingleCheck(pRandomBuf, randomLen);
	if (!randomCheckRet)
	{
		printf("RandomnessSingleCheck first time failed, check again!\n");
		randomCheckRet = !rcInstance->RandomnessSingleCheck(pRandomBuf, randomLen);
	}
	if (randomCheckRet)
	{
		printf("RandomnessSingleCheck successful\n");
	}
	else printf("RandomnessSingleCheck failed\n");

	return randomCheckRet;
}

bool GMCheck::startAlgRanCheck(int checkType)
{
	switch(checkType)
	{
	case GMCheck::SM2ED_CK:
		return sm2EncryptAndDecryptCheck();
	case GMCheck::SM2SV_CK:
		return sm2SignedAndVerifyCheck();
	case GMCheck::SM3_CK:
		return sm3HashCheck();
	case GMCheck::SM4_CK:
		return sm4EncryptAndDecryptCheck();
	case GMCheck::RAN_CK:
		return randomCheck();
	case GMCheck::SM_ALL_CK:
		if (sm2EncryptAndDecryptCheck()) {
			if (sm2SignedAndVerifyCheck()) {
				if (sm3HashCheck()) {
					if (sm4EncryptAndDecryptCheck()) {
						return randomCheck();
					}
					return false;
				}
				else return false;
			}
			else return false;
		}
		else return false;
	default:
		printf("The check type is wrong,please check!");
		//JLOG(journal_.error()) << "The check type is wrong,please check!";
		return false;
	}
}

bool GMCheck::generateRandom2File()
{
//#ifdef _WIN32
#ifndef _WIN32
	if (!isRandomGenerateThread)
	{
		pthread_t id;
		int ret = pthread_create(&id, NULL, randomGenerateThreadFun, (void*)this);
		if (ret) {
			printf("generateRandom2File create pthread error!");
			return false;
		}
		return true;
	}
	else
	{
		printf("Last random generate thread is not finished!");
		return false;
	}
#else
	return true;
#endif
}

void* GMCheck::randomGenerateThreadFun(void *arg)
{
	GMCheck *ptrGMC = (GMCheck *)arg;
	ptrGMC->isRandomGenerateThread = true;

	//call handleGenerateRandom2File func
	bool randomGenerateRes = ptrGMC->handleGenerateRandom2File();  
	if (!randomGenerateRes)
	{
		printf("random generate failed!");
	}

	ptrGMC->isRandomGenerateThread = false;
}

bool GMCheck::handleGenerateRandom2File()
{
	int rv = 0;
	unsigned char pbOutBuffer[131072]; //10^6bit ≈ 128k bit = 131072 bit = 128*1024 bit
	char pFileName[32] = { 0x00 };
	//char pPathName[16] = { 0x00 };
	//sprintf(pPathName, "./data/random");
	std::string pathName("./data/random/");
	if (dataFolderCheck(pathName))
	{
		printf("生成data/random路径失败！\n");
		return false;
	}
	for (int i = 0; i<1000; i++)
	{
		memset(pbOutBuffer, 0x00, 131072);
		for (int j = 0; j < 128; j++)
		{
			rv = hEObj->GenerateRandom2File(1024, pbOutBuffer, j);
			if (rv)
			{
				return false;
			}
		}
		printf("\n++++++++++产生第%d个随机数文件成功++++++++++\n", i + 1);
		sprintf(pFileName, "./data/random/random%d", i);
		if (!FileWrite(pFileName, "wb+", pbOutBuffer, 128 * 1024))
		{
			printf("random%d.dat文件生成失败\n", i);
			return false;
		};
	}
	printf("采集随机数数据完成。\n");
	return true;
}

//写文件 ：SM4验证数据-加密-X-CBC.txt
int GMCheck::getDataSM4EncDec_CBC_Enc(int dataSetCnt, unsigned int plainLen)
{
	int rv;
	int i= 0;
	unsigned char pKey[16], pIV[16], pInData[10240], pOutData[10240], pEncData[10240], pTmpData[10240];
	unsigned int nTmpDataLen, nOutDataLen, nEncDataLen;
	char pFileName[128] = { 0x00 };
	unsigned char newline[] = { 0x0D,0x0A }, blankSpace[] = { 0x20 }, equal[] = { 0x3D,0x20 };
	
	//printf("\n请输入要采集数据的组数：");
	sprintf(pFileName, "data/SM4��֤����-����-%d-CBC.txt", dataSetCnt);
	FileWrite(pFileName, "wb", NULL, 0);
	for (i = 0; i<dataSetCnt; i++)
	{
		printf("SM4对称加密解密数据采集。\n");
		//printf("\n输入数据长度，必须为分组长度的整数倍(16~10K):");
		rv = hEObj->GenerateRandom(16, pKey);
		if (rv)
		{
			printf("生成随机密钥错误，错误码[0x%08x]\n", rv);
			return rv;
		}
		FileWrite(pFileName, "ab", (unsigned char *)"密钥", 4);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(pKey, 16, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);

		rv = hEObj->GenerateRandom(16, pIV);
		if (rv)
		{
			printf("生成随机PIV错误，错误码[0x%08x]\n", rv);
			return rv;
		}
		FileWrite(pFileName, "ab", (unsigned char *)"IV", 2);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(pIV, 16, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);
		//明文
		FileWrite(pFileName, "ab", (unsigned char *)"明文长度", 8);
		FileWrite(pFileName, "ab", equal, 2);
		sprintf((char*)pTmpData, "%08x", plainLen);
		FileWrite(pFileName, "ab", pTmpData, 8);
		FileWrite(pFileName, "ab", newline, 2);
		rv = hEObj->GenerateRandom(plainLen, pInData);
		if (rv)
		{
			printf("生成随机明文错误，错误码[0x%08x]\n", rv);
			return rv;
		}
		FileWrite(pFileName, "ab", (unsigned char *)"明文", 4);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(pInData, plainLen, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);

		rv = hEObj->SM4SymEncrypt(hEObj->CBC, pKey, 16, pInData, plainLen, pEncData, (unsigned long*)&nEncDataLen);
		FileWrite(pFileName, "ab", (unsigned char *)"密文", 4);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(pEncData, nEncDataLen, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);
		FileWrite(pFileName, "ab", newline, 2);
		PrintData("SGD_SMS4_CBC->密文", pEncData, nEncDataLen, 16);
		rv = hEObj->SM4SymDecrypt(hEObj->CBC, pKey, 16, pEncData, nEncDataLen, pOutData, (unsigned long*)&nOutDataLen);
		if (rv)
		{
			printf("SGD_SMS4_CBC模式解密错误，错误码[0x%08x]\n", rv);
			return rv;
		}
		if ((nOutDataLen == plainLen) && (memcmp(pOutData, pInData, plainLen) == 0))
		{
			PrintData("SGD_SMS4_CBC->解密结果", pOutData, nOutDataLen, 16);
			printf("SGD_SMS4_CBC运算结果：加密、解密及结果比较均正确。\n");
			printf("\n");
			printf("\n");
		}
		else
		{
			printf("SGD_SMS4_CBC运算结果：解密结果错误。\n");
		}

	}
	printf("采集SMS4对称加解密数据完成。\n");
	return 0;
}
//写文件 ：SM4验证数据-解密-X-CBC.txt
int GMCheck::getDataSM4EncDec_CBC_Dec(int dataSetCnt, unsigned int plainLen)
{
	int rv;
	int i, x = 0;
	unsigned char pKey[16], pIV[16], pInData[10240], pOutData[10240], pEncData[10240], pTmpData[10240];
	unsigned int nTmpDataLen, nOutDataLen, nEncDataLen;
	char pFileName[128] = { 0x00 };
	unsigned char newline[] = { 0x0D,0x0A }, blankSpace[] = { 0x20 }, equal[] = { 0x3D,0x20 };

	sprintf(pFileName, "data/SM4验证数据-解密-%d-CBC.txt", dataSetCnt);
	FileWrite(pFileName, "wb", NULL, 0);
	for (i = 0; i<dataSetCnt; i++)
	{
		printf("\n");
		printf("\n");
		printf("SM4对称加密解密数据采集。\n");
		rv = hEObj->GenerateRandom(16, pKey);
		if (rv)
		{
			printf("生成随机密钥错误，错误码[0x%08x]\n", rv);
			return rv;
		}
		FileWrite(pFileName, "ab", (unsigned char *)"密钥", 4);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(pKey, 16, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);

		rv = hEObj->GenerateRandom(16, pIV);
		if (rv)
		{
			printf("生成随机PIV错误，错误码[0x%08x]\n", rv);
			return rv;
		}
		FileWrite(pFileName, "ab", (unsigned char *)"IV", 2);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(pIV, 16, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);
		//明文
		FileWrite(pFileName, "ab", (unsigned char *)"明文长度", 8);
		FileWrite(pFileName, "ab", equal, 2);
		sprintf((char*)pTmpData, "%08x", plainLen);
		FileWrite(pFileName, "ab", pTmpData, 8);
		FileWrite(pFileName, "ab", newline, 2);
		rv = hEObj->GenerateRandom(plainLen, pInData);
		if (rv)
		{
			printf("生成随机明文错误，错误码[0x%08x]\n", rv);
			return rv;
		}
		rv = hEObj->SM4SymEncrypt(hEObj->CBC,pKey, 16, pInData, plainLen, pEncData, (unsigned long*)&nEncDataLen);
		if (rv)
		{
			printf("SGD_SMS4_CBC模式加密错误，错误码[0x%08x]\n", rv);
			return rv;
		}
		FileWrite(pFileName, "ab", (unsigned char *)"密文", 4);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(pEncData, nEncDataLen, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);
		PrintData("SGD_SMS4_CBC->密文", pEncData, nEncDataLen, 16);
		rv = hEObj->SM4SymDecrypt(hEObj->CBC, pKey, 16, pEncData, nEncDataLen, pOutData, (unsigned long*)&nOutDataLen);
		if (rv)
		{
			printf("SGD_SMS4_CBC模式解密错误，错误码[0x%08x]\n", rv);
			return rv;
		}
		if ((nOutDataLen == plainLen) && (memcmp(pOutData, pInData, plainLen) == 0))
		{
			PrintData("SGD_SMS4_CBC->解密结果", pOutData, nOutDataLen, 16);
			printf("SGD_SMS4_CBC运算结果：加密、解密及结果比较均正确。\n");
			printf("\n");
			printf("\n");
		}
		else
		{
			printf("SGD_SMS4_CBC运算结果：解密结果错误。\n");
		}
		FileWrite(pFileName, "ab", (unsigned char *)"明文", 4);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(pInData, plainLen, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);
		FileWrite(pFileName, "ab", newline, 2);
	}
	printf("采集SMS4对称加解密数据完成。\n");
	return 0;
}
//写文件 ：SM4验证数据-加密-X-ECB.txt
int GMCheck::getDataSM4EncDec_ECB_Enc(int dataSetCnt, unsigned int plainLen)
{
	int rv;
	int i, x = 0;
	unsigned char pKey[16], pInData[10240], pOutData[10240], pEncData[10240], pTmpData[10240];
	unsigned int nTmpDataLen, nOutDataLen, nEncDataLen;
	char pFileName[128] = { 0x00 };
	unsigned char newline[] = { 0x0D,0x0A }, equal[] = { 0x3D,0x20 };

	sprintf(pFileName, "data/SM4验证数据-加密-%d-ECB.txt", dataSetCnt);
	FileWrite(pFileName, "wb", NULL, 0);
	for (i = 0; i<dataSetCnt; i++)
	{
		unsigned plainLenTemp = plainLen * ((i % 5) + 1);
		printf("\n");
		printf("\n");
		printf("SM4对称加密解密数据采集。\n");
		rv = hEObj->GenerateRandom(16, pKey);
		if (rv)
		{
			printf("生成随机密钥错误，错误码[0x%08x]\n", rv);
			return rv;
		}
		FileWrite(pFileName, "ab", (unsigned char *)"密钥", 4);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(pKey, 16, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);
		//明文
		FileWrite(pFileName, "ab", (unsigned char *)"明文长度", 8);
		FileWrite(pFileName, "ab", equal, 2);
		sprintf((char*)pTmpData, "%08x", plainLenTemp);
		FileWrite(pFileName, "ab", pTmpData, 8);
		FileWrite(pFileName, "ab", newline, 2);
		rv = hEObj->GenerateRandom(plainLenTemp, pInData);
		if (rv)
		{
			printf("生成随机明文错误，错误码[0x%08x]\n", rv);
			return rv;
		}
		FileWrite(pFileName, "ab", (unsigned char *)"明文", 4);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(pInData, plainLenTemp, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);

		rv = hEObj->SM4SymEncrypt(hEObj->ECB, pKey, 16, pInData, plainLenTemp, pEncData, (unsigned long*)&nEncDataLen);
		if (rv)
		{
			printf("SGD_SMS4_ECB模式加密错误，错误码[0x%08x]\n", rv);
			return rv;
		}
		FileWrite(pFileName, "ab", (unsigned char *)"密文", 4);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(pEncData, nEncDataLen, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);
		FileWrite(pFileName, "ab", newline, 2);

		PrintData("SGD_SMS4_ECB->密文", pEncData, nEncDataLen, 16);
		rv = hEObj->SM4SymDecrypt(hEObj->ECB, pKey,16, pEncData, nEncDataLen, pOutData, (unsigned long*)&nOutDataLen);
		if (rv)
		{
			printf("SGD_SMS4_ECB模式解密错误，错误码[0x%08x]\n", rv);
			return rv;
		}
		if ((nOutDataLen == plainLenTemp) && (memcmp(pOutData, pInData, plainLenTemp) == 0))
		{
			PrintData("SGD_SMS4_ECB->解密结果", pOutData, nOutDataLen, 16);
			printf("SGD_SMS4_ECB运算结果：加密、解密及结果比较均正确。\n");
			printf("\n");
			printf("\n");
		}
		else
		{
			printf("SGD_SMS4_ECB运算结果：解密结果错误。\n");
		}
	}
	printf("采集SMS4对称加解密数据完成。\n");
	return 0;
}
//写文件 ：SM4验证数据-解密-X-ECB.txt
int GMCheck::getDataSM4EncDec_ECB_Dec(int dataSetCnt, unsigned int plainLen)
{
	int rv;
	int i, x = 0;
	unsigned char pKey[16], pInData[10240], pOutData[10240], pEncData[10240], pTmpData[10240];
	unsigned int nTmpDataLen, nOutDataLen, nEncDataLen;
	char pFileName[128] = { 0x00 };
	unsigned char newline[] = { 0x0D,0x0A }, equal[] = { 0x3D,0x20 };
	
	sprintf(pFileName, "data/SM4验证数据-解密-%d-ECB.txt", dataSetCnt);
	FileWrite(pFileName, "wb", NULL, 0);
	for (i = 0; i<dataSetCnt; i++)
	{
		unsigned plainLenTemp = plainLen * ((i % 5) + 1);
		printf("\n");
		printf("\n");
		printf("SM4对称加密解密数据采集。\n");
		rv = hEObj->GenerateRandom(16, pKey);
		if (rv)
		{
			printf("生成随机密钥错误，错误码[0x%08x]\n", rv);
			return rv;
		}
		FileWrite(pFileName, "ab", (unsigned char *)"密钥", 4);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(pKey, 16, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);
		//明文
		FileWrite(pFileName, "ab", (unsigned char *)"明文长度", 8);
		FileWrite(pFileName, "ab", equal, 2);
		sprintf((char*)pTmpData, "%08x", plainLenTemp);
		FileWrite(pFileName, "ab", pTmpData, 8);
		FileWrite(pFileName, "ab", newline, 2);
		rv = hEObj->GenerateRandom(plainLenTemp, pInData);
		if (rv)
		{
			printf("生成随机明文错误，错误码[0x%08x]\n", rv);
			return rv;
		}

		rv = hEObj->SM4SymEncrypt(hEObj->ECB, pKey,16, pInData, plainLenTemp, pEncData, (unsigned long*)&nEncDataLen);
		if (rv)
		{
			printf("SGD_SMS4_ECB模式加密错误，错误码[0x%08x]\n", rv);
			return rv;
		}
		FileWrite(pFileName, "ab", (unsigned char *)"密文", 4);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(pEncData, nEncDataLen, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);
		PrintData("SGD_SMS4_ECB->密文", pEncData, nEncDataLen, 16);
		rv = hEObj->SM4SymDecrypt(hEObj->ECB, pKey, 16, pEncData, nEncDataLen, pOutData, (unsigned long*)&nOutDataLen);
		if (rv)
		{
			printf("SGD_SMS4_ECB模式解密错误，错误码[0x%08x]\n", rv);
			return rv;
		}
		if ((nOutDataLen == plainLenTemp) && (memcmp(pOutData, pInData, plainLenTemp) == 0))
		{
			PrintData("SGD_SMS4_ECB->解密结果", pOutData, nOutDataLen, 16);
			printf("SGD_SMS4_ECB运算结果：加密、解密及结果比较均正确。\n");
			printf("\n");
			printf("\n");
		}
		else
		{
			printf("SGD_SMS4_ECB运算结果：解密结果错误。\n");
		}
		FileWrite(pFileName, "ab", (unsigned char *)"明文", 4);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(pInData, plainLenTemp, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);
		FileWrite(pFileName, "ab", newline, 2);
	}
	printf("采集SMS4对称加解密数据完成。\n");
	return 0;
}
//SM2 加密
int GMCheck::getDataSM2EncDec_Enc(int dataSetCnt, unsigned int plainLen)
{
	int rv = 0;
	int i = 0;
	unsigned char pInData[10240], pTmpData[10240], pCipherData[236], pOutData[10240];
	//unsigned char pGMStdCipher[MAX_LEN_4_GMSTD];
	unsigned int cipherLen = 236, nTmpDataLen, nOutDataLen;
	// unsigned int gmStdCipherLen = 96 + plainLen;
	// unsigned char* pGMStdCipher = new unsigned char[gmStdCipherLen];
	char pFileName[128] = { 0x00 };
	unsigned char newline[] = { 0x0D,0x0A }, equal[] = { 0x3D,0x20 };

	sprintf(pFileName, "data/SM2-加密-%d.txt", dataSetCnt);
	FileWrite(pFileName, "wb", NULL, 0);

	for (i = 0; i<dataSetCnt; i++)
	{
		unsigned plainLenTemp = plainLen * ((i % 5) + 1);
		unsigned int gmStdCipherLen = 96 + plainLenTemp;
		unsigned char* pGMStdCipher = new unsigned char[gmStdCipherLen];
		printf("\n");
		printf("\n");
		printf("SM2加密解密数据采集。\n");
		memset(pGMStdCipher, 0, gmStdCipherLen);

		rv = hEObj->GenerateRandom(plainLenTemp, pInData);
		if (rv)
		{
			printf("生成随机明文错误，错误码[0x%08x]\n", rv);
			break;
			//return rv;
		}
		rv = hEObj->SM2GenECCKeyPair();
		if (rv)
		{
			printf("生成随机SM2密钥对错误，错误码[0x%08x]\n", rv);
			break;
			//return rv;
		}

		std::pair<unsigned char*, int> tempPublickey = hEObj->getPublicKey();
		std::pair<unsigned char*, int> tempPrivatekey = hEObj->getPrivateKey();
		//公钥
		FileWrite(pFileName, "ab", (unsigned char *)"公钥", 4);
		FileWrite(pFileName, "ab", equal, 2);
		//将公钥头部的类型47去掉然后输出
		Data_Bin2Txt(tempPublickey.first + 1, tempPublickey.second - 1, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);
		//私钥
		FileWrite(pFileName, "ab", (unsigned char *)"私钥", 4);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(tempPrivatekey.first, tempPrivatekey.second, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);

		printf("\n");
		printf("\n");
		PrintData("SM2加密解密公钥", tempPublickey.first + 1, tempPublickey.second - 1, 16);
		PrintData("SM2加密解密私钥", tempPrivatekey.first, tempPrivatekey.second, 16);
		PrintData("SM2加密解密明文", pInData, plainLenTemp, 16);
		//明文长度
		FileWrite(pFileName, "ab", (unsigned char *)"明文长度", 8);
		FileWrite(pFileName, "ab", equal, 2);
		sprintf((char*)pTmpData, "%08x", plainLenTemp);
		FileWrite(pFileName, "ab", pTmpData, 8);
		FileWrite(pFileName, "ab", newline, 2);
		rv = hEObj->SM2ECCEncrypt(tempPublickey, pInData, plainLenTemp, pCipherData,(unsigned long*)&cipherLen);
		if (rv)
		{
			printf("SM2加密错误，错误码[0x%08x]\n", rv);
			break;
			//return rv;
		}
		//cipherLen = strlen((char*)pCipherData);
		cipher2GMStand(pCipherData, pGMStdCipher, plainLenTemp);
		//gmStdCipherLen = strlen((char*)pGMStdCipher);
		PrintData("SM2->加密结果", pGMStdCipher, gmStdCipherLen, 16);
		//密文
		FileWrite(pFileName, "ab", (unsigned char *)"密文", 4);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(pGMStdCipher, gmStdCipherLen, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);

		std::pair<int, int> pri4DecryptInfo = std::make_pair(hEObj->gmOutCard, 0);
		rv = hEObj->SM2ECCDecrypt(pri4DecryptInfo, tempPrivatekey, pCipherData, cipherLen, pOutData, (unsigned long*)&nOutDataLen);
		if (rv)
		{
			printf("SM2解密错误，错误码[0x%08x]\n", rv);
			break;
			//return rv;
		}
		PrintData("SM2->解密结果", pOutData, nOutDataLen, 16);
		if (memcmp(pOutData, pInData, nOutDataLen))
		{
			printf("SM2加密解密结果比较失败。\n");
		}
		else
		{
			printf("SM2加密解密结果比较成功。\n");
		}
		//明文
		FileWrite(pFileName, "ab", (unsigned char *)"明文", 4);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(pInData, plainLenTemp, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);
		FileWrite(pFileName, "ab", newline, 2);
		delete[] pGMStdCipher;
	}
	printf("采集SM2加密解密数据完成。\n");
	return rv;
}
//SM2 解密
int GMCheck::getDataSM2EncDec_Dec(int dataSetCnt, unsigned int plainLen)
{
	int rv = 0;
	int i = 0;
	unsigned char pInData[10240], pTmpData[10240], pCipherData[236], pOutData[10240];
	//unsigned char pGMStdCipher[MAX_LEN_4_GMSTD];
	unsigned int cipherLen = 236, nTmpDataLen, nOutDataLen;
	// unsigned int gmStdCipherLen = 96 + plainLen;
	// unsigned char* pGMStdCipher = new unsigned char[gmStdCipherLen];
	char pFileName[128] = { 0x00 };
	unsigned char newline[] = { 0x0D,0x0A }, equal[] = { 0x3D,0x20 };

	sprintf(pFileName, "data/SM2-解密-%d.txt", dataSetCnt);
	FileWrite(pFileName, "wb", NULL, 0);

	for (i = 0; i<dataSetCnt; i++)
	{
		unsigned plainLenTemp = plainLen * ((i % 5) + 1);
		unsigned int gmStdCipherLen = 96 + plainLenTemp;
		unsigned char* pGMStdCipher = new unsigned char[gmStdCipherLen];
		printf("\n");
		printf("\n");
		printf("SM2加密解密数据采集。\n");
		memset(pGMStdCipher, 0, gmStdCipherLen);

		rv = hEObj->GenerateRandom(plainLenTemp, pInData);
		if (rv)
		{
			printf("生成随机明文错误，错误码[0x%08x]\n", rv);
			break;
			//return rv;
		}
		rv = hEObj->SM2GenECCKeyPair();
		if (rv)
		{
			printf("生成随机SM2密钥对错误，错误码[0x%08x]\n", rv);
			break;
			//return rv;
		}
		std::pair<unsigned char*, int> tempPublickey = hEObj->getPublicKey();
		std::pair<unsigned char*, int> tempPrivatekey = hEObj->getPrivateKey();
		//公钥
		FileWrite(pFileName, "ab", (unsigned char *)"公钥=", 4);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(tempPublickey.first + 1, tempPublickey.second - 1, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);

		//私钥
		FileWrite(pFileName, "ab", (unsigned char *)"私钥", 4);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(tempPrivatekey.first, tempPrivatekey.second, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);

		printf("\n");
		printf("\n");
		PrintData("SM2加密解密公钥", tempPublickey.first + 1, tempPublickey.second - 1, 16);
		PrintData("SM2加密解密私钥", tempPrivatekey.first, tempPrivatekey.second, 16);
		PrintData("SM2加密解密明文", pInData, plainLenTemp, 16);
		//明文长度
		FileWrite(pFileName, "ab", (unsigned char *)"明文长度", 8);
		FileWrite(pFileName, "ab", equal, 2);
		sprintf((char*)pTmpData, "%08x", plainLenTemp);
		FileWrite(pFileName, "ab", pTmpData, 8);
		FileWrite(pFileName, "ab", newline, 2);

		rv = hEObj->SM2ECCEncrypt(tempPublickey, pInData, plainLenTemp, pCipherData, (unsigned long*)&cipherLen);
		if (rv)
		{
			printf("SM2加密错误，错误码[0x%08x]\n", rv);
			break;
			//return rv;
		}
		cipher2GMStand(pCipherData, pGMStdCipher, plainLenTemp);
		//gmStdCipherLen = strlen((char*)pGMStdCipher);
		PrintData("SM2->加密结果", pGMStdCipher, gmStdCipherLen, 16);
		//密文
		FileWrite(pFileName, "ab", (unsigned char *)"密文", 4);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(pGMStdCipher, gmStdCipherLen, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);

		std::pair<int, int> pri4DecryptInfo = std::make_pair(hEObj->gmOutCard, 0);
		rv = hEObj->SM2ECCDecrypt(pri4DecryptInfo, tempPrivatekey, pCipherData, cipherLen, pOutData, (unsigned long*)&nOutDataLen);
		if (rv)
		{
			printf("SM2解密错误，错误码[0x%08x]\n", rv);
			break;
			//return rv;
		}
		PrintData("SM2->解密结果", pOutData, nOutDataLen, 16);
		if (memcmp(pOutData, pInData, nOutDataLen))
		{
			printf("SM2加密解密结果比较失败。\n");
		}
		else
		{
			printf("SM2加密解密结果比较成功。\n");
		}
		//明文
		FileWrite(pFileName, "ab", (unsigned char *)"明文", 4);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(pInData, plainLenTemp, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);
		FileWrite(pFileName, "ab", newline, 2);
		delete[] pGMStdCipher;
	}
	printf("采集SM2加密解密数据完成。\n");
	return 0;
}
//SM2 签名数据 有预处理
int GMCheck::getDataSM2Sign(int dataSetCnt, unsigned int plainLen)
{
	int rv = 0;
	int i = 0;
	unsigned char pInData[10240], pTmpData[10240], pucID[256], pHashData[32];
	unsigned long signedLen = 64;
	unsigned char* pSignedBuf = new unsigned char[signedLen];
	unsigned int nTmpDataLen, nHashDateLen=32;
	char pFileName[128] = { 0x00 };
	unsigned char newline[] = { 0x0D,0x0A }, equal[] = { 0x3D,0x20 };

	//printf("\n请输入要采集数据的组数：");
	sprintf(pFileName, "data/SM2签名-预处理后-%d.txt", dataSetCnt);
	FileWrite(pFileName, "wb", NULL, 0);

	for (i = 0; i<dataSetCnt; i++)
	{
		printf("\n");
		printf("\n");
		printf("SM2签名数据采集。\n");
		//printf("\n输入数据长度(32~256):");
		
		rv = hEObj->GenerateRandom(plainLen, pInData);
		if (rv)
		{
			printf("生成随机明文错误，错误码[0x%08x]\n", rv);
			break;
			//return rv;
		}
		rv = hEObj->SM2GenECCKeyPair();
		if (rv)
		{
			printf("生成随机SM2密钥对错误，错误码[0x%08x]\n", rv);
			break;
			//return rv;
		}
		std::pair<unsigned char*, int> tempPublickey = hEObj->getPublicKey();
		std::pair<unsigned char*, int> tempPrivatekey = hEObj->getPrivateKey();
		//公钥
		FileWrite(pFileName, "ab", (unsigned char *)"公钥", 4);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(tempPublickey.first + 1, tempPublickey.second - 1, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);
		//私钥
		FileWrite(pFileName, "ab", (unsigned char *)"私钥", 4);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(tempPrivatekey.first, tempPrivatekey.second, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);
		printf("\n");
		printf("\n");
		PrintData("SM2签名验签公钥", tempPublickey.first + 1, tempPublickey.second - 1, 16);
		PrintData("SM2签名验签私钥", tempPrivatekey.first, tempPrivatekey.second, 16);
		PrintData("SM2签名验签明文", pInData, plainLen, 16);

		////签名数据长度
		//FileWrite(pFileName, "ab", (unsigned char *)"签名数据长度", 12);
		//FileWrite(pFileName, "ab", equal, 2);
		//sprintf((char*)pTmpData, "%08x", plainLen);
		//FileWrite(pFileName, "ab", pTmpData, 8);
		//FileWrite(pFileName, "ab", newline, 2);
		////签名数据
		//FileWrite(pFileName, "ab", (unsigned char *)"签名数据", 8);
		//FileWrite(pFileName, "ab", equal, 2);
		//Data_Bin2Txt(pInData, plainLen, (char*)pTmpData, (int*)&nTmpDataLen);
		//FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		//FileWrite(pFileName, "ab", newline, 2);

		unsigned char hashData[32] = { 0 };
		unsigned long hashDataLen = 32;
		
		rv = hEObj->SM3HashTotal(pInData, plainLen, hashData, &hashDataLen);
		if (rv)
		{
			printf("SM3杂凑处理错误\n");
			break;
			//return rv;
		}
		//签名数据
		FileWrite(pFileName, "ab", (unsigned char *)"签名数据e", 9);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(hashData, hashDataLen, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);
		memset(pSignedBuf, 0, signedLen);
		std::pair<int, int> pri4SignInfo = std::make_pair(hEObj->gmOutCard, 0);
		rv = hEObj->SM2ECCSign(pri4SignInfo, tempPrivatekey, hashData, hashDataLen, pSignedBuf,&signedLen);
		if (rv)
		{
			printf("SM2签名错误，错误码[0x%08x]\n", rv);
			break;
			//return rv;
		}
		//签名结果
		FileWrite(pFileName, "ab", (unsigned char *)"签名结果", 8);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(pSignedBuf, signedLen, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);
		FileWrite(pFileName, "ab", newline, 2);
		PrintData("SM2->签名值", pSignedBuf, signedLen, 16);

		rv = hEObj->SM2ECCVerify(tempPublickey, hashData, hashDataLen, pSignedBuf, signedLen);
		if (rv)
		{
			printf("SM2验证签名错误，错误码[0x%08x]\n", rv);
			break;
			//return rv;
		}
		else
		{
			printf("SM2验证签名正确。\n");
		}
	}
	delete[] pSignedBuf;
	printf("采集SM2签名验签数据完成。\n");
	return rv;
}

int GMCheck::getDataSM2Verify(int dataSetCnt, unsigned int plainLen)
{
	int rv = 0;
	int i = 0;
	unsigned char pInData[10240], pTmpData[10240], pucID[256], pHashData[32];
	unsigned long signedLen = 64;
	unsigned char* pSignedBuf = new unsigned char[signedLen];
	unsigned int nTmpDataLen, nHashDateLen = 32;
	char pFileName[128] = { 0x00 };
	unsigned char newline[] = { 0x0D,0x0A }, equal[] = { 0x3D,0x20 };

	sprintf(pFileName, "data/SM2验签-预处理后-%d.txt", dataSetCnt);
	FileWrite(pFileName, "wb", NULL, 0);

	for (i = 0; i<dataSetCnt; i++)
	{
		printf("\n");
		printf("\n");
		printf("SM2验签数据采集。\n");
		//printf("\n输入数据长度(32~256):");

		rv = hEObj->GenerateRandom(plainLen, pInData);
		if (rv)
		{
			printf("生成随机明文错误，错误码[0x%08x]\n", rv);
			break;
			//return rv;
		}
		rv = hEObj->SM2GenECCKeyPair();
		if (rv)
		{
			printf("生成随机SM2密钥对错误，错误码[0x%08x]\n", rv);
			break;
			//return rv;
		}
		std::pair<unsigned char*, int> tempPublickey = hEObj->getPublicKey();
		std::pair<unsigned char*, int> tempPrivatekey = hEObj->getPrivateKey();
		//公钥
		FileWrite(pFileName, "ab", (unsigned char *)"公钥", 4);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(tempPublickey.first + 1, tempPublickey.second - 1, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);
		//私钥
		FileWrite(pFileName, "ab", (unsigned char *)"私钥", 4);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(tempPrivatekey.first, tempPrivatekey.second, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);
		printf("\n");
		printf("\n");
		PrintData("SM2签名验签公钥", tempPublickey.first + 1, tempPublickey.second - 1, 16);
		PrintData("SM2签名验签私钥", tempPrivatekey.first, tempPrivatekey.second, 16);
		PrintData("SM2签名验签明文", pInData, plainLen, 16);

		//rv = hEObj->GenerateRandom(plainLen, pucID);
		//if (rv)
		//{
		//	printf("生成随机ID错误，错误码[0x%08x]\n", rv);
		//	return rv;
		//}
		////签名者ID
		//FileWrite(pFileName, "ab", (unsigned char *)"签名者ID", 8);
		//FileWrite(pFileName, "ab", equal, 2);
		//Data_Bin2Txt(pucID, plainLen, (char*)pTmpData, (int*)&nTmpDataLen);
		//FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		//FileWrite(pFileName, "ab", newline, 2);

		////签名数据长度
		//FileWrite(pFileName, "ab", (unsigned char *)"签名数据长度", 12);
		//FileWrite(pFileName, "ab", equal, 2);
		//sprintf((char*)pTmpData, "%08x", plainLen);
		//FileWrite(pFileName, "ab", pTmpData, 8);
		//FileWrite(pFileName, "ab", newline, 2);
		////签名数据
		//FileWrite(pFileName, "ab", (unsigned char *)"签名数据", 8);
		//FileWrite(pFileName, "ab", equal, 2);
		//Data_Bin2Txt(pInData, plainLen, (char*)pTmpData, (int*)&nTmpDataLen);
		//FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		//FileWrite(pFileName, "ab", newline, 2);

		unsigned char hashData[32] = { 0 };
		unsigned long hashDataLen = 32;

		rv = hEObj->SM3HashTotal(pInData, plainLen, hashData, &hashDataLen);
		if (rv)
		{
			printf("SM3杂凑处理错误\n");
			break;
			//return rv;
		}
		//签名数据
		FileWrite(pFileName, "ab", (unsigned char *)"签名数据e", 9);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(hashData, hashDataLen, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);
		memset(pSignedBuf, 0, signedLen);
		std::pair<int, int> pri4SignInfo = std::make_pair(hEObj->gmOutCard, 0);
		rv = hEObj->SM2ECCSign(pri4SignInfo, tempPrivatekey, hashData, hashDataLen, pSignedBuf, &signedLen);
		if (rv)
		{
			printf("SM2签名错误，错误码[0x%08x]\n", rv);
			break;
			//return rv;
		}
		//签名结果
		FileWrite(pFileName, "ab", (unsigned char *)"签名结果", 8);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(pSignedBuf, signedLen, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);
		FileWrite(pFileName, "ab", newline, 2);
		PrintData("SM2->签名值", pSignedBuf, signedLen, 16);

		rv = hEObj->SM2ECCVerify(tempPublickey, hashData, hashDataLen, pSignedBuf, signedLen);
		if (rv)
		{
			printf("SM2验证签名错误，错误码[0x%08x]\n", rv);
			break;
			//return rv;
		}
		else
		{
			printf("SM2验证签名正确。\n");
		}
	}
	delete[] pSignedBuf;
	printf("采集SM2签名验签数据完成。\n");
	return rv;
}
//SM2 密钥对生成
int GMCheck::getDataSM2KeyPair(int dataSetCnt)
{
	int rv;
	int i = 0;
	unsigned char pTmpData[10240];
	unsigned int nTmpDataLen;
	char pFileName[128] = { 0x00 };
	unsigned char newline[] = { 0x0D,0x0A }, equal[] = { 0x3D,0x20 };

	//printf("\n请输入要采集数据的组数：");
	sprintf(pFileName, "data/SM2_密钥对生产_%d.txt", dataSetCnt);
	FileWrite(pFileName, "wb", NULL, 0);

	for (i = 0; i<dataSetCnt; i++)
	{
		printf("\n");
		printf("\n");
		printf("++++++++++产生SM2密钥对，第%d组++++++++++\n", i + 1);
		rv = hEObj->SM2GenECCKeyPair();
		if (rv)
		{
			printf("生成随机SM2密钥对错误，错误码[0x%08x]\n", rv);
			return rv;
		}
		std::pair<unsigned char*, int> tempPublickey = hEObj->getPublicKey();
		std::pair<unsigned char*, int> tempPrivatekey = hEObj->getPrivateKey();
		//公钥
		FileWrite(pFileName, "ab", (unsigned char *)"公钥", 4);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(tempPublickey.first + 1, tempPublickey.second - 1, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);
		//私钥
		FileWrite(pFileName, "ab", (unsigned char *)"私钥", 4);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(tempPrivatekey.first, tempPrivatekey.second, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);
		FileWrite(pFileName, "ab", newline, 2);
		printf("\n");
		printf("\n");
		PrintData("SM2签名验签公钥", tempPublickey.first + 1, tempPublickey.second - 1, 16);
		PrintData("SM2签名验签私钥", tempPrivatekey.first, tempPrivatekey.second, 16);
	}
	printf("SM2密钥对数据采集完成。\n");
	return 0;
}
//SM3 杂凑
int GMCheck::getDataSM3(int dataSetCnt, unsigned int plainLen)
{
	int rv;
	int i = 0;
	unsigned char pInData[10240], pHashData[32], pTmpData[10240];
	unsigned int nHashDateLen=32, nTmpDataLen;
	char pFileName[128] = { 0x00 };
	unsigned char newline[] = { 0x0D,0x0A }, equal[] = { 0x3D,0x20 };

	HardEncrypt::SM3Hash objSM3(hEObj);
	//printf("\n请输入要采集数据的组数：");
	sprintf(pFileName, "data/SM3杂凑验证-%d.txt", dataSetCnt);
	FileWrite(pFileName, "wb", NULL, 0);
	for (i = 0; i<dataSetCnt; i++)
	{
		unsigned plainLenTemp = plainLen * ((i % 5) + 1);
		printf("\n");
		printf("\n");
		printf("SM3杂凑数据采集。\n");
		//printf("请选择输入数据的长度(16~8000)。\n");
		
		rv = hEObj->GenerateRandom(plainLenTemp, pInData);
		if (rv)
		{
			printf("生成随机明文错误，错误码[0x%08x]\n", rv);
			return rv;
		}
		//消息长度
		FileWrite(pFileName, "ab", (unsigned char *)"消息长度", 8);
		FileWrite(pFileName, "ab", equal, 2);
		sprintf((char*)pTmpData, "%08x", plainLenTemp);
		FileWrite(pFileName, "ab", pTmpData, 8);
		FileWrite(pFileName, "ab", newline, 2);

		//消息
		FileWrite(pFileName, "ab", (unsigned char *)"消息", 4);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(pInData, plainLenTemp, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);

		PrintData("SM3杂凑_明文", pInData, plainLenTemp, 16);
		
		unsigned char hashData[32] = { 0 };
		int HashDataLen = 0;
		objSM3.SM3HashInitFun();
		objSM3(pInData, plainLenTemp);
		objSM3.SM3HashFinalFun(pHashData, (unsigned long*)&nHashDateLen);

		PrintData("SM3杂凑结果", pHashData, nHashDateLen, 16);
		//杂凑结果
		FileWrite(pFileName, "ab", (unsigned char *)"杂凑值", 6);
		FileWrite(pFileName, "ab", equal, 2);
		Data_Bin2Txt(pHashData, nHashDateLen, (char*)pTmpData, (int*)&nTmpDataLen);
		FileWrite(pFileName, "ab", pTmpData, nTmpDataLen);
		FileWrite(pFileName, "ab", newline, 2);
		FileWrite(pFileName, "ab", newline, 2);
		printf("SM3杂凑成功。\n");
	}
	printf("采集SM3杂凑数据完成。\n");
	return 0;
}

int GMCheck::getDataSMALL(int dataSetCnt, unsigned int plainLen)
{
	//不提供CBC数据生成输出
	//getDataSM4EncDec_CBC_Enc(dataSetCnt, plainLen);
	//getDataSM4EncDec_CBC_Dec(dataSetCnt, plainLen);
	unsigned int plainLen4All = 16;
	getDataSM4EncDec_ECB_Enc(dataSetCnt, plainLen4All);
	getDataSM4EncDec_ECB_Dec(dataSetCnt, plainLen4All);
	getDataSM2EncDec_Enc(dataSetCnt, plainLen4All);
	getDataSM2EncDec_Dec(dataSetCnt, plainLen4All);
	getDataSM2Sign(dataSetCnt, plainLen4All);
	getDataSM2Verify(dataSetCnt, plainLen4All);
	getDataSM2KeyPair(dataSetCnt);
	getDataSM3(dataSetCnt, plainLen4All);
	return 0;
}

std::pair<bool, std::string> GMCheck::getAlgTypeData(int algType, int dataSetCnt, unsigned int plainDataLen)
{
	std::string errMsg("");
	/*char pPathName[16] = { 0x00 };
	sprintf(pPathName, "./data/");*/
	std::string pathName("./data/");
	if (dataFolderCheck(pathName))
	{
		errMsg = "生成data路径失败!";
		printf("%s\n", errMsg.c_str());
		return std::make_pair(false, errMsg);
	}

	switch (algType)
	{
	case GMCheck::SM2KEY:
		getDataSM2KeyPair(dataSetCnt);
		break;
	case GMCheck::SM2ENC:
		getDataSM2EncDec_Enc(dataSetCnt, plainDataLen);
		break;
	case GMCheck::SM2DEC:
		getDataSM2EncDec_Dec(dataSetCnt, plainDataLen);
		break;
	case GMCheck::SM2SIG:
		getDataSM2Sign(dataSetCnt, plainDataLen);
		break;
	case GMCheck::SM2VER:
		getDataSM2Verify(dataSetCnt, plainDataLen);
		break;
	case GMCheck::SM3DAT:
		getDataSM3(dataSetCnt, plainDataLen);
		break;
	case GMCheck::SM4CBCE:
		//gmCheckObj->getDataSM4EncDec_CBC_Enc(dataSetCount, plainDataLen);
		errMsg = "Don't support sm4 cbc mode";
		return std::make_pair(false, errMsg);
		break;
	case GMCheck::SM4CBCD:
		//gmCheckObj->getDataSM4EncDec_CBC_Dec(dataSetCount, plainDataLen);
		errMsg = "Don't support sm4 cbc mode";
		return std::make_pair(false, errMsg);
		break;
	case GMCheck::SM4ECBE:
		getDataSM4EncDec_ECB_Enc(dataSetCnt, plainDataLen);
		break;
	case GMCheck::SM4ECBD:
		getDataSM4EncDec_ECB_Dec(dataSetCnt, plainDataLen);
		break;
	case GMCheck::SM_ALL_DAT:
		getDataSMALL(dataSetCnt, plainDataLen);
		break;
	default:
		errMsg = "alg_type invalid";
		return std::make_pair(false, errMsg);
	}
	return std::make_pair(true, "");
}

int GMCheck::dataFolderCheck(std::string pathname)
{
	if (access(pathname.c_str(), 0))
	{
		size_t pre = 0, pos = 0;
		std::string tempDir("");
		while ((pos = pathname.find_first_of('/', pre)) != std::string::npos) {
			tempDir = pathname.substr(0, pos++);
			pre = pos;
			if (tempDir.size() == 0 || tempDir.size() == 1) continue; // if leading / first time is 0 length
			if (access(tempDir.c_str(), 0))
			{
				int mkditRes = mkdir(tempDir.c_str(), 493);//493 dec = 755 oct
				if (mkditRes) {
					return mkditRes;
				}
			}
		}
		//return mkdir(pathname.c_str(), 493);//493 dec = 755 oct
	}
	return 0;
}

int GMCheck::FileWrite(char *filename, char *mode, unsigned char *buffer, size_t size)
{
	FILE *fp;
	int rw, rwed;

	if ((fp = fopen(filename, mode)) == NULL)
	{
		return 0;
	}
	rwed = 0;
	while (size > rwed)
	{
		if ((rw = fwrite(buffer + rwed, 1, size - rwed, fp)) <= 0)
		{
			break;
		}
		rwed += rw;
	}
	fclose(fp);
	return rwed;
}

int GMCheck::Data_Bin2Txt(unsigned char *binData, int binDataLen, char *txtData, int *txtDataLen)
{
	int i, k;
	unsigned char t;

	*txtDataLen = (binDataLen << 1);

	k = 0;
	for (i = 0; i<binDataLen; i++)
	{
		t = (binData[i] >> 4);
		if (t<10)
		{
			txtData[k++] = t + '0';
		}
		else
		{
			txtData[k++] = t - 10 + 'A';
		}

		t = binData[i] & 0x0F;
		if (t<10)
		{
			txtData[k++] = t + '0';
		}
		else
		{
			txtData[k++] = t - 10 + 'A';
		}
	}

	return 1;

}

int GMCheck::PrintData(char *itemName, unsigned char *sourceData, unsigned int dataLength, unsigned int rowCount)
{
	int i, j;

	if ((sourceData == NULL) || (rowCount == 0) || (dataLength == 0))
		return -1;

	if (itemName != NULL)
		printf("%s[%d]:\n", itemName, dataLength);

	for (i = 0; i<(int)(dataLength / rowCount); i++)
	{
		printf("%08x  ", i*rowCount);
		for (j = 0; j<(int)rowCount; j++)
		{
			printf("%02x ", *(sourceData + i*rowCount + j));
		}

		printf("\n");
	}
	if (!(dataLength%rowCount))
		return 0;

	printf("%08x  ", (dataLength / rowCount)*rowCount);
	for (j = 0; j<(int)(dataLength%rowCount); j++)
	{
		printf("%02x ", *(sourceData + (dataLength / rowCount)*rowCount + j));
	}
	printf("\n");
	return 0;
}

GMCheck* GMCheck::gmInstance = nullptr;
GMCheck* GMCheck::getInstance()
{
	if (gmInstance == nullptr)
	{
		gmInstance = new GMCheck();
	}
	return gmInstance;
}

#endif