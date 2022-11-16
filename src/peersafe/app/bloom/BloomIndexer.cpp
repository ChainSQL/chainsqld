#include <ripple/ledger/ReadView.h>
#include <ripple/core/JobQueue.h>
#include <ripple/shamap/NodeFamily.h>
#include <ripple/nodestore/Database.h>
#include <ripple/protocol/digest.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <peersafe/app/bloom/BloomIndexer.h>
#include <peersafe/schema/Schema.h>

namespace ripple {

BloomGenerator::BloomGenerator()
{
    blooms = new uint8_t*[BLOOM_LENGTH];
    for (int i=0; i<BLOOM_LENGTH; i++)
    {
        blooms[i] = new uint8_t[DEFAULT_SECTION_SIZE/8];
        memset(blooms[i], 0, DEFAULT_SECTION_SIZE/8);
    }
}

BloomGenerator::~BloomGenerator()
{
    for (int i=0; i<BLOOM_LENGTH; i++)
    {
        delete []blooms[i];
    }
    delete []blooms;
}

void
BloomGenerator::addBloom(uint32_t index, uint2048 const& bloom)
{
    auto byteIndex = index / 8;
    auto bitIndex = uint8_t(7 - index%8);
    auto bloomByteLength = BLOOM_LENGTH / 8;
    for (int byt = 0; byt < bloomByteLength;  byt++)
    {
        auto bloomByte = bloom.data()[bloomByteLength - 1 - byt];
        if (bloomByte == 0)
            continue;
        auto base = 8 * byt;
        blooms[base + 7][byteIndex] |= ((bloomByte >> 7) & 1) << bitIndex;
        blooms[base + 6][byteIndex] |= ((bloomByte >> 6) & 1) << bitIndex;
        blooms[base + 5][byteIndex] |= ((bloomByte >> 5) & 1) << bitIndex;
        blooms[base + 4][byteIndex] |= ((bloomByte >> 4) & 1) << bitIndex;
        blooms[base + 3][byteIndex] |= ((bloomByte >> 3) & 1) << bitIndex;
        blooms[base + 2][byteIndex] |= ((bloomByte >> 2) & 1) << bitIndex;
        blooms[base + 1][byteIndex] |= ((bloomByte >> 1) & 1) << bitIndex;
        blooms[base][byteIndex] |= (bloomByte & 1) << bitIndex;
    }
}

uint8_t*
BloomGenerator::bitSet(uint32_t idx)
{
    assert(idx < BLOOM_LENGTH);
    return blooms[idx];
}

BloomIndexer::BloomIndexer(Schema& app, beast::Journal j)
    : app_(app), j_(j),
    storedSections_(0), 
    knownSections_(0)    
{
}

void
BloomIndexer::init(boost::optional<uint32_t> startSeq)
{
    if(bloomStartSeq_ = startSeq,bloomStartSeq_)
    {
        readStoredSection();
    }        
}

std::pair<uint32_t, uint32_t>
BloomIndexer::bloomStatus()
{
    return std::make_pair(DEFAULT_SECTION_SIZE, storedSections_);
}

Blob
BloomIndexer::getBloomBits(uint32_t bit, uint32_t section)
{
    if (!bloomStartSeq_)
        return Blob{};

    uint32_t lastSeq = *bloomStartSeq_ + (section + 1) * DEFAULT_SECTION_SIZE - 1;
    auto ledger = app_.getLedgerMaster().getLedgerBySeq(lastSeq);
    if (!ledger)
        return Blob{};
    uint256 lastHash = ledger->info().hash;
    if (auto obj = app_.getNodeFamily().db().fetch(bloomBitsKey(bit,section,lastHash), 0))
    {
        SerialIter s(makeSlice(obj->getData()));
        return s.getVL();
    }
    return Blob{};
}

void
BloomIndexer::onPubLedger(std::shared_ptr<ReadView const> const& lpAccepted)
{
    if (!bloomStartSeq_)
        return;

    knownSections_ =
        (lpAccepted->info().seq - *bloomStartSeq_ + 1) / DEFAULT_SECTION_SIZE;
    if (knownSections_ > storedSections_)
    {
        app_.getJobQueue().addJob(
            jtSAVE_SECTIONS,
            "advanceLedger",
            [this](Job&) { 
                processSections(); 
            },
            app_.doJobCounter());
    }
}

bool
BloomIndexer::processSection(uint32_t section)
{
    auto start = *bloomStartSeq_ + section * DEFAULT_SECTION_SIZE;
    auto end = *bloomStartSeq_ + (section + 1) * DEFAULT_SECTION_SIZE - 1;
    if (!app_.getLedgerMaster().haveLedger(start,end))
        return false;

    BloomGenerator bin;
    uint256 lastHash;
    for (auto seq = start; seq <= end; seq++)
    {
        auto ledger = app_.getLedgerMaster().getLedgerBySeq(seq);
        bin.addBloom(seq-start,ledger->info().bloom);
        if (seq == end)
            lastHash = ledger->info().hash;
    }
    //write to kv
    for (int i=0; i<BLOOM_LENGTH; i++)
    {
        auto bits = bin.bitSet(i);
        
        Serializer s(128);
        s.addVL(Slice(bits,DEFAULT_SECTION_SIZE/8));

        app_.getNodeStore().store(
            hotBLOOM_SECTION_BIT, 
            std::move(s.modData()),
            bloomBitsKey(i, section, lastHash), 
            0);
    }
    return true;
}

void
BloomIndexer::processSections()
{
    try
    {
        while (storedSections_ < knownSections_)
        {
            if (!processSection(storedSections_))
                break;
            storedSections_++;
            saveStoredSection(storedSections_);
            JLOG(j_.info()) << "BloomIndexer saved section:" + storedSections_;
        }
    }
    catch (std::exception const& e)
    {
        JLOG(j_.warn()) << "BloomIndexer exception when processSections:" << e.what();
    }
}

uint256
BloomIndexer::bloomSectionCountKey()
{
    return sha512Half<CommonKey::sha>(BLOOM_PREFIX + BLOOM_SAVED_SECTION_COUNT);
}

uint256
BloomIndexer::bloomBitsKey(
    uint32_t bit,
    uint32_t section,
    uint256 lastHash)
{
    return sha512Half<CommonKey::sha>(
        BLOOM_PREFIX,
        bit,
        section,
        lastHash
        );
}

void BloomIndexer::readStoredSection()
{
    if (auto obj = app_.getNodeFamily().db().fetch(bloomSectionCountKey(), 0))
    {
        SerialIter s(makeSlice(obj->getData()));
        storedSections_ = s.get32();
    }
}

void
BloomIndexer::saveStoredSection(uint32_t section)
{
    Serializer s(128);
    s.add32(section);

    app_.getNodeStore().store(
        hotBLOOM_SAVED_SECTION,
        std::move(s.modData()),
        bloomSectionCountKey(),
        0);
}

}