#pragma once

#include <ripple/basics/Blob.h>
#include <ripple/basics/base_uint.h>
#include <ripple/beast/utility/Journal.h>
#include <peersafe/app/bloom/BloomHelper.h>
#include <peersafe/app/bloom/BloomIndexer.h>
#include <peersafe/app/bloom/FilterApi.h>

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
    
    inline FilterApi&
    filterApi()
    {
        return filterApi_;
    }

    boost::optional<uint32_t>
    getBloomStartSeq();

    //Get section by ledger sequence
    uint32_t
    getSectionBySeq(uint32_t seq);

    //Get section range
    //first: start ledger sequence
    //end: end ledger sequence(include)
    std::pair<uint32_t, uint32_t>
    getSectionRange(uint32_t section);

    //Return ledger location
    //0:section number(0-...)
    //1:byte location (0-511)
    //2:bit location (0-7)
    std::tuple<uint32_t, uint32_t,uint8_t>
    getLedgerLocation(uint32_t seq);

private:
    uint256
    bloomStartLedgerKey();

private:
    Schema& app_;
    beast::Journal j_;
    BloomHelper helper_;
    BloomIndexer indexer_;
    FilterApi filterApi_;
    bool inited_;
    boost::optional<uint32_t> bloomStartSeq_;
};
}
