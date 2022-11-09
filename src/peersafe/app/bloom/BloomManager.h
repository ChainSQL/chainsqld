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

    Blob
    getBloomBits(uint32_t bit, uint64_t section, uint256 lastHash);

    inline BloomHelper&
    bloomHelper()
    {
        return helper_;
    }

private:
    Schema& app_;
    beast::Journal j_;
    BloomHelper helper_;
};
}
