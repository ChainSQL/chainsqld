
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
    indexer_(app, j), 
    inited_(false)
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

}
