#pragma once

#include <ripple/basics/Blob.h>
#include <ripple/basics/base_uint.h>
#include <ripple/beast/utility/Journal.h>
#include <peersafe/app/bloom/BloomHelper.h>

namespace ripple {
class Schema;

class BloomManager
{
public:
    BloomManager(Schema& app, beast::Journal j) : app_(app), j_(j)
    {
    }

    // getBloomBits returns the bit vector belonging to the given bit index after all
    // blooms have been added.
    Blob
    getBloomBits(uint32_t bit, uint64_t section, uint256 lastHash);

    void
    saveBloomStartLedger(uint32_t seq, uint256 const& hash);

    void
    loadBloomStartLedger();

    inline BloomHelper&
    bloomHelper()
    {
        return helper_;
    }

    boost::optional<uint32_t>
    getBloomStartSeq();

private:
    uint256
    bloomStartLedgerKey();

private:
    Schema& app_;
    beast::Journal j_;
    BloomHelper helper_;
    boost::optional<uint32_t> bloomStartSeq_;
};
}
