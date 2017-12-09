//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012, 2013 Ripple Labs Inc.
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
#include <test/jtx.h>
#include <ripple/app/paths/Flow.h>
#include <ripple/app/paths/impl/Steps.h>
#include <ripple/basics/contract.h>
#include <ripple/core/Config.h>
#include <ripple/ledger/PaymentSandbox.h>
#include <ripple/ledger/Sandbox.h>
#include <test/jtx/PathSet.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/JsonFields.h>
namespace ripple {
namespace test {

class SuiteSink : public beast::Journal::Sink
    {
        std::string partition_;
        beast::unit_test::suite& suite_;

    public:
        SuiteSink(std::string const& partition,
            beast::severities::Severity threshold,
            beast::unit_test::suite& suite)
            : Sink(threshold, false)
            , partition_(partition + " ")
            , suite_(suite)
        {
        }

        // For unit testing, always generate logging text.
        bool active(beast::severities::Severity level) const override
        {
            return true;
        }

        void
            write(beast::severities::Severity level,
                std::string const& text) override
        {
            using namespace beast::severities;
            std::string s;
            switch (level)
            {
            case kTrace:    s = "TRC:"; break;
            case kDebug:    s = "DBG:"; break;
            case kInfo:     s = "INF:"; break;
            case kWarning:  s = "WRN:"; break;
            case kError:    s = "ERR:"; break;
            default:
            case kFatal:    s = "FTL:"; break;
            }

            // Only write the string if the level at least equals the threshold.
            if (level >= threshold())
                suite_.log << s << partition_ << text << std::endl;
        }
    };

class SuiteLogs : public Logs
    {
        beast::unit_test::suite& suite_;

    public:
        explicit
            SuiteLogs(beast::unit_test::suite& suite)
            : Logs(beast::severities::kError)
            , suite_(suite)
        {
        }

        ~SuiteLogs() override = default;

        std::unique_ptr<beast::Journal::Sink>
            makeSink(std::string const& partition,
                beast::severities::Severity threshold) override
        {
            return std::make_unique<SuiteSink>(partition, threshold, suite_);
        }
    };
std::unique_ptr<ripple::Logs> logs_;

struct ChainSql_test : public beast::unit_test::suite
{
    void run() override
    {
        testcase("ChainSql test");
        using namespace jtx;
        auto const alice = Account("alice");

        Env env(*this, features(featureFlow), features(featureOwnerPaysFee));
        env.fund(ZXC(10000), alice);
        env(paytableset(alice));
        logs_ = std::make_unique<SuiteLogs>(*this);
        
        int errortimes = 0; 
        time_t start = clock();
        for (int i = 0; i < 1000; i++)
        {
            env(paysqlstatement(alice));
            if (env.ter() != tesSUCCESS)
            {
                printf("insert error\n");
                errortimes++;
            }      
        }
        time_t end = clock();
        printf("total running time is : %f\n", double(end - start) / CLOCKS_PER_SEC);
        printf("1000 times of insert errortimes is : %d\n", errortimes);
        JLOG(logs_->journal("ChainSql").trace()) << "1000 times of insert errortimes is"<< errortimes;
        JLOG(logs_->journal("ChainSql").trace()) << "total running time is" << double(end - start) / CLOCKS_PER_SEC;
    }
};

BEAST_DEFINE_TESTSUITE(ChainSql,app,ripple);

} // test
} // ripple
