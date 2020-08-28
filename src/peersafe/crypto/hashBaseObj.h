#ifndef HASH_BASE_OBJ_H_INCLUDE
#define HASH_BASE_OBJ_H_INCLUDE
#include <peersafe/crypto/hashBase.h>
#include <peersafe/gmencrypt/GmEncrypt.h>
#include <ripple/protocol/CommonKey.h>
#include <ripple/protocol/digest.h>

namespace ripple {

    class hashBaseObj
    {
    public:
        static hashBase *getHasher(CommonKey::HashType hashType = CommonKey::hashTypeGlobal)
        {
            switch (hashType)
            {
            case CommonKey::sm3:
            {
                GmEncrypt *hEObj = GmEncryptObj::getInstance();
                GmEncrypt::SM3Hash objSM3(hEObj);
                return &objSM3;
            }
            case CommonKey::sha:
            default:
            {
                sha512_half_hasher h;
                return &h;
            }
            }
        };
    };
}

#endif