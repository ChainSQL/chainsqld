//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012-2016 Ripple Labs Inc->

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
#include <map>
#include <utility>
#include <sstream>
#include <iostream>
#include <memory>
#include <cstdlib>
#include <ctime>

#include <BeastConfig.h>
#include <peersafe/consensus/hotstuff/impl/Types.h>
#include <peersafe/consensus/hotstuff/impl/Block.h>

#include <ripple/protocol/SecretKey.h>
#include <ripple/protocol/PublicKey.h>
#include <ripple/beast/unit_test.h>

#include <test/app/SuitLogs.h>

namespace ripple {
namespace test {

class HotstuffCore_test : public beast::unit_test::suite {
public:
    void run() override {
		BEAST_EXPECT(false);
    }
};

BEAST_DEFINE_TESTSUITE(HotstuffCore, consensus, ripple);

} // namespace ripple
} // namespace test