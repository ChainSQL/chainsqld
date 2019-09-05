//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
	Copyright (c) 2019 Peersafe Technology Co., Ltd.

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

#include <BeastConfig.h>
#include <ripple/app/misc/TxQ.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/JsonFields.h>
#include <test/jtx.h>
#include <ripple/beast/unit_test.h>

namespace ripple {

class LedgerTx_test : public beast::unit_test::suite
{


    void testLedgerTxFull()
    {
        testcase("Ledger_Tx Request, Full Option");
        using namespace test::jtx;

        Env env {*this};

        env.close();

        Json::Value jvParams;
        jvParams[jss::ledger_index] = 3u;
        jvParams[jss::include_success] = true;
		jvParams[jss::include_failure] = true;
        auto const jrr = env.rpc ( "json", "ledger_txs", to_string(jvParams) ) [jss::result];

		BEAST_EXPECT(jrr.isMember(jss::ledger_index));
		BEAST_EXPECT(jrr.isMember(jss::txn_count));
		BEAST_EXPECT(jrr.isMember(jss::txn_success));
		BEAST_EXPECT(jrr.isMember(jss::txn_failure));
        BEAST_EXPECT(jrr.isMember(jss::txn_failure_detail));
		BEAST_EXPECT(jrr.isMember(jss::txn_success_detail));
        BEAST_EXPECT(jrr[jss::txn_failure_detail].isArray());

    }








public:
    void run ()
    {

		testLedgerTxFull();
    }
};

BEAST_DEFINE_TESTSUITE(LedgerTx,app,ripple);

} // ripple

