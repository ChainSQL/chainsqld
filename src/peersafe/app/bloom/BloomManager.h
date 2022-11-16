#pragma once

#include <ripple/basics/Blob.h>
#include <ripple/basics/base_uint.h>
#include <ripple/beast/utility/Journal.h>
#include <peersafe/app/bloom/BloomHelper.h>
#include <peersafe/app/bloom/BloomIndexer.h>

namespace ripple {
class Schema;

class BloomManager
{
public:
    BloomManager(Schema& app, beast::Journal j); 

    void
    init();

    void
    saveBloomStartLedger(uint32_t seq, uint256 const& hash);

    void
    loadBloomStartLedger();

    inline BloomHelper&
    bloomHelper()
    {
        return helper_;
    }

    inline BloomIndexer&
    bloomIndexer()
    {
        return indexer_;
    }

    boost::optional<uint32_t>
    getBloomStartSeq();

    uint32_t
    getSectionBySeq(uint32_t seq);

private:
    uint256
    bloomStartLedgerKey();

private:
    Schema& app_;
    beast::Journal j_;
    BloomHelper helper_;
    BloomIndexer indexer_; 
    bool inited_;
    boost::optional<uint32_t> bloomStartSeq_;
};
}
