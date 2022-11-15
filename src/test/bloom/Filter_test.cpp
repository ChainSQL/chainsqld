//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2019 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include <string>
#include <tuple>

#include <ripple/protocol/Feature.h>
#include <ripple/protocol/jss.h>
#include <ripple/protocol/digest.h>
#include <ripple/beast/unit_test.h>

#include <peersafe/app/bloom/Filter.h>
#include <peersafe/app/bloom/Matcher.h>
#include <peersafe/app/bloom/Bloom.h>

#include <test/jtx.h>


namespace ripple {
namespace test {
class Filter_test : public beast::unit_test::suite {
public:
    void filter_test() {
        jtx::Env env{*this};
        Filter::pointer filter = Filter::newRangeFilter(env.app(), 1, 1, {}, {});
        filter->Logs();
        BEAST_EXPECT(true);
    }
    
//    void bloomBit_test() {
//        std::string s = "peersafe";
//        Slice slice(s.data(), s.size());
//        Bloom bloom;
//        uint32_t i1,i2,i3;
//        uint8_t v1,v2,v3;
//        std::tie(i1, v1, i2, v2, i3, v3) = bloom.bloomValues(slice);
//        
//        Matcher::bloomIndexes indexes = Matcher::calcBloomIndexes(slice);
//        
//        uint32_t bit_v1 = i1*8 + v1;
//        uint32_t bit_v2 = i2*8 + v2;
//        uint32_t bit_v3 = i3*8 + v3;
//        bool b1 = (2048 - bit_v1) == indexes[0] + 7;
//        bool b2 = (2048 - bit_v2) == indexes[1] + 7;
//        bool b3 = (2048 - bit_v3) == indexes[2] + 2;
//        BEAST_EXPECT(b1);
//        BEAST_EXPECT(b2);
//        BEAST_EXPECT(b3);
//    }
    
    void run() override {
        //bloomBit_test();
        filter_test();
    }
};

BEAST_DEFINE_TESTSUITE(Filter, bloom, ripple);

} // namespace test
} // namespace ripple

