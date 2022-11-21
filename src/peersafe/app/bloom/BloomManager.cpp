
#include <peersafe/app/bloom/BloomManager.h>
#include <peersafe/schema/Schema.h>
#include <peersafe/core/Tuning.h>
#include <ripple/shamap/NodeFamily.h>
#include <ripple/nodestore/Database.h>
#include <ripple/app/ledger/LedgerMaster.h>

namespace ripple {

BloomManager::BloomManager(Schema& app, beast::Journal j)
    : app_(app), 
    j_(j),
    helper_(),
    indexer_(app, j),
    filterApi_(app),
    inited_(false),
    bloomStartSeq_()
{
}

uint256
BloomManager::bloomStartLedgerKey()
{
    return sha512Half<CommonKey::sha>(BLOOM_PREFIX + BLOOM_START_LEDGER_KEY);
}

void
BloomManager::init()
{
    if (inited_)
        return;
    inited_ = true;
    loadBloomStartLedger();
    indexer_.init(bloomStartSeq_);
}

void BloomManager::saveBloomStartLedger(uint32_t seq, uint256 const& hash)
{
    bloomStartSeq_ = seq;
    indexer_.setBloomStartSeq(seq);
    Serializer s(128);
    s.add32(seq);
    s.add256(hash);

    app_.getNodeStore().store(
        hotBLOOM_START_LEDGER,
        std::move(s.modData()),
        bloomStartLedgerKey(),
        seq);
}

void
BloomManager::loadBloomStartLedger()
{
    if (auto obj = app_.getNodeFamily().db().fetch(bloomStartLedgerKey(), 0))
    {
        SerialIter s(makeSlice(obj->getData()));
        auto seq = s.get32();
        auto hash = s.get256();
        bloomStartSeq_ = seq;
        auto ledger = app_.getLedgerMaster().getLedgerByHash(hash);
        assert(ledger != nullptr && ledger->info().seq == seq);
    }
}

boost::optional<uint32_t>
BloomManager::getBloomStartSeq()
{
    return bloomStartSeq_;
}

uint32_t
BloomManager::getSectionBySeq(uint32_t seq)
{
    assert(bloomStartSeq_ != boost::none);
    return (seq - *bloomStartSeq_) / DEFAULT_SECTION_SIZE;
}

std::pair<uint32_t, uint32_t>
BloomManager::getSectionRange(uint32_t section)
{
    return indexer_.getSectionRange(section);
}

std::tuple<uint32_t, uint32_t, uint8_t>
BloomManager::getLedgerLocation(uint32_t seq)
{
    uint32_t section = getSectionBySeq(seq);
    auto index = seq - (*bloomStartSeq_ + section * DEFAULT_SECTION_SIZE);
    return std::make_tuple(section, index / 8, uint8_t(7 - index % 8));    
}

}
