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

#include <peersafe/gmencrypt/hardencrypt/HardEncrypt.h>
#include <string.h>
#ifdef _WIN64 //== _WIN32
#include <Userenv.h>
#pragma comment(lib, "userenv.lib")
#else
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif // _WIN64

//define FLAG AND MODE __WFP
#define SM3_HASH_TYPE 4
#define SM4_SYM_ALG   6
#define ALG_MODE_CBC  2

const unsigned char g_gmRootPublicKeyX_[32] = { 0xf4,0xa5,0xe1,0x31,0xb2,0x46,0xf3,0xd8,0x84,0xa6,0x4e,0xe0,0xff,0x10,0x5a,0x73,0xd2,0x40,0xe2,0xbd,0xd5,0xf2,0x13,0x3a,0xaa,0xff,0xf5,0xf3,0x46,0xcf,0xae,0xaa };
const unsigned char g_gmRootPublicKeyY_[32] = { 0x27,0xc3,0xad,0x6d,0x91,0xb8,0x61,0x0c,0x65,0x15,0x2e,0xfa,0x19,0x86,0xc9,0x0f,0xf4,0x55,0xa5,0x62,0x02,0xca,0x54,0x48,0x66,0x1d,0x7d,0xa8,0x21,0xa6,0x71,0xfa };
const unsigned char g_gmRootPrivateKey_[32] = { 0x32,0xbb,0xdc,0x4c,0xf2,0x66,0xbf,0x6d,0x40,0x8c,0x2a,0x24,0x35,0x4c,0x72,0x83,0xc2,0xb7,0x78,0xce,0xf4,0x91,0xc6,0x0d,0xfc,0x27,0xdd,0x2a,0xe2,0x14,0x56,0x81 };
//"Root account_id" : "rN7TwUjJ899savNXZkNJ8eFFv2VLKdESxj"
//"Root private_key": "p97evg5Rht7ZB7DbEpVqmV3yiSBMsR3pRBKJyLaRWt7SL5gEeBb"
//"Root public_key" : "pYvWhW4crFwcnovo5MhL71j5PyTWSJi2NVuzPYUzE9UYcSVLp29RhtssQB7seGvFmdjbtKRrBQ4g9bCW5hjBQSeb7LePMwFM"
//static CRITICAL_SECTION csSM3;
std::mutex HardEncrypt::SM3Hash::mutexSM3_;

HardEncrypt::HardEncrypt() : hEkey_(nullptr)
, hSessionHandle_(nullptr)
, hashType_(SM3_HASH_TYPE)
, symAlgFlag_(SM4_SYM_ALG)
, symAlgMode_(ALG_MODE_CBC)
//, objSM3_(this)
{
    memset(pubKeyRoot_, 0, sizeof(pubKeyRoot_));
}

HardEncrypt::~HardEncrypt()
{
}

bool HardEncrypt::isHardEncryptExist()
{
    return isHardEncryptExist_;
}

std::string HardEncrypt::GetHomePath() {
	std::string homeStr = "";
#ifdef _WIN64
	char lpProfileDir[512];
	DWORD size = 512;
	memset(lpProfileDir, 0, 512);
	HANDLE hToken = nullptr;
	BOOL bres1 = OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &hToken);
	BOOL bres = GetUserProfileDirectory(hToken, lpProfileDir, &size);
	homeStr.replace(0, 1, lpProfileDir);
#else
	struct passwd *pw = getpwuid(getuid());
	const char *homedir = pw->pw_dir;
	homeStr.replace(0, 1, homedir);
#endif
	return homeStr;
}

std::pair<unsigned char*, int> HardEncrypt::getRootPublicKey()
{
    pubKeyRoot_[0] = GM_ALG_MARK;
    memcpy(pubKeyRoot_ + 1, g_gmRootPublicKeyX_,32);
    memcpy(pubKeyRoot_ + 33, g_gmRootPublicKeyY_, 32);
    return std::make_pair(pubKeyRoot_, sizeof(pubKeyRoot_));
}

std::pair<unsigned char*, int> HardEncrypt::getRootPrivateKey()
{
    return std::make_pair((unsigned char*)g_gmRootPrivateKey_, sizeof(g_gmRootPrivateKey_));
}

void HardEncrypt::pkcs5Padding(unsigned char* srcUC, unsigned long srcUCLen, unsigned char* dstUC, unsigned long* dstUCLen)
{
    int paddingData = 16 - srcUCLen % 16;
    int lenTimes = srcUCLen / 16;
    memcpy(dstUC, srcUC, srcUCLen);
    for (int i = 0; i < paddingData; ++i)
    {
        dstUC[srcUCLen + i] = paddingData;
    }
    *dstUCLen = (lenTimes + 1) * 16;
}

void HardEncrypt::dePkcs5Padding(unsigned char* srcUC, unsigned long srcUCLen, unsigned char* dstUC, unsigned long* dstUCLen)
{
    int dePaddingData = srcUC[srcUCLen - 1];
    *dstUCLen = srcUCLen - dePaddingData;
    memcpy(dstUC, srcUC, *dstUCLen);
}

int HardEncrypt::FileWrite(const char *filename, char *mode, unsigned char *buffer, size_t size)
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
/*HardEncrypt::SM3Hash &HardEncrypt::getSM3Obj()
{
    return objSM3_;
}*/

HardEncrypt::SM3Hash::SM3Hash(HardEncrypt *pEncrypt)
{
#ifdef SD_KEY_SWITCH
    mutexSM3_.lock();
#endif
    pHardEncrypt_ = pEncrypt;
}

HardEncrypt::SM3Hash::~SM3Hash()
{
#ifdef SD_KEY_SWITCH
    mutexSM3_.unlock();
#endif
}

void HardEncrypt::SM3Hash::SM3HashInitFun()
{
    pHardEncrypt_->SM3HashInit(&hSM3Handle_);
}

void HardEncrypt::SM3Hash::SM3HashFinalFun(unsigned char *pHashData, unsigned long *pulHashDataLen)
{
    pHardEncrypt_->SM3HashFinal(hSM3Handle_, pHashData,pulHashDataLen);
}

void HardEncrypt::SM3Hash::operator()(void const* data, std::size_t size) noexcept
{
    pHardEncrypt_->operator()(hSM3Handle_, data,size);
}
