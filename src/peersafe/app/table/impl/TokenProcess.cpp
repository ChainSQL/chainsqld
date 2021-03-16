#include <peersafe/app/table/TokenProcess.h>
#include <peersafe/crypto/AES.h>
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
        if (secret_key.keyTypeInt_ == KeyType::secp256k1 || secret_key.keyTypeInt_ == KeyType::ed25519)
		{
            secretkeyType = hEObj->comKey;
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
			if (secret_key.keyTypeInt_ == KeyType::gmalg)
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
			else if (secret_key.keyTypeInt_ == KeyType::gmInCard)
			{
				secretkeyType = hEObj->gmInCard;
				std::pair<int, int> pri4DecryptInfo = std::make_pair(secretkeyType, secret_key.encrytCardIndex_);
				std::pair<unsigned char*, int> pri4Decrypt = std::make_pair((unsigned char*)secret_key.data(), secret_key.size());
                Blob tempBlob;
				int rv = hEObj->SM2ECCDecrypt(pri4DecryptInfo, pri4Decrypt, (unsigned char*)&cipherBlob[0], cipherBlob.size(), tempBlob, true, sm4Handle);
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

	Blob TokenProcess::symmertryDecrypt(Blob rawEncrept, const PublicKey& publicKey)
	{
		if (isValidate)
		{
			Blob rawDecrypted;
            // if (secretkeyType == hEObj->comKey)
            auto const type = publicKeyType(publicKey);
            switch(*type)
            {
                case KeyType::gmalg:
                {
                    GmEncrypt* hEObj = GmEncryptObj::getInstance();
                    unsigned char *pPlainData = new unsigned char[rawEncrept.size()];
                    unsigned long plainDataLen = rawEncrept.size();
                    if (secretkeyType == hEObj->gmInCard && sm4Handle != nullptr)
                    {
                        hEObj->SM4SymDecrypt(hEObj->ECB, (unsigned char *)sm4Handle, 0, 
                            rawEncrept.data(), rawEncrept.size(), pPlainData, &plainDataLen, hEObj->gmInCard);
                    }
                    else if (passBlob.size() > 0)
                    {
                        /* both support gmOutCard and comKey, when comKey user grant to gmOutCard user, 
                        item use comkey, token secretType is comKey, gmOutCard user operate table, need
                        decrypt the raw. by LC */
                        hEObj->SM4SymDecrypt(hEObj->ECB, passBlob.data(), passBlob.size(), 
                            rawEncrept.data(), rawEncrept.size(), pPlainData, &plainDataLen);
                    }
                    rawDecrypted = Blob(pPlainData, pPlainData + plainDataLen);
                    delete[] pPlainData;
                    break;
                }
                case KeyType::secp256k1:
                case KeyType::ed25519:
                {
                    rawDecrypted = ripple::decryptAES(passBlob, rawEncrept);
                    break;
                }
                default:
                    break;
            }
			return rawDecrypted;
		}
		else {
			assert(0);
			Blob rawDecrypted;
			return rawDecrypted;
		}
	}
}
