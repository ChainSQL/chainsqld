#include <peersafe/app/table/TokenProcess.h>
#include <peersafe/gmencrypt/GmEncryptObj.h>

namespace ripple {
	TokenProcess::TokenProcess():sm4Handle(nullptr), isValidate(false)
	{
	}
	TokenProcess::~TokenProcess()
	{
		sm4Handle = nullptr;
	}
	bool TokenProcess::setSymmertryKey(const Blob& cipherBlob, const SecretKey& secret_key)
	{
		GmEncrypt* hEObj = GmEncryptObj::getInstance();
		// if (nullptr == hEObj)
        if (secret_key.keyTypeInt_ == hEObj->comKey)
		{
			passBlob = ripple::decrypt(cipherBlob, secret_key);
			if (passBlob.size() > 0)
			{
				isValidate = true;
			}
			else
			{
				isValidate = false;
			}
			return isValidate;
		}
		else
		{
			if (secret_key.keyTypeInt_ == hEObj->gmOutCard)
			{
				secretkeyType = hEObj->gmOutCard;
				passBlob = ripple::decrypt(cipherBlob, secret_key);
				if (passBlob.size() > 0)
				{
					isValidate = true;
				}
				else
				{
					isValidate = false;
				}
			}
			else if (secret_key.keyTypeInt_ == hEObj->gmInCard)
			{
				secretkeyType = hEObj->gmInCard;
				std::pair<int, int> pri4DecryptInfo = std::make_pair(secret_key.keyTypeInt_, secret_key.encrytCardIndex_);
				std::pair<unsigned char*, int> pri4Decrypt = std::make_pair((unsigned char*)secret_key.data(), secret_key.size());
				unsigned long lHandle = 0; 
				int rv = hEObj->SM2ECCDecrypt(pri4DecryptInfo, pri4Decrypt, (unsigned char*)&cipherBlob[0], cipherBlob.size(), (unsigned char*)(&lHandle) , nullptr, true);
				sm4Handle = (void *)lHandle;
				if (rv)
				{
					DebugPrint("ECCDecrypt error! rv = 0x%04x", rv);
					isValidate = false;
				}
				else if (sm4Handle != nullptr)
				{
					DebugPrint("ECCDecrypt OK!");
					isValidate = true;
				}
			}
			else isValidate = false;
			return isValidate;
		}
		return false;
	}

	Blob TokenProcess::symmertryDecrypt(Blob rawEncrept)
	{
		if (isValidate)
		{
			Blob rawDecrypted;
			GmEncrypt* hEObj = GmEncryptObj::getInstance();
            if (secretkeyType == hEObj->comKey)
			{
				rawDecrypted = RippleAddress::decryptAES(passBlob, rawEncrept);
			}
			else
			{
				unsigned char* pPlainData = new unsigned char[rawEncrept.size()];
				unsigned long plainDataLen = rawEncrept.size();
				if (secretkeyType == hEObj->gmInCard && sm4Handle != nullptr )
				{
					hEObj->SM4SymDecrypt(hEObj->ECB, (unsigned char*)sm4Handle, 0, rawEncrept.data(), rawEncrept.size(), pPlainData, &plainDataLen, hEObj->gmInCard);
				}
				else if(secretkeyType == hEObj->gmOutCard && passBlob.size() > 0)
				{
					hEObj->SM4SymDecrypt(hEObj->ECB, passBlob.data(), passBlob.size(), rawEncrept.data(), rawEncrept.size(), pPlainData, &plainDataLen);
				}
				rawDecrypted = Blob(pPlainData, pPlainData + plainDataLen);
				delete[] pPlainData;
			}
			return rawDecrypted;
		}
	}
}