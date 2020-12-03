#pragma once

#ifndef COMMONKEY_H_INCLUDE
#define COMMONKEY_H_INCLUDE

//#include <gmencrypt/GmEncryptObj.h>
#include <ripple/crypto/KeyType.h>
#include <peersafe/gmencrypt/GmEncrypt.h>

namespace ripple {
	class CommonKey {
	public:
        enum HashType { unknown = -1, sha, sm3};
        static KeyType algTypeGlobal;
        static HashType hashTypeGlobal;
		int keyTypeInt_;
		int encrytCardIndex_;

	public:
		CommonKey() { keyTypeInt_ = GmEncrypt::comKey; encrytCardIndex_ = 0; };
		CommonKey(int keyType, int index):keyTypeInt_(keyType), encrytCardIndex_(index){ };

        // static void setAlgType(KeyType algTypeCnf) { algTypeGlobal = algTypeCnf; };
        static bool setAlgType(std::string& nodeAlgTypeStr)
        {
            if (nodeAlgTypeStr == "secp256k1")
            {
                algTypeGlobal = KeyType::secp256k1;
                return true;
            }
            else if (nodeAlgTypeStr == "ed25519")
            {
                algTypeGlobal = KeyType::ed25519;
                return true;
            }
            else if (nodeAlgTypeStr == "gmalg")
            {
                algTypeGlobal = KeyType::gmalg;
                return true;
            }
            else return false;
        }
        static std::string getAlgTypeStr()
        {
            switch(algTypeGlobal)
            {
                case KeyType::ed25519:
                    return "ed25519";
                case KeyType::secp256k1:
                    return "secp256k1";
                case KeyType::gmalg:
                    return "gmalg";
                default:
                    return "invalid";
            }
        }
        static bool setHashType(std::string& hashTypeStr)
        {
            if (hashTypeStr == "sha")
            {
                hashTypeGlobal = HashType::sha;
                return true;
            }
            else if (hashTypeStr == "sm3")
            {
                hashTypeGlobal = HashType::sm3;
                return true;
            }
            else return false;
        }
        static std::string getHashTypeStr()
        {
            switch(hashTypeGlobal)
            {
                case HashType::sha:
                    return "sha";
                case HashType::sm3:
                    return "sm3";
                default:
                    return "invalid";
            }
        }
	};
}

#endif