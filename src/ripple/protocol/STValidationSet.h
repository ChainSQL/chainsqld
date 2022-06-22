
#ifndef PEERSAFE_PROTOCOL_STVALIDATIONSET_H_INCLUDED
#define PEERSAFE_PROTOCOL_STVALIDATIONSET_H_INCLUDED


#include <ripple/protocol/STValidation.h>
#include <ripple/basics/FeeUnits.h>
#include <ripple/basics/Log.h>
#include <ripple/protocol/STObject.h>
#include <ripple/protocol/PublicKey.h>
#include <ripple/protocol/SecretKey.h>
#include <ripple/app/misc/Manifest.h>
#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>


namespace ripple {

class STValidationSet final : public STObject, public CountedObject<STValidationSet>
{
public:
    static char const* getCountedObjectName()
    {
        return "STValidationSet";
    }

    using pointer = std::shared_ptr<STValidationSet>;
    using ref = const std::shared_ptr<STValidationSet>&;
    using valSet = std::vector<std::shared_ptr<STValidation>>;

    STValidationSet(
        SerialIter& sit,
        PublicKey const& publicKey,
        ManifestCache& cache);

    STValidationSet(
        std::uint32_t validationSeq,
        uint256 validationHash,
        PublicKey const& publicKey,
        valSet set,
        NetClock::time_point closeTime);

    Blob getSerialized() const;

    const valSet& 
    getValSet();

    private:

    std::uint32_t mValidationSeq;
    uint256 mValidationHash;
    NodeID mNodeID;
    PublicKey mPublic;
    valSet mSet;

private:
    static SOTemplate const& validationSetFormat();
};

} // ripple

#endif
