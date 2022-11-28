#pragma once

#ifndef COMMONKEY_H_INCLUDE
#define COMMONKEY_H_INCLUDE

//#include <gmencrypt/GmEncryptObj.h>
#include <ripple/protocol/KeyType.h>
#include <peersafe/gmencrypt/GmEncrypt.h>

namespace ripple {
	class CommonKey {
	public:
        enum HashType { unknown = -1, sha, sm3, sha3};
        static KeyType chainAlgTypeG;
        static HashType chainHashTypeG;
        KeyType keyTypeInt_;
		int encrytCardIndex_;

	public:
		CommonKey() { keyTypeInt_ = KeyType::secp256k1; encrytCardIndex_ = 0; };
		CommonKey(KeyType keyType, int index):keyTypeInt_(keyType), encrytCardIndex_(index){ };

        // static void setAlgType(KeyType algTypeCnf) { chainAlgTypeG = algTypeCnf; };
        static bool setAlgType(std::string& nodeAlgTypeStr)
        {
            chainAlgTypeG = *(keyTypeFromString(nodeAlgTypeStr));
            
            if(chainAlgTypeG == KeyType::invalid) return false;
            else
            {
                return setHashTypeByAlgType(chainAlgTypeG);
            }
        }
        static std::string getAlgTypeStr()
        {
            return to_string(chainAlgTypeG);
        }

        static bool setHashTypeByAlgType(KeyType algType)
        {
            switch(algType)
            {
                case KeyType::secp256k1:
                case KeyType::ed25519:
                {
                    chainHashTypeG = HashType::sha;
                    return true;
                }
                case KeyType::gmalg:
                {
                    chainHashTypeG = HashType::sm3;
                    return true;
                }
                default:
                    return false;
            }
        }
        
        static bool setHashType(std::string& hashTypeStr)
        {
            auto hashType = hashTypeFromString(hashTypeStr);
            if (hashType == HashType::unknown)
                return false;
            chainHashTypeG = hashType;
            return true;
        }
        static std::string getHashTypeStr()
        {
            switch(chainHashTypeG)
            {
                case HashType::sha:
                    return "sha";
                case HashType::sm3:
                    return "sm3";
                default:
                    return "invalid";
            }
        }
        static HashType
        hashTypeFromString(std::string const& hashTypeStr)
        {
            if (hashTypeStr == "sha")
                return HashType::sha;
            else if (hashTypeStr == "sm3")
                return HashType::sm3;
            else
                return HashType::unknown;
        }
	};
}

#endif
