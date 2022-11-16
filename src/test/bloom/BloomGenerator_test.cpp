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
#include <map>

#include <boost/algorithm/string/trim.hpp>

#include <ripple/protocol/Feature.h>
#include <ripple/protocol/jss.h>
#include <ripple/protocol/digest.h>
#include <ripple/beast/unit_test.h>

#include <peersafe/app/bloom/Matcher.h>
#include <peersafe/app/bloom/Bloom.h>
#include <peersafe/app/bloom/BloomIndexer.h>

#include <test/jtx.h>


namespace ripple {
namespace test {
class BloomGenerator_test : public beast::unit_test::suite {
public:
    uint256
    bloomBitsKey(uint32_t bit,
                 uint32_t section)
    {
        return sha512Half<CommonKey::sha>(BLOOM_PREFIX,
                                          bit,
                                          section);
    }
    
    void bloomseciton_test() {
        std::string pattern = "President Xi couple mask off, %1% but the entourage maks on. Anogher sign? ";
        BloomGenerator generator;
        for(auto i = 0; i < 10; i ++) {
            std::string data = (boost::format(pattern) % i).str();
            Bloom bloom;
            bloom.add(Slice(data.data(), data.size()));
            generator.addBloom(i, bloom.value());
        }
        
        for(auto i = 10; i < 4096; i ++) {
            std::string data = (boost::format(pattern) % (i+100)).str();
            Bloom bloom;
            bloom.add(Slice(data.data(), data.size()));
            generator.addBloom(i, bloom.value());
        }
        
        std::map<uint256, uint8_t*> kvTable;
        for (int i = 0; i < BLOOM_LENGTH; i++) {
            uint256 key = bloomBitsKey(i, 0);
            kvTable[key] = generator.bitSet(i);
        }
        
        auto matcher = [&](const int32_t i) -> bool {
            std::string queryStr = (boost::format(pattern) % i).str();
            Slice query(queryStr.data(), queryStr.size());
            Matcher::bloomIndexes indexes = Matcher::calcBloomIndexes(query);
            uint256 key1 = bloomBitsKey(indexes[0], 0);
            uint256 key2 = bloomBitsKey(indexes[1], 0);
            uint256 key3 = bloomBitsKey(indexes[2], 0);
            
            uint8_t* v1 = kvTable[key1];
            uint8_t* v2 = kvTable[key2];
            uint8_t* v3 = kvTable[key3];
            
            auto byteIndex = i / 8;
            auto bit = 7 - i%8;
            uint8_t byte1 = v1[byteIndex];
            bool ok1 = (byte1 &(1<<bit)) != 0;
            
            uint8_t byte2 = v2[byteIndex];
            bool ok2 = (byte2 &(1<<bit)) != 0;
            
            uint8_t byte3 = v3[byteIndex];
            bool ok3 = (byte3 &(1<<bit)) != 0;
            //BEAST_EXPECT(ok1 && ok2 && ok3);
            return ok1 && ok2 && ok3;
        };
        
        for(auto i = 0; i < 10; i ++) {
            BEAST_EXPECT(matcher(i));
        }
        
        for(auto i = 10; i < 110; i++) {
            BEAST_EXPECT(matcher(i) == false);
        }
        
        for(auto i = 10; i < 4096; i++) {
            BEAST_EXPECT(matcher(i + 100) == false);
        }
    }
    
    void run() override {
        bloomseciton_test();
    }
};

BEAST_DEFINE_TESTSUITE(BloomGenerator, bloom, ripple);

} // namespace test
} // namespace ripple


