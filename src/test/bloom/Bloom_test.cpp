//==============================================================================

#include <ripple/beast/unit_test.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/jss.h>

#include <peersafe/app/bloom/Bloom.h>

#include <test/jtx.h>

namespace ripple {
namespace test {
class Bloom_test : public beast::unit_test::suite
{
public:
    void
    bloom_test()
    {
        std::vector<std::string> positive =  {
            "testtest", "test", "hallo", "other"};
        std::vector<std::string> negative = {"tes", "lo"};

        Bloom b;
        for (auto data : positive)
        {
            b.add(Slice(data.data(),data.size()));
        }
        for (auto data : positive)
        {
            BEAST_EXPECT(b.test(Slice(data.data(), data.size())));
        }

        for (auto data : negative)
        {
            BEAST_EXPECT(!b.test(Slice(data.data(), data.size())));
        }
    }

    void
    bloom_hash_test()
    {
        uint256 exp = from_hex_text<uint256>("c8d3ca65cdb4874300a9e39475508f23ed6da09fdbc487f89a2dcf50b09eb263");
        Bloom b;
        for (int i=0; i<100; i++)
        {
            std::string data = (boost::format(
                "xxxxxxxxxx data %d yyyyyyyyyyyyyy") %i).str();
            b.add(Slice(data.data(), data.size()));
        }

        auto got = sha512Half<CommonKey::sha3>(Slice(b.value().data(),b.value().size()));
        BEAST_EXPECT(exp == got);
    }

    void
    run() override
    {
        bloom_test();
        bloom_hash_test();
    }
};

BEAST_DEFINE_TESTSUITE(Bloom, bloom, ripple);

}  // namespace test
}  // namespace ripple
