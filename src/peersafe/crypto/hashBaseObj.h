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
        static std::unique_ptr<hashBase> getHasher(CommonKey::HashType hashType = CommonKey::chainHashTypeG)
        {
            switch (hashType)
            {
            case CommonKey::sm3:
            {
                GmEncrypt *hEObj = GmEncryptObj::getInstance(GmEncryptObj::soft);
                // GmEncrypt::SM3Hash* pObjSM3 = new GmEncrypt::SM3Hash(hEObj);
                // return pObjSM3;
                return std::make_unique<GmEncrypt::SM3Hash>(hEObj);
            }
            case CommonKey::sha:
            default:
            {
                // sha512_half_hasher* ph = new sha512_half_hasher();
                // return ph;
                return std::make_unique<sha512_half_hasher>();
            }
            }
        };
    };
}

#endif