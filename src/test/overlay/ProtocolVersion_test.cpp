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

#include <ripple/beast/unit_test.h>
#include <ripple/overlay/impl/ProtocolVersion.h>

namespace ripple {

class ProtocolVersion_test : public beast::unit_test::suite
{
private:
    template <class FwdIt>
    static std::string
    join(FwdIt first, FwdIt last, char const* sep = ",")
    {
        std::string result;
        if (first == last)
            return result;
        result = to_string(*first++);
        while (first != last)
            result += sep + to_string(*first++);
        return result;
    }

    void
    check(std::string const& s, std::string const& answer)
    {
        auto const result = parseProtocolVersions(s);
        BEAST_EXPECT(join(result.begin(), result.end()) == answer);
    }

public:
    void
    run() override
    {
        testcase("Convert protocol version to string");
        BEAST_EXPECT(to_string(make_protocol(1, 2)) == "RTXP/1.2");
        BEAST_EXPECT(to_string(make_protocol(1, 3)) == "ZXCL/1.3");
        BEAST_EXPECT(to_string(make_protocol(2, 0)) == "ZXCL/2.0");
        BEAST_EXPECT(to_string(make_protocol(2, 1)) == "ZXCL/2.1");
        BEAST_EXPECT(to_string(make_protocol(10, 10)) == "ZXCL/10.10");

        {
            testcase("Convert strings to protocol versions");

            // Empty string
            check("", "");
            check(
                "RTXP/1.1,RTXP/1.3,ZXCL/2.1,RTXP/1.2,ZXCL/2.0",
                "RTXP/1.2,ZXCL/2.0,ZXCL/2.1");
            check(
                "RTXP/0.9,RTXP/1.01,ZXCL/0.3,ZXCL/2.01,ZXCL/19.04,Oscar/"
                "123,NIKB",
                "");
            check(
                "RTXP/1.2,ZXCL/2.0,RTXP/1.2,ZXCL/2.0,ZXCL/19.4,ZXCL/7.89,ZXCL/"
                "A.1,ZXCL/2.01",
                "RTXP/1.2,ZXCL/2.0,ZXCL/7.89,ZXCL/19.4");
            check(
                "ZXCL/2.0,ZXCL/3.0,ZXCL/4,ZXCL/,ZXCL,OPT ZXCL/2.2,ZXCL/5.67",
                "ZXCL/2.0,ZXCL/3.0,ZXCL/5.67");
        }

        {
            testcase("Protocol version negotiation");

            BEAST_EXPECT(
                negotiateProtocolVersion("RTXP/1.2") == make_protocol(1, 2));
            BEAST_EXPECT(
                negotiateProtocolVersion("RTXP/1.2, ZXCL/2.0") ==
                make_protocol(2, 0));
            BEAST_EXPECT(
                negotiateProtocolVersion("ZXCL/2.0") == make_protocol(2, 0));
            BEAST_EXPECT(
                negotiateProtocolVersion("RTXP/1.2, ZXCL/2.0, ZXCL/999.999") ==
                make_protocol(2, 0));
            BEAST_EXPECT(
                negotiateProtocolVersion("ZXCL/999.999, WebSocket/1.0") ==
                boost::none);
            BEAST_EXPECT(negotiateProtocolVersion("") == boost::none);
        }
    }
};

BEAST_DEFINE_TESTSUITE(ProtocolVersion, overlay, ripple);

}  // namespace ripple
