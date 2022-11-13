#pragma once

#include <boost/optional.hpp>
#include <ripple/basics/base_uint.h>
#include <ripple/beast/utility/Journal.h>
#include <peersafe/core/Tuning.h>

namespace ripple {
class Schema;
class ReadView;

class BloomGenerator
{
public:
    BloomGenerator();
    ~BloomGenerator();
    void
    addBloom(uint32_t index,uint2048 const& bloom);
    uint8_t* 
    bitSet(uint32_t idx);
private:
    uint8_t** blooms;
};

class BloomIndexer
{
public:
    BloomIndexer(Schema& app, beast::Journal j);

    void init(boost::optional<uint32_t> startSeq);

    void
    onPubLedger(std::shared_ptr<ReadView const> const& lpAccepted);

    // getBloomBits returns the bit vector belonging to the given bit index
    // after all
    // blooms have been added.
    Blob
    getBloomBits(uint32_t bit, uint32_t section, uint256 lastHash);

private:
    void
    processSections();

    bool
    processSection(uint32_t section);

    void
    readStoredSection();

    void
    saveStoredSection(uint32_t section);

    uint256
    bloomSectionCountKey();

    uint256
    bloomBitsKey(uint32_t bit,uint32_t section,uint256 lastHash);
private:
    Schema&         app_;
    beast::Journal  j_;
    uint32_t        storedSections_;   // Number of sections successfully indexed into the database
    uint32_t        knownSections_;    // Number of sections known to be complete (block wise)
    boost::optional<uint32_t> bloomStartSeq_;
};

}  // namespace ripple
